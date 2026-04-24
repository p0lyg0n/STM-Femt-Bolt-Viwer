// Defines small application model types that are shared between runtime code
// and unit-testable helper modules.

#pragma once

#include "i18n.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Enumerates the point-cloud rendering modes shown in the viewer UI.
enum class PointRenderMode : uint8_t {
    GpuMesh = 0,
    GpuPoint = 1,
    CpuPoint = 2,
};

// Stores the per-session stream resolution and frame-rate settings.
struct StreamSettings {
    int depthW = 640;
    int depthH = 576;
    int colorW = 1280;
    int colorH = 720;
    int fps = 30;
};

// Describes how one camera is attached to the host USB topology.
struct UsbInfo {
    std::string controllerId;
    std::string controllerName;
    std::string rootHub;
    std::string portInfo;
};

// Captures the host USB controller ordering and per-device attachments.
struct SystemUsbTopology {
    std::vector<std::string> controllers;
    std::unordered_map<std::string, std::string> controllerNames;
    std::unordered_map<std::string, UsbInfo> deviceMap;
};

namespace app_settings {

// Persists the user-facing settings that survive across application restarts.
struct AppSettings {
    i18n::Lang lang = i18n::Lang::Japanese;
    PointRenderMode pointMode = PointRenderMode::GpuMesh;
    StreamSettings stream;
    bool showIr = false;
    bool vsync = true;
};

} // namespace app_settings
