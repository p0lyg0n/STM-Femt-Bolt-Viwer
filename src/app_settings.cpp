#include "app_settings.h"

#include <cstdlib>
#include <cstring>
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

void trimCrSpaces(std::string &s) {
    while(!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    size_t i = 0;
    while(i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    if(i > 0) s.erase(0, i);
}

PointRenderMode pointModeFromCode(const char *code) {
    if(!code) return PointRenderMode::GpuMesh;
    if(std::strcmp(code, "point")     == 0) return PointRenderMode::GpuPoint;
    if(std::strcmp(code, "cpu_point") == 0) return PointRenderMode::CpuPoint;
    return PointRenderMode::GpuMesh;
}

const char *pointModeCode(PointRenderMode m) {
    switch(m) {
        case PointRenderMode::GpuPoint: return "point";
        case PointRenderMode::CpuPoint: return "cpu_point";
        default:                        return "mesh";
    }
}

int parseInt(const std::string &s, int fallback) {
    try {
        return std::stoi(s);
    } catch(...) {
        return fallback;
    }
}

} // namespace

AppSettings load() {
    AppSettings out;
    std::ifstream f(settingsFilePath());
    if(!f.good()) return out;
    std::string line;
    while(std::getline(f, line)) {
        trimCrSpaces(line);
        if(line.empty() || line[0] == '#' || line[0] == ';') continue;
        const size_t eq = line.find('=');
        if(eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq);
        const std::string val = line.substr(eq + 1);

        if(key == "lang") {
            out.lang = i18n::langFromCode(val.c_str());
        } else if(key == "point_mode") {
            out.pointMode = pointModeFromCode(val.c_str());
        } else if(key == "depth_w") {
            out.stream.depthW = parseInt(val, out.stream.depthW);
        } else if(key == "depth_h") {
            out.stream.depthH = parseInt(val, out.stream.depthH);
        } else if(key == "color_w") {
            out.stream.colorW = parseInt(val, out.stream.colorW);
        } else if(key == "color_h") {
            out.stream.colorH = parseInt(val, out.stream.colorH);
        } else if(key == "fps") {
            out.stream.fps = parseInt(val, out.stream.fps);
        } else if(key == "show_ir") {
            out.showIr = (val == "1" || val == "true" || val == "yes");
        }
    }
    return out;
}

void save(const AppSettings &s) {
    std::ofstream f(settingsFilePath(), std::ios::trunc);
    if(!f.good()) return;
    f << "lang="        << i18n::langCode(s.lang)        << "\n";
    f << "point_mode="  << pointModeCode(s.pointMode)    << "\n";
    f << "depth_w="     << s.stream.depthW               << "\n";
    f << "depth_h="     << s.stream.depthH               << "\n";
    f << "color_w="     << s.stream.colorW               << "\n";
    f << "color_h="     << s.stream.colorH               << "\n";
    f << "fps="         << s.stream.fps                  << "\n";
    f << "show_ir="     << (s.showIr ? "1" : "0")        << "\n";
}

} // namespace app_settings
