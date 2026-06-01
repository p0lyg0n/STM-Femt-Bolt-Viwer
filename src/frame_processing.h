#pragma once

#include "types.h"

// ---------------------------------------------------------------------------
// Frame conversion
// ---------------------------------------------------------------------------

bool convertColorFrameToRgb(const std::shared_ptr<ob::VideoFrame> &frame, std::vector<uint8_t> &out, int &w, int &h);
bool convertDepthFrameToPseudoRgb(const std::shared_ptr<ob::DepthFrame> &frame, std::vector<uint8_t> &out, int &w, int &h);
bool convertIrFrameToGrayscaleRgb(const std::shared_ptr<ob::VideoFrame> &frame, std::vector<uint8_t> &out, int &w, int &h);

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

// ---------------------------------------------------------------------------
// Floor auto-leveling
// ---------------------------------------------------------------------------
// All matrices below are column-major 4x4 (glMultMatrixf-ready) and operate on
// render-frame points (+X right, +Y up, +Z toward viewer — the (x,-y,-z) cloud
// stored in GpuMesh). They rotate the cloud so the floor / gravity is vertical.

// Build a rotation that brings the (render-frame) up direction onto +Y. The
// +Y-facing half of the gravity/normal line is chosen, so the sign convention
// of the input does not matter. Writes identity if already level.
void levelMatrixFromUp(float ux, float uy, float uz, float outMat16[16]);

// RANSAC-fit the dominant near-horizontal plane in a render-frame point cloud
// and build the matrix that stands it level. Returns false if no usable
// (roughly horizontal) plane was found.
bool computeFloorLevelMatrix(const GpuMesh &mesh, float outMat16[16]);

// Build the level matrix from a raw accelerometer reading (m/s^2). accelToOptRot
// is the row-major 3x3 rotation that maps the IMU frame into the camera optical
// frame (+X right, +Y down, +Z forward); pass identity if unknown. At rest the
// reading lies along gravity. Returns false if the vector is too small
// (free-fall / no data).
bool computeImuLevelMatrix(float ax, float ay, float az, const float accelToOptRot[9], float outMat16[16]);

// Given the level ROTATION in rotMat16, find the visible floor (lowest dense Y
// band after rotation) and return the translation (tx,ty,tz) that places the
// floor's centroid at the grid origin (0, kGridY, 0) — i.e. centers the cloud on
// the grid AND drops its floor onto it. Returns false if no floor band could be
// estimated. The caller bakes (tx,ty,tz) into the matrix's translation column.
bool estimateFloorSnap(const float rotMat16[16], const GpuMesh &mesh,
                       float &txOut, float &tyOut, float &tzOut);
