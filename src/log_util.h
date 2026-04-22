#pragma once

// ANSI-color console helpers. On Windows we enable virtual terminal processing
// once at startup so std::cout / std::cerr interpret the escape sequences
// below as colors instead of printing them literally.
namespace logc {

// Reset / attributes
constexpr const char *reset = "\033[0m";
constexpr const char *bold  = "\033[1m";
constexpr const char *dim   = "\033[2m";

// Foreground colors
constexpr const char *red     = "\033[31m";
constexpr const char *green   = "\033[32m";
constexpr const char *yellow  = "\033[33m";
constexpr const char *blue    = "\033[34m";
constexpr const char *magenta = "\033[35m";
constexpr const char *cyan    = "\033[36m";
constexpr const char *white   = "\033[97m";
constexpr const char *gray    = "\033[90m";
constexpr const char *brightYellow = "\033[93m";
constexpr const char *brightCyan   = "\033[96m";
constexpr const char *brightGreen  = "\033[92m";
constexpr const char *brightRed    = "\033[91m";

// Must be called once at program start so Windows consoles render colors
// rather than emit the raw escape codes as text. No-op on non-Windows.
void enableVirtualTerminal();

} // namespace logc
