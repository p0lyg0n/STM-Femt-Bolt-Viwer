#include "camera_session.h"
#include "log_util.h"

#include <algorithm>
#include <chrono>
#include <thread>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Device list / info compatibility shims (SDK API version differences)
// ---------------------------------------------------------------------------

template <typename T>
auto deviceListCountImpl(const T &deviceList, int) -> decltype(deviceList->getCount()) {
    return deviceList->getCount();
}

template <typename T>
auto deviceListCountImpl(const T &deviceList, long) -> decltype(deviceList->deviceCount()) {
    return deviceList->deviceCount();
}

template <typename T>
uint32_t deviceListCount(const T &deviceList) {
    return deviceListCountImpl(deviceList, 0);
}

template <typename T>
auto deviceNameImpl(const T &deviceInfo, int) -> decltype(deviceInfo->getName()) {
    return deviceInfo->getName();
}

template <typename T>
auto deviceNameImpl(const T &deviceInfo, long) -> decltype(deviceInfo->name()) {
    return deviceInfo->name();
}

template <typename T>
auto deviceSerialImpl(const T &deviceInfo, int) -> decltype(deviceInfo->getSerialNumber()) {
    return deviceInfo->getSerialNumber();
}

template <typename T>
auto deviceSerialImpl(const T &deviceInfo, long) -> decltype(deviceInfo->serialNumber()) {
    return deviceInfo->serialNumber();
}

template <typename T>
auto deviceConnectionImpl(const T &deviceInfo, int) -> decltype(deviceInfo->getConnectionType()) {
    return deviceInfo->getConnectionType();
}

template <typename T>
auto deviceConnectionImpl(const T &deviceInfo, long) -> decltype(deviceInfo->connectionType()) {
    return deviceInfo->connectionType();
}

template <typename T>
auto deviceNameText(const T &deviceInfo) -> decltype(deviceNameImpl(deviceInfo, 0)) {
    return deviceNameImpl(deviceInfo, 0);
}

template <typename T>
auto deviceSerialText(const T &deviceInfo) -> decltype(deviceSerialImpl(deviceInfo, 0)) {
    return deviceSerialImpl(deviceInfo, 0);
}

template <typename T>
auto deviceConnectionText(const T &deviceInfo) -> decltype(deviceConnectionImpl(deviceInfo, 0)) {
    return deviceConnectionImpl(deviceInfo, 0);
}

// ---------------------------------------------------------------------------
// Misc helpers
// ---------------------------------------------------------------------------

std::string toFormatText(OBFormat fmt) {
    switch(fmt) {
    case OB_FORMAT_RGB: return "RGB";
    case OB_FORMAT_BGR: return "BGR";
    case OB_FORMAT_Y16: return "Y16";
    case OB_FORMAT_Z16: return "Z16";
    case OB_FORMAT_MJPG: return "MJPG";
    case OB_FORMAT_YUYV: return "YUYV";
    default: return "FMT?";
    }
}

std::string currentTimestampText() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S");
    return oss.str();
}

std::shared_ptr<ob::FrameSet> takeLatestFrameSet(const std::shared_ptr<CameraSession> &session) {
    if(!session) return nullptr;
    std::lock_guard<std::mutex> guard(session->latestFrameMutex);
    auto frameSet = session->latestFrameSet;
    session->latestFrameSet.reset();
    return frameSet;
}

std::shared_ptr<CameraSession> createCameraSession(const std::shared_ptr<ob::Device> &device, int deviceIndex) {
    auto session = std::make_shared<CameraSession>();
    session->deviceIndex = deviceIndex;
    session->device = device;
    session->pipeline = std::make_shared<ob::Pipeline>(device);
    session->align = std::make_shared<ob::Align>(OB_STREAM_COLOR);

    if(auto info = device->getDeviceInfo()) {
        session->deviceName = deviceNameText(info) ? deviceNameText(info) : "";
        session->serialNumber = deviceSerialText(info) ? deviceSerialText(info) : "";
        session->connectionType = deviceConnectionText(info) ? deviceConnectionText(info) : "";
    }
    return session;
}

} // namespace

// ---------------------------------------------------------------------------
// Public functions
// ---------------------------------------------------------------------------

std::shared_ptr<ob::FrameSet> getAlignedFrameSet(
    const std::shared_ptr<ob::FrameSet> &rawFrameSet,
    const std::shared_ptr<ob::Align> &align) {
    if(!rawFrameSet) return nullptr;
    try {
        auto aligned = align ? align->process(rawFrameSet) : rawFrameSet;
        auto alignedFrameSet = aligned ? aligned->as<ob::FrameSet>() : nullptr;
        return alignedFrameSet ? alignedFrameSet : rawFrameSet;
    } catch(...) {
        return rawFrameSet;
    }
}

void tryFetchCameraParam(const std::shared_ptr<ob::Pipeline> &pipeline, bool &cameraParamReady, OBCameraParam &cameraParam) {
    if(cameraParamReady) return;
    try {
        cameraParam = pipeline->getCameraParam();
        cameraParamReady = true;
    } catch(...) {
        cameraParamReady = false;
    }
}

std::shared_ptr<ob::Config> createStreamConfig(const StreamSettings &settings) {
    auto config = std::make_shared<ob::Config>();
    config->enableVideoStream(OB_STREAM_DEPTH, settings.depthW, settings.depthH, settings.fps, OB_FORMAT_Y16);
    config->enableVideoStream(OB_STREAM_COLOR, settings.colorW, settings.colorH, settings.fps, OB_FORMAT_RGB);
    // IR shares the same CMOS and resolution as Depth on Femto Bolt, so enable
    // it alongside with matching parameters. The UI shows it on demand (click
    // the middle pane to toggle between Depth and IR). Kept always-on to keep
    // the toggle instant instead of requiring a pipeline restart.
    config->enableVideoStream(OB_STREAM_IR, settings.depthW, settings.depthH, settings.fps, OB_FORMAT_Y16);
    config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_FULL_FRAME_REQUIRE);
    return config;
}

void logSession(const std::shared_ptr<CameraSession> &session, const std::string &message) {
    if(!session) return;
    // Pick color by the message body so frequent status lines are easy to scan:
    // errors/timeouts in red, recovery/restart in yellow, success/started in green.
    auto containsAny = [&](std::initializer_list<const char *> needles) {
        for(const char *n : needles) if(message.find(n) != std::string::npos) return true;
        return false;
    };
    const char *levelTag   = logc::gray;
    const char *levelLabel = "[LOG ]";
    if(containsAny({"failed", "error", "timeout"})) {
        levelTag = logc::brightRed; levelLabel = "[ERR ]";
    } else if(containsAny({"restart", "reconnect", "recovery", "recover"})) {
        levelTag = logc::yellow; levelLabel = "[WARN]";
    } else if(containsAny({"started", "resumed", "recovered", "ok"})) {
        levelTag = logc::brightGreen; levelLabel = "[OK  ]";
    }
    std::cerr << levelTag << levelLabel << logc::reset
              << logc::dim << " [" << currentTimestampText() << "]" << logc::reset
              << logc::brightYellow << "[Device " << session->deviceIndex << "]" << logc::reset;
    if(!session->serialNumber.empty()) std::cerr << logc::dim << "[SN " << session->serialNumber << "]" << logc::reset;
    std::cerr << " " << message << std::endl;
}

void syncSessionDeviceInfo(const std::shared_ptr<CameraSession> &session, const std::shared_ptr<ob::Device> &device) {
    if(!session || !device) return;
    if(auto info = device->getDeviceInfo()) {
        session->deviceName = deviceNameText(info) ? deviceNameText(info) : "";
        session->serialNumber = deviceSerialText(info) ? deviceSerialText(info) : "";
        session->connectionType = deviceConnectionText(info) ? deviceConnectionText(info) : "";
    }
}

void attachSessionDevice(const std::shared_ptr<CameraSession> &session, const std::shared_ptr<ob::Device> &device) {
    if(!session || !device) return;
    try {
        if(session->pipeline) {
            session->pipeline->stop();
        }
    } catch(...) {
    }

    session->device = device;
    session->pipeline = std::make_shared<ob::Pipeline>(device);
    session->align = std::make_shared<ob::Align>(OB_STREAM_COLOR);
    session->cameraParamReady = false;
    session->healthy.store(false);
    session->reconnecting.store(false);
    session->disconnected.store(false);
    session->lastFrameReceived = std::chrono::steady_clock::now();
    session->lastRestartAttempt = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    syncSessionDeviceInfo(session, device);
}

void stopImuSensors(const std::shared_ptr<CameraSession> &session) {
    if(!session) return;
    if(session->accelSensor) {
        try { session->accelSensor->stop(); } catch(...) {}
        session->accelSensor.reset();
    }
    if(session->gyroSensor) {
        try { session->gyroSensor->stop(); } catch(...) {}
        session->gyroSensor.reset();
    }
}

void disconnectSession(const std::shared_ptr<CameraSession> &session, const std::string &reason) {
    if(!session) return;
    const bool wasDisconnected = session->disconnected.exchange(true);
    session->reconnecting.store(false);
    session->healthy.store(false);
    session->cameraParamReady = false;
    session->latestPoints = 0;
    session->viewState.mesh = GpuMesh{};
    session->rgb.clear();
    session->depthPseudo.clear();
    session->cpuPointPreview.clear();

    try {
        if(session->pipeline) {
            session->pipeline->stop();
        }
    } catch(...) {
    }
    stopImuSensors(session);
    session->device.reset();
    session->pipeline.reset();
    session->align.reset();

    {
        std::lock_guard<std::mutex> guard(session->latestFrameMutex);
        session->latestFrameSet.reset();
    }

    if(!wasDisconnected) {
        logSession(session, reason);
    }
}

void startImuSensors(const std::shared_ptr<CameraSession> &session) {
    if(!session || !session->device) return;
    try {
        auto sensor = session->device->getSensor(OB_SENSOR_ACCEL);
        if(sensor) {
            auto profiles = sensor->getStreamProfileList();
            auto profile = profiles->getAccelStreamProfile(OB_ACCEL_FS_4g, OB_SAMPLE_RATE_100_HZ);
            std::weak_ptr<CameraSession> weak = session;
            sensor->start(profile, [weak](std::shared_ptr<ob::Frame> frame) {
                auto locked = weak.lock();
                if(!locked) return;
                auto af = frame->as<ob::AccelFrame>();
                OBAccelValue v = af->value();
                std::lock_guard<std::mutex> g(locked->imuMutex);
                locked->lastAccel = v;
                locked->imuReady = true;
            });
            session->accelSensor = sensor;
        }
    } catch(...) {}
    try {
        auto sensor = session->device->getSensor(OB_SENSOR_GYRO);
        if(sensor) {
            auto profiles = sensor->getStreamProfileList();
            auto profile = profiles->getGyroStreamProfile(OB_GYRO_FS_250dps, OB_SAMPLE_RATE_100_HZ);
            std::weak_ptr<CameraSession> weak = session;
            sensor->start(profile, [weak](std::shared_ptr<ob::Frame> frame) {
                auto locked = weak.lock();
                if(!locked) return;
                auto gf = frame->as<ob::GyroFrame>();
                OBGyroValue v = gf->value();
                std::lock_guard<std::mutex> g(locked->imuMutex);
                locked->lastGyro = v;
            });
            session->gyroSensor = sensor;
        }
    } catch(...) {}
}

void pollDeviceTemperature(const std::shared_ptr<CameraSession> &session) {
    if(!session || !session->device || session->disconnected.load()) return;
    const auto now = std::chrono::steady_clock::now();
    if(std::chrono::duration_cast<std::chrono::seconds>(now - session->lastTempPoll).count() < 2) return;
    session->lastTempPoll = now;
    try {
        OBDeviceTemperature temp;
        uint32_t dataSize = sizeof(temp);
        session->device->getStructuredData(OB_STRUCT_DEVICE_TEMPERATURE, reinterpret_cast<uint8_t *>(&temp), &dataSize);
        std::lock_guard<std::mutex> g(session->tempMutex);
        session->cpuTemp = temp.cpuTemp;
        session->irTemp = temp.irTemp;
        session->ldmTemp = temp.ldmTemp;
        session->tempReady = true;
    } catch(...) {}
}

void startCameraSession(const std::shared_ptr<CameraSession> &session) {
    if(!session || !session->pipeline) return;
    auto config = createStreamConfig(session->streamSettings);
    session->pipeline->enableFrameSync();
    std::weak_ptr<CameraSession> weakSession = session;
    session->pipeline->start(config, [weakSession](std::shared_ptr<ob::FrameSet> frameSet) {
        if(auto locked = weakSession.lock()) {
            std::lock_guard<std::mutex> guard(locked->latestFrameMutex);
            locked->latestFrameSet = std::move(frameSet);
            locked->lastFrameReceived = std::chrono::steady_clock::now();
            const bool wasRecovering = locked->reconnecting.exchange(false);
            locked->healthy.store(true);
            if(wasRecovering) {
                logSession(locked, "recovered and resumed frame streaming");
            }
        }
    });
    session->healthy.store(false);
    logSession(session, "pipeline started");
    startImuSensors(session);
}

void restartCameraSession(const std::shared_ptr<CameraSession> &session, const char *reason) {
    if(!session || !session->pipeline) return;
    if(session->disconnected.load()) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if(now - session->lastRestartAttempt < std::chrono::seconds(5)) return;
    session->lastRestartAttempt = now;
    session->reconnecting.store(true);
    session->healthy.store(false);
    session->restartCount.fetch_add(1);

    std::ostringstream msg;
    msg << "restarting camera session";
    if(reason && *reason) msg << " (" << reason << ")";
    msg << ", restart count=" << session->restartCount.load();
    logSession(session, msg.str());

    try {
        session->pipeline->stop();
    } catch(...) {
    }

    {
        std::lock_guard<std::mutex> guard(session->latestFrameMutex);
        session->latestFrameSet.reset();
    }

    try {
        startCameraSession(session);
        session->lastFrameReceived = std::chrono::steady_clock::now();
    } catch(const std::exception &e) {
        logSession(session, std::string("restart failed: ") + e.what());
    } catch(...) {
        logSession(session, "restart failed: unknown error");
    }
}

void applyStreamSettingsToAllSessions(AppRuntime &runtime) {
    for(auto &session : runtime.sessions) {
        if(!session) continue;
        // Always propagate settings so the next startCameraSession (including
        // an auto-reconnect from a disconnected state) uses the new config.
        session->streamSettings = runtime.streamSettings;
        if(!session->pipeline) continue;

        // Stop IMU first to avoid callbacks firing on a torn-down pipeline.
        try { stopImuSensors(session); } catch(...) {}

        try {
            session->pipeline->stop();
        } catch(...) {
        }

        // Discard stale frame data and cached params — they were computed
        // with the previous depth/color resolution and can crash the Align
        // filter or the mesh builder if reused with the new stream sizes.
        {
            std::lock_guard<std::mutex> guard(session->latestFrameMutex);
            session->latestFrameSet.reset();
        }
        session->align = std::make_shared<ob::Align>(OB_STREAM_COLOR);
        session->cameraParamReady = false;
        session->cameraParam = {};
        session->viewState.mesh = GpuMesh{};
        session->rgb.clear();
        session->depthPseudo.clear();
        session->cpuPointPreview.clear();
        session->latestPoints = 0;
        session->viewState.colorW = 0;
        session->viewState.colorH = 0;
        session->viewState.depthW = 0;
        session->viewState.depthH = 0;

        // Small delay so the SDK can finish tearing down the previous
        // stream internally before we bring it back up with new config.
        std::this_thread::sleep_for(std::chrono::milliseconds(150));

        try {
            startCameraSession(session);
            session->lastFrameReceived = std::chrono::steady_clock::now();
            logSession(session, "pipeline restarted with new stream settings");
        } catch(const std::exception &e) {
            logSession(session, std::string("stream settings apply failed: ") + e.what());
        } catch(...) {
            logSession(session, "stream settings apply failed: unknown error");
        }
    }
}

uint32_t getDeviceListCount(const std::shared_ptr<ob::DeviceList> &deviceList) {
    if(!deviceList) return 0;
    return deviceListCount(deviceList);
}

std::shared_ptr<ob::Device> findDeviceBySerial(const std::shared_ptr<ob::DeviceList> &deviceList, const std::string &serialNumber) {
    if(!deviceList || serialNumber.empty()) return nullptr;
    const uint32_t count = deviceListCount(deviceList);
    for(uint32_t i = 0; i < count; ++i) {
        auto device = deviceList->getDevice(i);
        if(!device) continue;
        auto info = device->getDeviceInfo();
        if(!info) continue;
        const auto serial = deviceSerialText(info);
        if(serial && serial == serialNumber) {
            return device;
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<CameraSession>> createCameraSessionsFromDeviceList(const std::shared_ptr<ob::DeviceList> &deviceList, int maxSessions) {
    std::vector<std::shared_ptr<CameraSession>> sessions;
    if(!deviceList) return sessions;
    const int count = std::min<int>(static_cast<int>(deviceListCount(deviceList)), maxSessions);
    sessions.reserve(static_cast<size_t>(count));
    for(int i = 0; i < count; ++i) {
        auto device = deviceList->getDevice(static_cast<uint32_t>(i));
        sessions.push_back(createCameraSession(device, i));
    }
    return sessions;
}

void updateSessionFromFrames(const std::shared_ptr<CameraSession> &session) {
    if(!session) return;
    if(session->disconnected.load()) return;

    const auto now = std::chrono::steady_clock::now();
    // Give the USB topology worker (which polls device presence) a chance to
    // notice a physical unplug first — it handles the teardown safely on a
    // background thread. If after 8s nothing has happened, the device may
    // just be momentarily stuck; only then does the main thread attempt a
    // blocking pipeline restart.
    if(now - session->lastFrameReceived > std::chrono::seconds(8)) {
        try {
            restartCameraSession(session, "frame timeout");
        } catch(...) {
            logSession(session, "restart threw; will rely on USB worker for recovery");
        }
        return;
    }

    std::shared_ptr<ob::FrameSet> frameSet;
    std::shared_ptr<ob::FrameSet> alignedFrameset;
    try {
        frameSet = takeLatestFrameSet(session);
        if(!frameSet) return;
        alignedFrameset = getAlignedFrameSet(frameSet, session->align);
    } catch(...) {
        return;
    }
    if(!alignedFrameset) return;

    auto colorFrame = alignedFrameset->colorFrame();
    auto depthFrame = alignedFrameset->depthFrame();
    // IR frame lives on the raw frameset because Align(OB_STREAM_COLOR) does not
    // touch IR. Grabbing it from the raw pre-align frameset keeps the native
    // Depth-sensor resolution (matching Depth size).
    auto irFrame = frameSet->irFrame();

    tryFetchCameraParam(session->pipeline, session->cameraParamReady, session->cameraParam);

    if(colorFrame && convertColorFrameToRgb(colorFrame, session->rgb, session->viewState.colorW, session->viewState.colorH)) {
        uploadRgbTexture(session->texRgb, session->rgb, session->viewState.colorW, session->viewState.colorH);
        session->fpsColor.tick();
        session->viewState.colorFmt = toFormatText(colorFrame->format());
    }

    if(depthFrame && convertDepthFrameToPseudoRgb(depthFrame, session->depthPseudo, session->viewState.depthW, session->viewState.depthH)) {
        uploadRgbTexture(session->texDepth, session->depthPseudo, session->viewState.depthW, session->viewState.depthH);
        session->fpsDepth.tick();
        session->viewState.depthFmt = toFormatText(depthFrame->format());
    }

    if(irFrame && convertIrFrameToGrayscaleRgb(irFrame, session->irImage, session->viewState.irW, session->viewState.irH)) {
        uploadRgbTexture(session->texIr, session->irImage, session->viewState.irW, session->viewState.irH);
        session->fpsIr.tick();
        session->viewState.irFmt = toFormatText(irFrame->format());
    }

    if(colorFrame && depthFrame && session->cameraParamReady && !session->rgb.empty()) {
        if(rebuildMeshFromAlignedDepthColor(depthFrame, session->rgb, session->viewState.colorW, session->viewState.colorH, session->cameraParam, session->viewState.mesh)) {
            session->latestPoints = session->viewState.mesh.points;
            session->fpsPoint.tick();
        } else {
            session->latestPoints = 0;
        }
    }
}
