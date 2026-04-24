// Declares pure pixel-conversion helpers that can be tested without the SDK.

#pragma once

#include <cstdint>
#include <vector>

// Enumerates the raw frame encodings supported by the pure conversion helpers.
enum class RawFrameFormat : uint8_t {
    Unsupported = 0,
    Rgb,
    Bgr,
    Y8,
    Y16,
    Z16,
};

// Converts RGB or BGR pixels into packed RGB bytes.
bool convertColorPixelsToRgb(const uint8_t *src, RawFrameFormat format, int width, int height,
                             std::vector<uint8_t> &out);

// Converts depth pixels into the viewer's pseudo-color RGB preview.
bool convertDepthPixelsToPseudoRgb(const uint16_t *src, RawFrameFormat format, int width,
                                   int height, float scaleMm, std::vector<uint8_t> &out);

// Converts IR pixels into a grayscale RGB preview image.
bool convertIrPixelsToGrayscaleRgb(const void *src, RawFrameFormat format, int width, int height,
                                   std::vector<uint8_t> &out);
