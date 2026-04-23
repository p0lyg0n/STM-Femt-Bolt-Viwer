// Implements pure USB topology string-formatting helpers for the viewer UI.

#include "usb_topology_format.h"

#include <algorithm>
#include <cctype>
#include <string>

std::string normalizeUsbControllerName(std::string name) {
    auto replaceAll = [&name](const std::string &from, const std::string &replacement) {
        if(from.empty()) {
            return;
        }
        size_t pos = 0;
        while((pos = name.find(from, pos)) != std::string::npos) {
            name.replace(pos, from.size(), replacement);
            pos += replacement.size();
        }
    };

    replaceAll("eXtensible Host Controller", "xHCI");
    replaceAll(" - 1.10 (Microsoft)", "");

    auto trimLeft = [](std::string &value) {
        while(!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
            value.erase(value.begin());
        }
    };
    auto trimRight = [](std::string &value) {
        while(!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
            value.pop_back();
        }
    };
    auto collapseSpaces = [](std::string &value) {
        std::string out;
        out.reserve(value.size());
        bool previousWasSpace = false;
        for(const unsigned char currentChar : value) {
            const bool isSpace = std::isspace(currentChar) != 0;
            if(isSpace) {
                if(!previousWasSpace) {
                    out.push_back(' ');
                }
            } else {
                out.push_back(static_cast<char>(currentChar));
            }
            previousWasSpace = isSpace;
        }
        value.swap(out);
    };

    trimLeft(name);
    trimRight(name);
    collapseSpaces(name);
    return name;
}

std::string formatControllerDisplayName(const SystemUsbTopology &usbMap,
                                        const std::string &controllerId,
                                        const std::string &fallbackName) {
    if(controllerId.empty()) {
        return fallbackName.empty() ? "Unknown Controller" : fallbackName;
    }

    const auto controllerIt =
        std::find(usbMap.controllers.begin(), usbMap.controllers.end(), controllerId);
    const int controllerIndex =
        (controllerIt == usbMap.controllers.end())
            ? -1
            : static_cast<int>(std::distance(usbMap.controllers.begin(), controllerIt)) + 1;
    std::string displayName = fallbackName.empty() ? "Unknown Controller" : fallbackName;

    if(controllerIndex > 0) {
        return "#" + std::to_string(controllerIndex) + " " + displayName;
    }
    return displayName;
}
