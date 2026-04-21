#pragma once

#include "types.h"

// ---------------------------------------------------------------------------
// Frame conversion
// ---------------------------------------------------------------------------

bool convertColorFrameToRgb(const std::shared_ptr<ob::VideoFrame> &frame, std::vector<uint8_t> &out, int &w, int &h);
bool convertDepthFrameToPseudoRgb(const std::shared_ptr<ob::DepthFrame> &frame, std::vector<uint8_t> &out, int &w, int &h);

// ---------------------------------------------------------------------------
// Mesh / point cloud
// ---------------------------------------------------------------------------

bool rebuildMeshFromAlignedDepthColor(
    const std::shared_ptr<ob::DepthFrame> &depthFrame,
    const std::vector<uint8_t> &rgb,
    int colorW,
    int colorH,
    const OBCameraParam &cameraParam,
    GpuMesh &meshOut);

const char *toPointModeText(PointRenderMode mode);
