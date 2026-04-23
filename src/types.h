#pragma once

#include "app_model.h"

#include <libobsensor/ObSensor.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <GLFW/glfw3.h>
#include <imgui.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

constexpr int kInitWinW = 960;
constexpr int kInitWinH = 360;
constexpr const char *kWindowTitle = "STM2 Femto Bolt Viewer";
constexpr float kPanelAspectRatio = 16.0f / 9.0f;
constexpr double kPointPaneStartRatio = 2.0 / 3.0;
constexpr float kYawSensitivity = 0.35f;
constexpr float kPitchSensitivity = 0.25f;
constexpr float kPitchClampDeg = 89.0f;
constexpr float kZoomMin = 0.35f;
constexpr float kZoomMax = 4.0f;
constexpr float kZoomStepScale = 0.1f;
constexpr float kPanToWorldScale = 0.0008f;
constexpr float kBaseFovDeg = 55.0f;
constexpr float kNearClipZ = 0.05f;
constexpr float kFarClipZ = 30.0f;
constexpr float kCameraBaseOffsetZ = -1.2f;
constexpr float kPi = 3.1415926535f;
constexpr int kMeshSamplingStep = 6;
constexpr float kMeshMinDepthMeters = 0.12f;
constexpr float kMeshMaxDepthMeters = 12.0f;
constexpr int kCpuPreviewW = 640;
constexpr int kCpuPreviewH = 360;
constexpr int kCpuFallbackTargetPoints = 6000;
constexpr float kGridY = -0.55f;
constexpr float kGridHalfExtent = 1.2f;
constexpr float kGridStep = 0.1f;
constexpr float kAxisLength = 0.5f;
constexpr int kSidebarW = 360;
constexpr int kSidebarPad = 12;
constexpr int kSidebarSectionGap = 10;
constexpr int kSidebarHeaderH = 42;

// ---------------------------------------------------------------------------
// Timing constants for the connection-recovery state machine
// ---------------------------------------------------------------------------
// Main thread's silent-failure detection: if no frame arrives for this long
// the session is marked disconnected and the USB worker / hotplug callback
// take over recovery on their threads.
constexpr int kFrameTimeoutSec = 5;
// After a reattach, the USB topology worker waits this long for the first
// frame before classifying the attach as a silent failure.
constexpr int kPostAttachFrameWaitSec = 2;
// Short backoff used after an attach threw (device is present but SDK
// threw during pipeline start).
constexpr int kReattachExceptionBackoffMs = 1500;
// Longer backoff used after a silent failure (no frames flowed). Gives
// Windows / Orbbec time to settle before the next attempt.
constexpr int kSilentFailureBackoffSec = 10;
// Delay between pipeline tear-down and re-start when stream settings change
// (resolution / fps).
constexpr int kStreamRebuildDelayMs = 150;
// USB topology background poll cadence: total interval is kUsbTopologyPollSlice
// × kUsbTopologyPollSlices (= 500ms). Split so the worker can react to the
// shutdown flag quickly.
constexpr int kUsbTopologyPollSliceMs = 100;
constexpr int kUsbTopologyPollSlices = 5;
// Per-device header height above each session's image panes. Contains the
// device name row and a second row with USB / IMU / TEMP in three columns.
// Compact enough that 4 cameras fit as 1x4 vertical in a typical 1280x960 window.
constexpr int kSessionRowHeaderH = 82;
constexpr int kSessionRowPad = 12;
constexpr int kSessionPaneGap = 10;

// ---------------------------------------------------------------------------
// Structs / Enums
// ---------------------------------------------------------------------------

struct FpsMeter {
    std::chrono::steady_clock::time_point windowStart = std::chrono::steady_clock::now();
    int frameCount = 0;
    double fps = 0.0;
    void tick() {
        ++frameCount;
        const auto now = std::chrono::steady_clock::now();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - windowStart).count();
        if(ms >= 1000) {
            fps = frameCount * 1000.0 / static_cast<double>(ms);
            frameCount = 0;
            windowStart = now;
        }
    }
};

struct ViewerControl {
    float yawDeg = -8.0f;
    float pitchDeg = 5.0f;
    float zoom = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;
};

struct MouseControl {
    bool rotating = false;
    bool panning = false;
    double lastX = 0.0;
    double lastY = 0.0;
};

struct GpuMesh {
    std::vector<float> xyz;
    std::vector<uint8_t> rgb;
    std::vector<uint32_t> tris;
    int points = 0;
    bool hasData = false;
};

// Per-camera view state (renamed from AppState).
struct CameraViewState {
    ViewerControl view;
    MouseControl mouse;
    GpuMesh mesh;
    PointRenderMode pointMode = PointRenderMode::GpuMesh;
    bool wasMKeyDown = false;
    bool wasRKeyDown = false;
    int framebufferW = kInitWinW;
    int framebufferH = kInitWinH;
    int colorW = 0;
    int colorH = 0;
    int depthW = 0;
    int depthH = 0;
    int irW = 0;
    int irH = 0;
    std::string colorFmt = "-";
    std::string depthFmt = "-";
    std::string irFmt = "-";
};

struct CameraSession {
    int deviceIndex = 0;
    std::string deviceName;
    std::string serialNumber;
    std::string connectionType;
    std::shared_ptr<ob::Device> device;
    std::shared_ptr<ob::Pipeline> pipeline;
    std::shared_ptr<ob::Align> align;
    std::shared_ptr<ob::FrameSet> latestFrameSet;
    std::mutex latestFrameMutex;
    std::chrono::steady_clock::time_point lastFrameReceived = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point lastRestartAttempt = std::chrono::steady_clock::now() - std::chrono::seconds(10);
    std::atomic<bool> reconnecting{false};
    std::atomic<bool> disconnected{false};
    std::atomic<bool> healthy{false};
    std::atomic<uint32_t> restartCount{0};
    // True while attachSessionDevice + startCameraSession is in flight on
    // some worker thread. Femto Bolt's pipeline init after a hotplug event
    // can take 5-7 seconds (SDK internal device creation + depth engine
    // load), during which no frames arrive. The main thread's no-frame
    // timeout must NOT interpret this as a stuck session and trigger
    // another reattach — that race kills the pipeline right as it starts
    // streaming and crashes the SDK.
    std::atomic<bool> attachInProgress{false};

    CameraViewState viewState;
    StreamSettings streamSettings;
    // Serializes attach / disconnect / restart so that the hotplug callback
    // and the USB topology polling thread don't race each other on the same
    // session (double pipeline->stop, double attach, etc.).
    std::mutex lifecycleMutex;
    // After a failed reattach (typically because USB re-enumeration has not
    // finished yet), back off a bit before the next attempt so we don't
    // thrash the SDK with pipeline create/destroy cycles every 500ms.
    std::chrono::steady_clock::time_point reattachNotBefore = std::chrono::steady_clock::time_point::min();

    GLuint texRgb = 0;
    GLuint texDepth = 0;
    GLuint texIr = 0;
    GLuint texPointCpu = 0;
    std::vector<uint8_t> rgb;
    std::vector<uint8_t> depthPseudo;
    std::vector<uint8_t> irImage;
    std::vector<uint8_t> cpuPointPreview;
    OBCameraParam cameraParam = {};
    bool cameraParamReady = false;

    FpsMeter fpsColor;
    FpsMeter fpsDepth;
    FpsMeter fpsIr;
    FpsMeter fpsPoint;
    FpsMeter fpsLog;
    int latestPoints = 0;

    std::shared_ptr<ob::Sensor> accelSensor;
    std::shared_ptr<ob::Sensor> gyroSensor;
    std::mutex imuMutex;
    OBAccelValue lastAccel = {};
    OBGyroValue lastGyro = {};
    bool imuReady = false;

    std::mutex tempMutex;
    float cpuTemp = 0.0f;
    float irTemp = 0.0f;
    float ldmTemp = 0.0f;
    bool tempReady = false;
    std::chrono::steady_clock::time_point lastTempPoll = std::chrono::steady_clock::now() - std::chrono::seconds(10);
};

struct AppRuntime {
    std::vector<std::shared_ptr<CameraSession>> sessions;
    int framebufferW = kInitWinW;
    int framebufferH = kInitWinH;
    int activeSessionIndex = 0;
    mutable std::mutex usbTopologyMutex;
    SystemUsbTopology usbTopology;
    std::unordered_map<std::string, int> controllerUsage;
    std::atomic<bool> usbTopologyStop{false};
    std::thread usbTopologyThread;
    std::string glVendor;
    std::string glRenderer;
    std::string glVersion;
    ImFont* fontSmall  = nullptr;
    ImFont* fontNormal = nullptr;
    ImFont* fontLarge  = nullptr;
    StreamSettings streamSettings;
    // When true, the middle pane shows the IR (grayscale) image instead of Depth.
    // Toggled by clicking the middle pane.
    bool showIr = false;
    // When true, glfwSwapInterval(1) is active — UI is capped to the monitor
    // refresh rate. Off gives snappier mouse drag at the cost of occasional
    // tearing and a bit more GPU power.
    bool vsync  = true;
};

struct Viewport {
    int x = 0;
    int y = 0;
    int w = 1;
    int h = 1;
};
