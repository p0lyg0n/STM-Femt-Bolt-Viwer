// Implements pure settings.ini parsing and serialization helpers.

#include "settings_codec.h"

#include <cstring>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>

namespace {

void trimAsciiWhitespace(std::string &value) {
    while(!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ' ||
                             value.back() == '\t')) {
        value.pop_back();
    }
    size_t start = 0;
    while(start < value.size() && (value[start] == ' ' || value[start] == '\t')) {
        ++start;
    }
    if(start > 0) {
        value.erase(0, start);
    }
}

int parseInt(const std::string &value, int fallback) {
    try {
        return std::stoi(value);
    } catch(...) {
        return fallback;
    }
}

bool parseBool(const std::string &value) {
    return value == "1" || value == "true" || value == "yes";
}

} // namespace

namespace app_settings {

PointRenderMode pointModeFromCode(const char *code) {
    if(code == nullptr) {
        return PointRenderMode::GpuMesh;
    }
    if(std::strcmp(code, "point") == 0) {
        return PointRenderMode::GpuPoint;
    }
    if(std::strcmp(code, "cpu_point") == 0) {
        return PointRenderMode::CpuPoint;
    }
    return PointRenderMode::GpuMesh;
}

const char *pointModeCode(PointRenderMode mode) {
    switch(mode) {
    case PointRenderMode::GpuPoint:
        return "point";
    case PointRenderMode::CpuPoint:
        return "cpu_point";
    default:
        return "mesh";
    }
}

AppSettings loadFromStream(std::istream &input) {
    AppSettings out;
    std::string line;
    while(std::getline(input, line)) {
        trimAsciiWhitespace(line);
        if(line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        const size_t separator = line.find('=');
        if(separator == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, separator);
        std::string value = line.substr(separator + 1);
        trimAsciiWhitespace(key);
        trimAsciiWhitespace(value);

        if(key == "lang") {
            out.lang = i18n::langFromCode(value.c_str());
        } else if(key == "point_mode") {
            out.pointMode = pointModeFromCode(value.c_str());
        } else if(key == "depth_w") {
            out.stream.depthW = parseInt(value, out.stream.depthW);
        } else if(key == "depth_h") {
            out.stream.depthH = parseInt(value, out.stream.depthH);
        } else if(key == "color_w") {
            out.stream.colorW = parseInt(value, out.stream.colorW);
        } else if(key == "color_h") {
            out.stream.colorH = parseInt(value, out.stream.colorH);
        } else if(key == "fps") {
            out.stream.fps = parseInt(value, out.stream.fps);
        } else if(key == "show_ir") {
            out.showIr = parseBool(value);
        } else if(key == "vsync") {
            out.vsync = parseBool(value);
        }
    }
    return out;
}

void saveToStream(std::ostream &output, const AppSettings &settings) {
    output << "lang=" << i18n::langCode(settings.lang) << "\n";
    output << "point_mode=" << pointModeCode(settings.pointMode) << "\n";
    output << "depth_w=" << settings.stream.depthW << "\n";
    output << "depth_h=" << settings.stream.depthH << "\n";
    output << "color_w=" << settings.stream.colorW << "\n";
    output << "color_h=" << settings.stream.colorH << "\n";
    output << "fps=" << settings.stream.fps << "\n";
    output << "show_ir=" << (settings.showIr ? "1" : "0") << "\n";
    output << "vsync=" << (settings.vsync ? "1" : "0") << "\n";
}

std::string serialize(const AppSettings &settings) {
    std::ostringstream output;
    saveToStream(output, settings);
    return output.str();
}

} // namespace app_settings
