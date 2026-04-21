#include "frame_processing.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

bool convertColorFrameToRgb(const std::shared_ptr<ob::VideoFrame> &frame, std::vector<uint8_t> &out, int &w, int &h) {
    if(!frame) return false;
    w = frame->width();
    h = frame->height();
    if(w <= 0 || h <= 0) return false;

    const uint8_t *src = reinterpret_cast<const uint8_t *>(frame->data());
    if(!src) return false;
    out.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 3u);

    if(frame->format() == OB_FORMAT_RGB) {
        std::copy(src, src + out.size(), out.begin());
        return true;
    }
    if(frame->format() == OB_FORMAT_BGR) {
        for(size_t i = 0; i + 2 < out.size(); i += 3) {
            out[i + 0] = src[i + 2];
            out[i + 1] = src[i + 1];
            out[i + 2] = src[i + 0];
        }
        return true;
    }
    return false;
}

bool convertDepthFrameToPseudoRgb(const std::shared_ptr<ob::DepthFrame> &frame, std::vector<uint8_t> &out, int &w, int &h) {
    if(!frame) return false;
    if(frame->format() != OB_FORMAT_Y16 && frame->format() != OB_FORMAT_Z16) return false;
    w = frame->width();
    h = frame->height();
    if(w <= 0 || h <= 0) return false;

    const uint16_t *src = reinterpret_cast<const uint16_t *>(frame->data());
    if(!src) return false;
    const float scaleMm = frame->getValueScale() > 0.0f ? frame->getValueScale() : 1.0f;
    out.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 3u);

    for(int y = 0; y < h; ++y) {
        for(int x = 0; x < w; ++x) {
            const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
            const size_t rgb = idx * 3u;
            float zMm = src[idx] * scaleMm;
            if(zMm <= 0.0f) {
                out[rgb + 0] = 0;
                out[rgb + 1] = 0;
                out[rgb + 2] = 0;
                continue;
            }
            float t = std::clamp((zMm - kDepthPseudoMinMm) / kDepthPseudoRangeMm, 0.0f, 1.0f);
            const uint8_t r = static_cast<uint8_t>(255.0f * (1.0f - t));
            const uint8_t g = static_cast<uint8_t>(255.0f * std::abs(0.5f - t) * 2.0f);
            const uint8_t b = static_cast<uint8_t>(255.0f * t);
            out[rgb + 0] = r;
            out[rgb + 1] = g;
            out[rgb + 2] = b;
        }
    }
    return true;
}

bool convertIrFrameToGrayscaleRgb(const std::shared_ptr<ob::VideoFrame> &frame, std::vector<uint8_t> &out, int &w, int &h) {
    if(!frame) return false;
    w = frame->width();
    h = frame->height();
    if(w <= 0 || h <= 0) return false;

    // Femto Bolt IR is Y16 (16-bit grayscale). Map 0..4095 -> 0..255 with saturation;
    // typical reflected-IR signal at 0.5–3 m falls well inside that range and looks
    // close to a black-and-white IR photo. A fixed scale is preferred over auto-gain
    // to avoid frame-to-frame flicker.
    if(frame->format() == OB_FORMAT_Y16) {
        const uint16_t *src = reinterpret_cast<const uint16_t *>(frame->data());
        if(!src) return false;
        out.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 3u);
        const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
        for(size_t i = 0; i < n; ++i) {
            const uint32_t v = src[i] >> 4;
            const uint8_t g = static_cast<uint8_t>(v > 255 ? 255 : v);
            out[i * 3u + 0] = g;
            out[i * 3u + 1] = g;
            out[i * 3u + 2] = g;
        }
        return true;
    }
    if(frame->format() == OB_FORMAT_Y8) {
        const uint8_t *src = reinterpret_cast<const uint8_t *>(frame->data());
        if(!src) return false;
        out.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 3u);
        const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
        for(size_t i = 0; i < n; ++i) {
            const uint8_t g = src[i];
            out[i * 3u + 0] = g;
            out[i * 3u + 1] = g;
            out[i * 3u + 2] = g;
        }
        return true;
    }
    return false;
}

bool rebuildMeshFromAlignedDepthColor(
    const std::shared_ptr<ob::DepthFrame> &depthFrame,
    const std::vector<uint8_t> &rgb,
    int colorW,
    int colorH,
    const OBCameraParam &cameraParam,
    GpuMesh &meshOut) {
    meshOut = GpuMesh{};
    if(!depthFrame || rgb.empty()) return false;
    if(depthFrame->format() != OB_FORMAT_Y16 && depthFrame->format() != OB_FORMAT_Z16) return false;
    const int w = depthFrame->width();
    const int h = depthFrame->height();
    if(w <= 1 || h <= 1 || colorW != w || colorH != h) return false;

    const uint16_t *depthRaw = reinterpret_cast<const uint16_t *>(depthFrame->data());
    const float depthScaleMm = depthFrame->getValueScale() > 0.0f ? depthFrame->getValueScale() : 1.0f;
    const float fx = std::fabs(cameraParam.depthIntrinsic.fx) > 1e-6f ? cameraParam.depthIntrinsic.fx : static_cast<float>(w);
    const float fy = std::fabs(cameraParam.depthIntrinsic.fy) > 1e-6f ? cameraParam.depthIntrinsic.fy : static_cast<float>(h);
    const float cx = std::fabs(cameraParam.depthIntrinsic.cx) > 1e-6f ? cameraParam.depthIntrinsic.cx : (w * 0.5f);
    const float cy = std::fabs(cameraParam.depthIntrinsic.cy) > 1e-6f ? cameraParam.depthIntrinsic.cy : (h * 0.5f);

    const int step = kMeshSamplingStep;
    const int cols = (w + step - 1) / step;
    const int rows = (h + step - 1) / step;
    std::vector<int32_t> gridToVertex(static_cast<size_t>(rows * cols), -1);

    meshOut.xyz.reserve(static_cast<size_t>(rows * cols) * 3u);
    meshOut.rgb.reserve(static_cast<size_t>(rows * cols) * 3u);

    for(int gy = 0, py = 0; py < h; py += step, ++gy) {
        for(int gx = 0, px = 0; px < w; px += step, ++gx) {
            const size_t didx = static_cast<size_t>(py) * static_cast<size_t>(w) + static_cast<size_t>(px);
            const uint16_t d = depthRaw[didx];
            if(d == 0) continue;
            const float z = d * depthScaleMm * 0.001f;
            if(!std::isfinite(z) || z <= kMeshMinDepthMeters || z > kMeshMaxDepthMeters) continue;
            const float x = (px - cx) * z / fx;
            const float y = (py - cy) * z / fy;
            if(!std::isfinite(x) || !std::isfinite(y)) continue;

            meshOut.xyz.push_back(x);
            meshOut.xyz.push_back(-y);
            meshOut.xyz.push_back(-z);

            const size_t cidx = didx * 3u;
            meshOut.rgb.push_back(rgb[cidx + 0]);
            meshOut.rgb.push_back(rgb[cidx + 1]);
            meshOut.rgb.push_back(rgb[cidx + 2]);

            gridToVertex[static_cast<size_t>(gy * cols + gx)] = static_cast<int32_t>(meshOut.xyz.size() / 3u - 1u);
        }
    }

    meshOut.points = static_cast<int>(meshOut.xyz.size() / 3u);
    if(meshOut.points <= 0) return false;

    auto zAt = [&](int idx) -> float { return -meshOut.xyz[static_cast<size_t>(idx) * 3u + 2u]; };
    auto gapOk = [&](int a, int b) -> bool {
        const float za = zAt(a), zb = zAt(b);
        return std::fabs(za - zb) < (0.06f + 0.18f * std::max(za, zb));
    };

    for(int gy = 0; gy + 1 < rows; ++gy) {
        for(int gx = 0; gx + 1 < cols; ++gx) {
            const int i00 = gridToVertex[static_cast<size_t>(gy * cols + gx)];
            const int i10 = gridToVertex[static_cast<size_t>(gy * cols + gx + 1)];
            const int i01 = gridToVertex[static_cast<size_t>((gy + 1) * cols + gx)];
            const int i11 = gridToVertex[static_cast<size_t>((gy + 1) * cols + gx + 1)];
            if(i00 >= 0 && i10 >= 0 && i01 >= 0 && gapOk(i00, i10) && gapOk(i00, i01) && gapOk(i10, i01)) {
                meshOut.tris.push_back(static_cast<uint32_t>(i00));
                meshOut.tris.push_back(static_cast<uint32_t>(i10));
                meshOut.tris.push_back(static_cast<uint32_t>(i01));
            }
            if(i11 >= 0 && i10 >= 0 && i01 >= 0 && gapOk(i11, i10) && gapOk(i11, i01) && gapOk(i10, i01)) {
                meshOut.tris.push_back(static_cast<uint32_t>(i11));
                meshOut.tris.push_back(static_cast<uint32_t>(i01));
                meshOut.tris.push_back(static_cast<uint32_t>(i10));
            }
        }
    }

    meshOut.hasData = true;
    return true;
}

const char *toPointModeText(PointRenderMode mode) {
    switch(mode) {
    case PointRenderMode::GpuMesh: return "GPU MESH";
    case PointRenderMode::GpuPoint: return "GPU POINT";
    case PointRenderMode::CpuPoint: return "CPU POINT";
    default: return "UNKNOWN";
    }
}
