#include "app_settings.h"

#include "settings_codec.h"

#include <fstream>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace app_settings {

namespace {

std::string settingsFilePath() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if(n == 0 || n >= MAX_PATH) return "settings.ini";
    std::string p(buf, n);
    const size_t sep = p.find_last_of("\\/");
    if(sep == std::string::npos) return "settings.ini";
    return p.substr(0, sep + 1) + "settings.ini";
#else
    return "settings.ini";
#endif
}

} // namespace

AppSettings load() {
    std::ifstream f(settingsFilePath());
    if(!f.good()) return AppSettings{};
    return loadFromStream(f);
}

void save(const AppSettings &s) {
    std::ofstream f(settingsFilePath(), std::ios::trunc);
    if(!f.good()) return;
    saveToStream(f, s);
}

} // namespace app_settings
