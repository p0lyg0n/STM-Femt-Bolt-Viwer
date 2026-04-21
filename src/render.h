#pragma once

#include "types.h"
#include "gl_utils.h"
#include "usb_topology.h"

#include <imgui.h>
#include <unordered_map>
#include <string>

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void renderSidebar(AppRuntime &runtime);

void renderSessionSlot(
    const std::shared_ptr<CameraSession> &session,
    const AppRuntime &runtime,
    size_t sessionIndex,
    const SystemUsbTopology &usbTopology,
    const std::unordered_map<std::string, int> &controllerUsage);
