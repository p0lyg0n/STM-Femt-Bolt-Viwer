#pragma once

#include "i18n.h"
#include "types.h"

// Persists user-facing preferences (language, display mode, stream presets)
// across runs to a settings.ini file sitting next to the exe.
// Called once at startup (load) and once on shutdown (save).
namespace app_settings {

struct AppSettings {
    i18n::Lang       lang      = i18n::Lang::Japanese;
    PointRenderMode  pointMode = PointRenderMode::GpuMesh;
    StreamSettings   stream;
    bool             showIr    = false;
    bool             vsync     = true;
};

// Reads settings.ini next to the exe. Missing file / keys fall back to defaults.
AppSettings load();

// Overwrites settings.ini next to the exe.
void save(const AppSettings &s);

} // namespace app_settings
