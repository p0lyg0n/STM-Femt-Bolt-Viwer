// Declares pure helpers for formatting USB controller labels in the UI.

#pragma once

#include "app_model.h"

#include <string>

// Normalizes a raw USB controller name into the shortened UI label.
std::string normalizeUsbControllerName(std::string name);

// Formats a numbered controller display name for the sidebar and device rows.
std::string formatControllerDisplayName(const SystemUsbTopology &usbMap,
                                        const std::string &controllerId,
                                        const std::string &fallbackName);
