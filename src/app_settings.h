#pragma once

#include "app_model.h"

namespace app_settings {

// Reads settings.ini next to the exe. Missing file / keys fall back to defaults.
AppSettings load();

// Overwrites settings.ini next to the exe.
void save(const AppSettings &s);

} // namespace app_settings
