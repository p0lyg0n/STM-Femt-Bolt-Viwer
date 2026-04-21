#pragma once

#include "types.h"
#include "frame_processing.h"
#include "gl_utils.h"

// ---------------------------------------------------------------------------
// Stream config
// ---------------------------------------------------------------------------

std::shared_ptr<ob::Config> createStreamConfig(const StreamSettings &settings);

// Stop + restart every session's pipeline with the settings stored in
// runtime.streamSettings. Safe to call from UI thread. Each session's
// streamSettings is updated in-place; if start fails, that session is
// left reconnecting (same as a normal disconnect).
void applyStreamSettingsToAllSessions(AppRuntime &runtime);

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------

void logSession(const std::shared_ptr<CameraSession> &session, const std::string &message);

// ---------------------------------------------------------------------------
// Device info sync
// ---------------------------------------------------------------------------

void syncSessionDeviceInfo(const std::shared_ptr<CameraSession> &session, const std::shared_ptr<ob::Device> &device);

// ---------------------------------------------------------------------------
// Session lifecycle
// ---------------------------------------------------------------------------

// Attach a newly found device to an existing session (used during reconnect).
void attachSessionDevice(const std::shared_ptr<CameraSession> &session, const std::shared_ptr<ob::Device> &device);

// Mark session as disconnected and free device resources (renamed from markSessionDisconnected).
void disconnectSession(const std::shared_ptr<CameraSession> &session, const std::string &reason);

// ---------------------------------------------------------------------------
// IMU sensors
// ---------------------------------------------------------------------------

void stopImuSensors(const std::shared_ptr<CameraSession> &session);
void startImuSensors(const std::shared_ptr<CameraSession> &session);

// ---------------------------------------------------------------------------
// Temperature polling
// ---------------------------------------------------------------------------

void pollDeviceTemperature(const std::shared_ptr<CameraSession> &session);

// ---------------------------------------------------------------------------
// Pipeline control
// ---------------------------------------------------------------------------

void startCameraSession(const std::shared_ptr<CameraSession> &session);
void restartCameraSession(const std::shared_ptr<CameraSession> &session, const char *reason);

// ---------------------------------------------------------------------------
// Session creation
// ---------------------------------------------------------------------------

std::vector<std::shared_ptr<CameraSession>> createCameraSessionsFromDeviceList(
    const std::shared_ptr<ob::DeviceList> &deviceList, int maxSessions = 4);

// ---------------------------------------------------------------------------
// Device list helpers (needed by usb_topology and main)
// ---------------------------------------------------------------------------

// Returns the number of devices in a DeviceList, compatible across SDK API versions.
uint32_t getDeviceListCount(const std::shared_ptr<ob::DeviceList> &deviceList);

std::shared_ptr<ob::Device> findDeviceBySerial(const std::shared_ptr<ob::DeviceList> &deviceList, const std::string &serialNumber);

// ---------------------------------------------------------------------------
// Frame alignment helper
// ---------------------------------------------------------------------------

std::shared_ptr<ob::FrameSet> getAlignedFrameSet(
    const std::shared_ptr<ob::FrameSet> &rawFrameSet,
    const std::shared_ptr<ob::Align> &align);

void tryFetchCameraParam(const std::shared_ptr<ob::Pipeline> &pipeline, bool &cameraParamReady, OBCameraParam &cameraParam);

// ---------------------------------------------------------------------------
// Per-frame update (calls frame_processing functions)
// ---------------------------------------------------------------------------

void updateSessionFromFrames(const std::shared_ptr<CameraSession> &session);
