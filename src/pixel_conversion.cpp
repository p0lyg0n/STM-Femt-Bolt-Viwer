// Implements pure frame-to-preview conversion helpers used by tests and runtime.

#include "pixel_conversion.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace {

constexpr float kDepthPseudoMinMm = 250.0f;
constexpr float kDepthPseudoRangeMm = 4750.0f;
constexpr size_t kRgbChannelCount = 3U;

bool hasValidDimensions(int width, int height) {
    return width > 0 && height > 0;
}

size_t rgbByteCount(int width, int height) {
    return static_cast<size_t>(width) * static_cast<size_t>(height) * kRgbChannelCount;
}

} // namespace

bool convertColorPixelsToRgb(const uint8_t *src, RawFrameFormat format, int width, int height,
                             std::vector<uint8_t> &out) {
    if(src == nullptr || !hasValidDimensions(width, height)) {
        return false;
    }
    if(format != RawFrameFormat::Rgb && format != RawFrameFormat::Bgr) {
        return false;
    }

    out.resize(rgbByteCount(width, height));
    if(format == RawFrameFormat::Rgb) {
        std::copy(src, src + out.size(), out.begin());
        return true;
    }
    if(format == RawFrameFormat::Bgr) {
        for(size_t i = 0; i + 2 < out.size(); i += 3) {
            out[i + 0] = src[i + 2];
            out[i + 1] = src[i + 1];
            out[i + 2] = src[i + 0];
        }
        return true;
    }
    return false;
}

bool convertDepthPixelsToPseudoRgb(const uint16_t *src, RawFrameFormat format, int width,
                                   int height, float scaleMm, std::vector<uint8_t> &out) {
    if(src == nullptr || !hasValidDimensions(width, height)) {
        return false;
    }
    if(format != RawFrameFormat::Y16 && format != RawFrameFormat::Z16) {
        return false;
    }

    const float resolvedScaleMm = scaleMm > 0.0f ? scaleMm : 1.0f;
    out.resize(rgbByteCount(width, height));

    for(int row = 0; row < height; ++row) {
        for(int column = 0; column < width; ++column) {
            const size_t pixelIndex = (static_cast<size_t>(row) * static_cast<size_t>(width)) +
                                      static_cast<size_t>(column);
            const size_t rgbIndex = pixelIndex * kRgbChannelCount;
            const float depthMm = static_cast<float>(src[pixelIndex]) * resolvedScaleMm;

            if(depthMm <= 0.0f) {
                out[rgbIndex + 0] = 0;
                out[rgbIndex + 1] = 0;
                out[rgbIndex + 2] = 0;
                continue;
            }

            const float normalizedDepth =
                std::clamp((depthMm - kDepthPseudoMinMm) / kDepthPseudoRangeMm, 0.0f, 1.0f);
            out[rgbIndex + 0] = static_cast<uint8_t>(255.0f * (1.0f - normalizedDepth));
            out[rgbIndex + 1] =
                static_cast<uint8_t>(255.0f * std::abs(0.5f - normalizedDepth) * 2.0f);
            out[rgbIndex + 2] = static_cast<uint8_t>(255.0f * normalizedDepth);
        }
    }

    return true;
}

bool convertIrPixelsToGrayscaleRgb(const void *src, RawFrameFormat format, int width, int height,
                                   std::vector<uint8_t> &out) {
    if(src == nullptr || !hasValidDimensions(width, height)) {
        return false;
    }
    if(format != RawFrameFormat::Y16 && format != RawFrameFormat::Y8) {
        return false;
    }

    out.resize(rgbByteCount(width, height));
    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);

    if(format == RawFrameFormat::Y16) {
        const auto *pixels = reinterpret_cast<const uint16_t *>(src);
        for(size_t i = 0; i < pixelCount; ++i) {
            const uint32_t shifted = pixels[i] >> 4;
            const uint8_t gray = static_cast<uint8_t>(shifted > 255 ? 255 : shifted);
            out[(i * kRgbChannelCount) + 0] = gray;
            out[(i * kRgbChannelCount) + 1] = gray;
            out[(i * kRgbChannelCount) + 2] = gray;
        }
        return true;
    }

    if(format == RawFrameFormat::Y8) {
        const auto *pixels = reinterpret_cast<const uint8_t *>(src);
        for(size_t i = 0; i < pixelCount; ++i) {
            const uint8_t gray = pixels[i];
            out[(i * kRgbChannelCount) + 0] = gray;
            out[(i * kRgbChannelCount) + 1] = gray;
            out[(i * kRgbChannelCount) + 2] = gray;
        }
        return true;
    }

    return false;
}
