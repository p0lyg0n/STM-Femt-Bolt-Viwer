// Declares pure helpers for serializing and parsing settings.ini content.

#pragma once

#include "app_model.h"

#include <iosfwd>
#include <string>

namespace app_settings {

// Converts a persisted point-mode code into the runtime enum.
PointRenderMode pointModeFromCode(const char *code);

// Converts a runtime point-mode enum into its persisted string code.
const char *pointModeCode(PointRenderMode mode);

// Parses settings.ini content from a stream into application settings.
AppSettings loadFromStream(std::istream &input);

// Writes application settings to a stream using the settings.ini layout.
void saveToStream(std::ostream &output, const AppSettings &settings);

// Serializes application settings into one settings.ini formatted string.
std::string serialize(const AppSettings &settings);

} // namespace app_settings
