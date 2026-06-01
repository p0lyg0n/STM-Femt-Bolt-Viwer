#include "frame_processing.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
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
    // The depth here is aligned to COLOR (ob::Align(OB_STREAM_COLOR)) so it lives
    // in the color image grid at color resolution (e.g. 1280x720). Deproject with
    // rgbIntrinsic, NOT depthIntrinsic (the native 640x576 model, fx~504/cx~334),
    // which would scale/skew the cloud (wrong FOV + ~300px principal-point shift).
    const OBCameraIntrinsic &intr = cameraParam.rgbIntrinsic;
    const float fx = std::fabs(intr.fx) > 1e-6f ? intr.fx : static_cast<float>(w);
    const float fy = std::fabs(intr.fy) > 1e-6f ? intr.fy : static_cast<float>(h);
    const float cx = std::fabs(intr.cx) > 1e-6f ? intr.cx : (w * 0.5f);
    const float cy = std::fabs(intr.cy) > 1e-6f ? intr.cy : (h * 0.5f);

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

// ---------------------------------------------------------------------------
// Floor auto-leveling
// ---------------------------------------------------------------------------

namespace {

void setIdentity16(float m[16]) {
    for(int i = 0; i < 16; ++i) m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

// Rodrigues rotation about a (possibly non-unit) axis by `angle` radians,
// stored column-major (out[col*4 + row]) so it feeds straight into glMultMatrixf.
void axisAngleToMat16(float ax, float ay, float az, float angle, float out[16]) {
    setIdentity16(out);
    const float len = std::sqrt(ax*ax + ay*ay + az*az);
    if(len < 1e-9f) return;
    ax /= len; ay /= len; az /= len;
    const float c = std::cos(angle), s = std::sin(angle), t = 1.0f - c;
    out[0] = t*ax*ax + c;     out[4] = t*ax*ay - s*az;  out[8]  = t*ax*az + s*ay;
    out[1] = t*ax*ay + s*az;  out[5] = t*ay*ay + c;     out[9]  = t*ay*az - s*ax;
    out[2] = t*ax*az - s*ay;  out[6] = t*ay*az + s*ax;  out[10] = t*az*az + c;
}

} // namespace

void levelMatrixFromUp(float ux, float uy, float uz, float outMat16[16]) {
    setIdentity16(outMat16);
    const float n = std::sqrt(ux*ux + uy*uy + uz*uz);
    if(n < 1e-6f) return;
    ux /= n; uy /= n; uz /= n;
    if(uy < 0.0f) { ux = -ux; uy = -uy; uz = -uz; }  // pick the +Y (up) half of the line
    const float angle = std::acos(std::clamp(uy, -1.0f, 1.0f));  // angle from +Y
    if(angle < 0.0017f) return;                                  // < ~0.1 deg: already level
    // Rotation axis = up x (0,1,0) = (-uz, 0, ux); turning `up` onto +Y.
    axisAngleToMat16(-uz, 0.0f, ux, angle, outMat16);
}

bool computeImuLevelMatrix(float ax, float ay, float az, const float accelToOptRot[9], float outMat16[16]) {
    setIdentity16(outMat16);
    const float n = std::sqrt(ax*ax + ay*ay + az*az);
    if(n < 1e-3f) return false;  // free-fall / no reading: nothing to align to
    // The accelerometer reports in the IMU frame, which on Femto Bolt is NOT the
    // optical frame. Rotate it into the optical frame (row-major: opt = R*accel).
    const float ox = accelToOptRot[0]*ax + accelToOptRot[1]*ay + accelToOptRot[2]*az;
    const float oy = accelToOptRot[3]*ax + accelToOptRot[4]*ay + accelToOptRot[5]*az;
    const float oz = accelToOptRot[6]*ax + accelToOptRot[7]*ay + accelToOptRot[8]*az;
    // Optical (+X right, +Y down, +Z forward) -> render frame (x, -y, -z).
    // levelMatrixFromUp picks the +Y (true up) half, so whether the IMU reports
    // gravity or anti-gravity does not matter.
    levelMatrixFromUp(ox, -oy, -oz, outMat16);
    return true;
}

bool computeFloorLevelMatrix(const GpuMesh &mesh, float outMat16[16]) {
    setIdentity16(outMat16);
    const int total = mesh.points;
    if(!mesh.hasData || total < 200) return false;

    // Subsample so RANSAC stays cheap; a few thousand points define a plane well.
    constexpr int kMaxSamples = 4000;
    const int stride = std::max(1, total / kMaxSamples);
    std::vector<float> pts;
    pts.reserve(static_cast<size_t>(total / stride + 1) * 3u);
    for(int i = 0; i < total; i += stride) {
        const size_t p = static_cast<size_t>(i) * 3u;
        pts.push_back(mesh.xyz[p + 0]);
        pts.push_back(mesh.xyz[p + 1]);
        pts.push_back(mesh.xyz[p + 2]);
    }
    const int m = static_cast<int>(pts.size() / 3u);
    if(m < 50) return false;

    std::mt19937 rng(12345u);  // deterministic; the live cloud already varies frame-to-frame
    std::uniform_int_distribution<int> pick(0, m - 1);

    constexpr float kInlier = 0.03f;            // 3 cm band
    const int kMinInliers = std::max(50, m / 8);

    // Each accepted candidate is a roughly-horizontal plane whose normal has
    // been oriented toward the side that holds most of the cloud (the room sits
    // above the floor). `above` is how many points lie on that populated side.
    struct Cand { float n[3]; float d; int inl; int above; };
    std::vector<Cand> cands;

    constexpr int kIters = 240;
    for(int it = 0; it < kIters; ++it) {
        const int i0 = pick(rng), i1 = pick(rng), i2 = pick(rng);
        if(i0 == i1 || i1 == i2 || i0 == i2) continue;
        const float *a = &pts[static_cast<size_t>(i0) * 3u];
        const float *b = &pts[static_cast<size_t>(i1) * 3u];
        const float *c = &pts[static_cast<size_t>(i2) * 3u];
        const float e1x = b[0]-a[0], e1y = b[1]-a[1], e1z = b[2]-a[2];
        const float e2x = c[0]-a[0], e2y = c[1]-a[1], e2z = c[2]-a[2];
        float nx = e1y*e2z - e1z*e2y, ny = e1z*e2x - e1x*e2z, nz = e1x*e2y - e1y*e2x;
        const float nl = std::sqrt(nx*nx + ny*ny + nz*nz);
        if(nl < 1e-6f) continue;
        nx /= nl; ny /= nl; nz /= nl;
        // Quick reject near-vertical planes (walls). The mount can be steep, so
        // still allow normals up to ~80 deg from vertical.
        if(std::fabs(ny) < 0.15f) continue;
        float d = -(nx*a[0] + ny*a[1] + nz*a[2]);
        int inl = 0, above = 0, below = 0;
        for(int k = 0; k < m; ++k) {
            const float *q = &pts[static_cast<size_t>(k) * 3u];
            const float s = nx*q[0] + ny*q[1] + nz*q[2] + d;
            if(std::fabs(s) < kInlier) ++inl;
            else if(s > 0.0f)          ++above;
            else                       ++below;
        }
        if(inl < kMinInliers) continue;
        // Orient the normal toward the populated (room) side.
        if(below > above) { nx = -nx; ny = -ny; nz = -nz; d = -d; std::swap(above, below); }
        // Floor-like only: the room-facing normal must point generally UP. This
        // rejects the ceiling (room is below it -> normal points down) and any
        // wall that slipped past the quick reject.
        if(ny < 0.15f) continue;
        cands.push_back(Cand{{nx, ny, nz}, d, inl, above});
    }
    if(cands.empty()) return false;

    // Pick the FLOOR: among well-supported candidates, the plane with the most
    // points above it is the lowest one. A tabletop has points below it (the
    // real floor), so it loses to the floor, which has the whole room above.
    int maxInl = 0;
    for(const auto &c : cands) maxInl = std::max(maxInl, c.inl);
    const int inlThresh = std::max(kMinInliers, (maxInl * 6) / 10);
    const Cand *best = nullptr;
    for(const auto &c : cands) {
        if(c.inl < inlThresh) continue;
        if(!best || c.above > best->above) best = &c;
    }
    if(!best) return false;

    float N[3] = {best->n[0], best->n[1], best->n[2]};
    const float D = best->d;

    // Refine the normal by least-squares over the chosen plane's inliers (robust
    // largest-determinant-axis form, no eigensolver needed).
    double cx = 0, cy = 0, cz = 0; int cnt = 0;
    for(int k = 0; k < m; ++k) {
        const float *q = &pts[static_cast<size_t>(k) * 3u];
        if(std::fabs(N[0]*q[0] + N[1]*q[1] + N[2]*q[2] + D) < kInlier) {
            cx += q[0]; cy += q[1]; cz += q[2]; ++cnt;
        }
    }
    if(cnt >= 3) {
        cx /= cnt; cy /= cnt; cz /= cnt;
        double xx = 0, xy = 0, xz = 0, yy = 0, yz = 0, zz = 0;
        for(int k = 0; k < m; ++k) {
            const float *q = &pts[static_cast<size_t>(k) * 3u];
            if(std::fabs(N[0]*q[0] + N[1]*q[1] + N[2]*q[2] + D) >= kInlier) continue;
            const double dx = q[0]-cx, dy = q[1]-cy, dz = q[2]-cz;
            xx += dx*dx; xy += dx*dy; xz += dx*dz; yy += dy*dy; yz += dy*dz; zz += dz*dz;
        }
        const double detX = yy*zz - yz*yz, detY = xx*zz - xz*xz, detZ = xx*yy - xy*xy;
        double rx, ry, rz;
        if(detX >= detY && detX >= detZ)      { rx = detX;            ry = xz*yz - xy*zz; rz = xy*yz - xz*yy; }
        else if(detY >= detX && detY >= detZ) { rx = xz*yz - xy*zz;   ry = detY;          rz = xy*xz - yz*xx; }
        else                                  { rx = xy*yz - xz*yy;   ry = xy*xz - yz*xx; rz = detZ;          }
        const double rl = std::sqrt(rx*rx + ry*ry + rz*rz);
        if(rl > 1e-9) {
            float rnx = static_cast<float>(rx/rl), rny = static_cast<float>(ry/rl), rnz = static_cast<float>(rz/rl);
            // Keep the refined normal on the same (up) side as the chosen one.
            if(rnx*N[0] + rny*N[1] + rnz*N[2] < 0.0f) { rnx = -rnx; rny = -rny; rnz = -rnz; }
            N[0] = rnx; N[1] = rny; N[2] = rnz;
        }
    }

    if(N[1] < 0.0f) { N[0] = -N[0]; N[1] = -N[1]; N[2] = -N[2]; }
    if(std::fabs(N[1]) < 0.15f) return false;
    levelMatrixFromUp(N[0], N[1], N[2], outMat16);

    // Center + snap onto the reference grid: rotate the floor's inlier centroid
    // (which lies on the plane) and translate so it lands at (0, kGridY, 0).
    if(cnt >= 3) {
        const float rx = outMat16[0]*static_cast<float>(cx) + outMat16[4]*static_cast<float>(cy) + outMat16[8] *static_cast<float>(cz);
        const float ry = outMat16[1]*static_cast<float>(cx) + outMat16[5]*static_cast<float>(cy) + outMat16[9] *static_cast<float>(cz);
        const float rz = outMat16[2]*static_cast<float>(cx) + outMat16[6]*static_cast<float>(cy) + outMat16[10]*static_cast<float>(cz);
        outMat16[12] = -rx;
        outMat16[13] = kGridY - ry;
        outMat16[14] = -rz;
    }
    return true;
}

bool estimateFloorSnap(const float rot[16], const GpuMesh &mesh,
                       float &txOut, float &tyOut, float &tzOut) {
    const int total = mesh.points;
    if(!mesh.hasData || total < 100) return false;

    constexpr int kMaxSamples = 6000;
    const int stride = std::max(1, total / kMaxSamples);
    std::vector<float> rxv, ryv, rzv;
    const size_t cap = static_cast<size_t>(total / stride + 1);
    rxv.reserve(cap); ryv.reserve(cap); rzv.reserve(cap);
    float ymin = 1e9f, ymax = -1e9f;
    for(int i = 0; i < total; i += stride) {
        const size_t p = static_cast<size_t>(i) * 3u;
        const float x = mesh.xyz[p + 0], y = mesh.xyz[p + 1], z = mesh.xyz[p + 2];
        // Rotated point (rotation part only; col-major rot[col*4 + row]).
        const float rx = rot[0]*x + rot[4]*y + rot[8] *z;
        const float ry = rot[1]*x + rot[5]*y + rot[9] *z;
        const float rz = rot[2]*x + rot[6]*y + rot[10]*z;
        rxv.push_back(rx); ryv.push_back(ry); rzv.push_back(rz);
        ymin = std::min(ymin, ry);
        ymax = std::max(ymax, ry);
    }
    if(ryv.size() < 50 || (ymax - ymin) < 1e-4f) return false;

    // Histogram the rotated heights; the floor is the LOWEST bin with a real
    // population (sparse bins below it are depth noise, not the floor).
    constexpr int B = 128;
    int hist[B] = {0};
    const float scale = static_cast<float>(B - 1) / (ymax - ymin);
    for(const float y : ryv) {
        int b = static_cast<int>((y - ymin) * scale);
        if(b < 0) b = 0; else if(b >= B) b = B - 1;
        ++hist[b];
    }
    const int need = std::max(20, static_cast<int>(ryv.size()) / 50);  // ~2% of points
    int floorBin = -1;
    for(int b = 0; b < B; ++b) {
        if(hist[b] >= need) { floorBin = b; break; }
    }
    if(floorBin < 0) return false;

    const float floorY = ymin + (static_cast<float>(floorBin) + 0.5f) * (ymax - ymin) / static_cast<float>(B);
    // Centroid of the floor-band points (the visible floor patch) in XZ + its Y.
    double sx = 0, sy = 0, sz = 0; int c = 0;
    for(size_t k = 0; k < ryv.size(); ++k) {
        if(std::fabs(ryv[k] - floorY) < 0.06f) { sx += rxv[k]; sy += ryv[k]; sz += rzv[k]; ++c; }
    }
    if(c < 10) return false;
    txOut = -static_cast<float>(sx / c);
    tyOut = kGridY - static_cast<float>(sy / c);
    tzOut = -static_cast<float>(sz / c);
    return true;
}
