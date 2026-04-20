#include <libobsensor/ObSensor.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <conio.h>
#include <windows.h>
#endif

#include <GLFW/glfw3.h>

namespace {

constexpr int kInitWinW = 960;
constexpr int kInitWinH = 360;
constexpr const char *kWindowTitle = "STM Femto Bolt Viewer ver1";

struct FpsMeter {
    std::chrono::steady_clock::time_point windowStart = std::chrono::steady_clock::now();
    int frameCount = 0;
    double fps = 0.0;
    void tick() {
        ++frameCount;
        const auto now = std::chrono::steady_clock::now();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - windowStart).count();
        if(ms >= 1000) {
            fps = frameCount * 1000.0 / static_cast<double>(ms);
            frameCount = 0;
            windowStart = now;
        }
    }
};

struct ViewerControl {
    float yawDeg = -8.0f;
    float pitchDeg = 5.0f;
    float zoom = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;
};

struct MouseControl {
    bool rotating = false;
    bool panning = false;
    double lastX = 0.0;
    double lastY = 0.0;
};

struct GpuMesh {
    std::vector<float> xyz;
    std::vector<uint8_t> rgb;
    std::vector<uint32_t> tris;
    int points = 0;
    bool hasData = false;
};

enum class PointRenderMode {
    GpuMesh = 0,
    GpuPoint = 1,
    CpuPoint = 2,
};

struct AppState {
    ViewerControl view;
    MouseControl mouse;
    GpuMesh mesh;
    PointRenderMode pointMode = PointRenderMode::GpuMesh;
    bool mPrev = false;
    bool rPrev = false;
    int fbW = kInitWinW;
    int fbH = kInitWinH;
    int colorW = 0;
    int colorH = 0;
    int depthW = 0;
    int depthH = 0;
    std::string colorFmt = "-";
    std::string depthFmt = "-";
};

struct Viewport {
    int x = 0;
    int y = 0;
    int w = 1;
    int h = 1;
};

Viewport fitAspectInRect(int x, int y, int w, int h, float aspect) {
    Viewport vp{x, y, std::max(1, w), std::max(1, h)};
    if(aspect <= 0.0f) return vp;
    const float rectAspect = static_cast<float>(vp.w) / static_cast<float>(vp.h);
    if(rectAspect > aspect) {
        const int nw = std::max(1, static_cast<int>(vp.h * aspect));
        vp.x += (vp.w - nw) / 2;
        vp.w = nw;
    } else {
        const int nh = std::max(1, static_cast<int>(vp.w / aspect));
        vp.y += (vp.h - nh) / 2;
        vp.h = nh;
    }
    return vp;
}

#ifdef _WIN32
GLuint g_fontBase = 0;
bool initGlTextFont() {
    if(g_fontBase != 0) return true;
    HDC hdc = wglGetCurrentDC();
    if(!hdc) return false;
    g_fontBase = glGenLists(96);
    if(g_fontBase == 0) return false;
    HFONT hFont = CreateFontA(
        -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_TT_PRECIS,
        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FF_DONTCARE, "Consolas");
    if(!hFont) return false;
    HGDIOBJ old = SelectObject(hdc, hFont);
    BOOL ok = wglUseFontBitmapsA(hdc, 32, 96, g_fontBase);
    SelectObject(hdc, old);
    DeleteObject(hFont);
    return ok == TRUE;
}

void drawText2D(const Viewport &vp, float normX, float normY, const std::string &text, float r = 1.0f, float g = 1.0f, float b = 1.0f) {
    if(g_fontBase == 0 || text.empty()) return;
    glViewport(vp.x, vp.y, vp.w, vp.h);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glColor3f(r, g, b);
    glRasterPos2f(normX, normY);
    glListBase(g_fontBase - 32);
    glCallLists(static_cast<GLsizei>(text.size()), GL_UNSIGNED_BYTE, text.c_str());
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}
#endif

std::string obFormatToText(OBFormat fmt) {
    switch(fmt) {
    case OB_FORMAT_RGB: return "RGB";
    case OB_FORMAT_BGR: return "BGR";
    case OB_FORMAT_Y16: return "Y16";
    case OB_FORMAT_Z16: return "Z16";
    case OB_FORMAT_MJPG: return "MJPG";
    case OB_FORMAT_YUYV: return "YUYV";
    default: return "FMT?";
    }
}

void onMouseButton(GLFWwindow *window, int button, int action, int) {
    auto *s = reinterpret_cast<AppState *>(glfwGetWindowUserPointer(window));
    if(!s) return;
    double x = 0.0, y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    const bool inPointPane = (x >= (s->fbW * 2.0 / 3.0));
    if(!inPointPane) return;
    if(button == GLFW_MOUSE_BUTTON_LEFT) {
        s->mouse.rotating = (action == GLFW_PRESS);
    }
    if(button == GLFW_MOUSE_BUTTON_RIGHT) {
        s->mouse.panning = (action == GLFW_PRESS);
    }
    s->mouse.lastX = x;
    s->mouse.lastY = y;
}

void onCursorPos(GLFWwindow *window, double x, double y) {
    auto *s = reinterpret_cast<AppState *>(glfwGetWindowUserPointer(window));
    if(!s) return;
    if(!s->mouse.rotating && !s->mouse.panning) {
        s->mouse.lastX = x;
        s->mouse.lastY = y;
        return;
    }
    const double dx = x - s->mouse.lastX;
    const double dy = y - s->mouse.lastY;
    if(s->mouse.rotating) {
        s->view.yawDeg += static_cast<float>(dx * 0.35);
        s->view.pitchDeg += static_cast<float>(dy * 0.25);
        s->view.pitchDeg = std::clamp(s->view.pitchDeg, -89.0f, 89.0f);
    } else if(s->mouse.panning) {
        s->view.panX += static_cast<float>(dx);
        s->view.panY += static_cast<float>(dy);
    }
    s->mouse.lastX = x;
    s->mouse.lastY = y;
}

void onScroll(GLFWwindow *window, double, double yoffset) {
    auto *s = reinterpret_cast<AppState *>(glfwGetWindowUserPointer(window));
    if(!s) return;
    double x = 0.0, y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    if(x < (s->fbW * 2.0 / 3.0)) return;
    float ratio = 1.0f + static_cast<float>(yoffset) / 10.0f;
    s->view.zoom = std::clamp(s->view.zoom * ratio, 0.35f, 4.0f);
}

bool colorFrameToRgbBytes(const std::shared_ptr<ob::VideoFrame> &frame, std::vector<uint8_t> &out, int &w, int &h) {
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

bool depthToPseudoRgb(const std::shared_ptr<ob::DepthFrame> &frame, std::vector<uint8_t> &out, int &w, int &h) {
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
            float t = std::clamp((zMm - 250.0f) / 4750.0f, 0.0f, 1.0f);
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

bool buildMeshFromDepthColor(
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

    const int step = 6;
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
            if(!std::isfinite(z) || z <= 0.12f || z > 12.0f) continue;
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

GLuint createRgbTexture() {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

void uploadRgbTexture(GLuint tex, const std::vector<uint8_t> &rgb, int w, int h) {
    if(!tex || rgb.empty() || w <= 0 || h <= 0) return;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

const char *pointModeText(PointRenderMode mode) {
    switch(mode) {
    case PointRenderMode::GpuMesh: return "GPU MESH";
    case PointRenderMode::GpuPoint: return "GPU POINT";
    case PointRenderMode::CpuPoint: return "CPU POINT";
    default: return "UNKNOWN";
    }
}

void rotateYawPitch(float x, float y, float z, float yawDeg, float pitchDeg, float &ox, float &oy, float &oz) {
    const float yaw = yawDeg * 3.1415926535f / 180.0f;
    const float pitch = pitchDeg * 3.1415926535f / 180.0f;
    const float cy = std::cos(yaw), sy = std::sin(yaw);
    const float cx = std::cos(pitch), sx = std::sin(pitch);

    // Ry
    const float x1 = cy * x + sy * z;
    const float y1 = y;
    const float z1 = -sy * x + cy * z;
    // Rx
    ox = x1;
    oy = cx * y1 - sx * z1;
    oz = sx * y1 + cx * z1;
}

bool buildCpuPointPreview(const AppState &s, int w, int h, std::vector<uint8_t> &rgbOut) {
    if(w <= 0 || h <= 0 || !s.mesh.hasData || s.mesh.points <= 0) return false;
    rgbOut.assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 3u, 0);
    std::vector<float> zbuf(static_cast<size_t>(w) * static_cast<size_t>(h), 1e9f);

    const float aspect = static_cast<float>(w) / static_cast<float>(h);
    const float fovDeg = 55.0f / std::clamp(s.view.zoom, 0.35f, 4.0f);
    const float tanHalf = std::tan((fovDeg * 3.1415926535f / 180.0f) * 0.5f);

    for(int i = 0; i < s.mesh.points; ++i) {
        const size_t p = static_cast<size_t>(i) * 3u;
        float x = s.mesh.xyz[p + 0];
        float y = s.mesh.xyz[p + 1];
        float z = s.mesh.xyz[p + 2];
        rotateYawPitch(x, y, z, s.view.yawDeg, s.view.pitchDeg, x, y, z);
        x += s.view.panX * 0.0008f;
        y += -s.view.panY * 0.0008f;
        z += -1.2f;
        if(z >= -0.05f || z <= -30.0f) continue;

        const float invZ = 1.0f / (-z);
        const float nx = (x * invZ) / (tanHalf * aspect);
        const float ny = (y * invZ) / tanHalf;
        if(nx < -1.0f || nx > 1.0f || ny < -1.0f || ny > 1.0f) continue;

        const int px = static_cast<int>((nx * 0.5f + 0.5f) * static_cast<float>(w - 1));
        const int py = static_cast<int>((1.0f - (ny * 0.5f + 0.5f)) * static_cast<float>(h - 1));
        if(px < 0 || px >= w || py < 0 || py >= h) continue;
        const size_t idx = static_cast<size_t>(py) * static_cast<size_t>(w) + static_cast<size_t>(px);
        const float depth = -z;
        if(depth >= zbuf[idx]) continue;
        zbuf[idx] = depth;
        const size_t o = idx * 3u;
        rgbOut[o + 0] = s.mesh.rgb[p + 0];
        rgbOut[o + 1] = s.mesh.rgb[p + 1];
        rgbOut[o + 2] = s.mesh.rgb[p + 2];
    }

    auto projectToScreen = [&](float wx, float wy, float wz, int &sx, int &sy) -> bool {
        float x = wx, y = wy, z = wz;
        rotateYawPitch(x, y, z, s.view.yawDeg, s.view.pitchDeg, x, y, z);
        x += s.view.panX * 0.0008f;
        y += -s.view.panY * 0.0008f;
        z += -1.2f;
        if(z >= -0.05f || z <= -30.0f) return false;
        const float invZ = 1.0f / (-z);
        const float nx = (x * invZ) / (tanHalf * aspect);
        const float ny = (y * invZ) / tanHalf;
        if(nx < -1.0f || nx > 1.0f || ny < -1.0f || ny > 1.0f) return false;
        sx = static_cast<int>((nx * 0.5f + 0.5f) * static_cast<float>(w - 1));
        sy = static_cast<int>((1.0f - (ny * 0.5f + 0.5f)) * static_cast<float>(h - 1));
        return sx >= 0 && sx < w && sy >= 0 && sy < h;
    };

    auto putPix = [&](int px, int py, uint8_t r, uint8_t g, uint8_t b) {
        if(px < 0 || px >= w || py < 0 || py >= h) return;
        const size_t o = (static_cast<size_t>(py) * static_cast<size_t>(w) + static_cast<size_t>(px)) * 3u;
        rgbOut[o + 0] = r;
        rgbOut[o + 1] = g;
        rgbOut[o + 2] = b;
    };

    auto drawLine2D = [&](int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b) {
        int dx = std::abs(x1 - x0);
        int sx = (x0 < x1) ? 1 : -1;
        int dy = -std::abs(y1 - y0);
        int sy = (y0 < y1) ? 1 : -1;
        int err = dx + dy;
        while(true) {
            putPix(x0, y0, r, g, b);
            if(x0 == x1 && y0 == y1) break;
            int e2 = err * 2;
            if(e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if(e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    };

    // 3D線を分割投影して描く（端点が画面外でも途中が見えていれば表示する）
    auto drawWorldLine = [&](float x0, float y0, float z0, float x1, float y1, float z1, uint8_t r, uint8_t g, uint8_t b, int segments = 40) {
        bool prevVisible = false;
        int prevX = 0, prevY = 0;
        for(int i = 0; i <= segments; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(segments);
            const float wx = x0 + (x1 - x0) * t;
            const float wy = y0 + (y1 - y0) * t;
            const float wz = z0 + (z1 - z0) * t;
            int sx = 0, sy = 0;
            const bool visible = projectToScreen(wx, wy, wz, sx, sy);
            if(visible && prevVisible) {
                drawLine2D(prevX, prevY, sx, sy, r, g, b);
            } else if(visible) {
                putPix(sx, sy, r, g, b);
            }
            prevVisible = visible;
            prevX = sx;
            prevY = sy;
        }
    };

    // GPU表示と同じガイド: 床グリッド(XZ) + XYZ軸
    const float gridY = -0.55f;
    const float gridHalf = 1.2f;
    const float step = 0.1f;
    for(float p = -gridHalf; p <= gridHalf + 1e-6f; p += step) {
        const bool major = std::fabs(std::fmod(std::fabs(p), 0.5f)) < 1e-4f;
        const uint8_t c = static_cast<uint8_t>(major ? 84 : 52);
        drawWorldLine(-gridHalf, gridY, p, gridHalf, gridY, p, c, c, c, 48);
        drawWorldLine(p, gridY, -gridHalf, p, gridY, gridHalf, c, c, c, 48);
    }

    drawWorldLine(0.0f, gridY, 0.0f, 0.5f, gridY, 0.0f, 255, 50, 50, 20);        // X
    drawWorldLine(0.0f, gridY, 0.0f, 0.0f, gridY + 0.5f, 0.0f, 50, 255, 50, 20); // Y
    drawWorldLine(0.0f, gridY, 0.0f, 0.0f, gridY, -0.5f, 50, 120, 255, 20);      // Z

    return true;
}

Viewport drawTexturePanel(GLuint tex, int x, int y, int w, int h, float targetAspect = 16.0f / 9.0f) {
    Viewport vp = fitAspectInRect(x, y, w, h, targetAspect);
    glViewport(vp.x, vp.y, vp.w, vp.h);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(0.0f, 0.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(1.0f, 0.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(1.0f, 1.0f);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(0.0f, 1.0f);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);
    return vp;
}

Viewport drawPointPanel(const AppState &s, int x, int y, int w, int h, float targetAspect = 16.0f / 9.0f) {
    Viewport vp = fitAspectInRect(x, y, w, h, targetAspect);
    glViewport(vp.x, vp.y, vp.w, vp.h);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    const float aspect = static_cast<float>(vp.w) / static_cast<float>(std::max(1, vp.h));
    const float nearZ = 0.05f;
    const float farZ = 30.0f;
    const float fovDeg = 55.0f / std::clamp(s.view.zoom, 0.35f, 4.0f);
    const float top = nearZ * std::tan((fovDeg * 3.1415926535f / 180.0f) * 0.5f);
    const float right = top * aspect;
    glFrustum(-right, right, -top, top, nearZ, farZ);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(s.view.panX * 0.0008f, -s.view.panY * 0.0008f, -1.2f);
    glRotatef(s.view.pitchDeg, 1.0f, 0.0f, 0.0f);
    glRotatef(s.view.yawDeg, 0.0f, 1.0f, 0.0f);

    // Unity風の参照ガイド: 床グリッド(XZ) + XYZ軸ライン
    {
        const float gridY = -0.55f;
        const float gridHalf = 1.2f;
        const float step = 0.1f;
        glLineWidth(1.0f);
        glBegin(GL_LINES);
        for(float p = -gridHalf; p <= gridHalf + 1e-6f; p += step) {
            const bool major = std::fabs(std::fmod(std::fabs(p), 0.5f)) < 1e-4f;
            const float c = major ? 0.33f : 0.20f;
            glColor3f(c, c, c);
            glVertex3f(-gridHalf, gridY, p);
            glVertex3f(gridHalf, gridY, p);
            glVertex3f(p, gridY, -gridHalf);
            glVertex3f(p, gridY, gridHalf);
        }

        // XYZ軸: X=赤, Y=緑, Z=青
        const float axisLen = 0.5f;
        glColor3f(1.0f, 0.20f, 0.20f);
        glVertex3f(0.0f, gridY, 0.0f);
        glVertex3f(axisLen, gridY, 0.0f);
        glColor3f(0.20f, 1.0f, 0.20f);
        glVertex3f(0.0f, gridY, 0.0f);
        glVertex3f(0.0f, gridY + axisLen, 0.0f);
        glColor3f(0.20f, 0.45f, 1.0f);
        glVertex3f(0.0f, gridY, 0.0f);
        glVertex3f(0.0f, gridY, -axisLen);
        glEnd();
    }

    if(s.mesh.hasData && s.pointMode == PointRenderMode::GpuMesh && !s.mesh.tris.empty()) {
        glBegin(GL_TRIANGLES);
        for(size_t i = 0; i + 2 < s.mesh.tris.size(); i += 3) {
            for(int k = 0; k < 3; ++k) {
                const uint32_t idx = s.mesh.tris[i + static_cast<size_t>(k)];
                const size_t p = static_cast<size_t>(idx) * 3u;
                glColor3ub(s.mesh.rgb[p + 0], s.mesh.rgb[p + 1], s.mesh.rgb[p + 2]);
                glVertex3f(s.mesh.xyz[p + 0], s.mesh.xyz[p + 1], s.mesh.xyz[p + 2]);
            }
        }
        glEnd();
    }
    if(s.mesh.hasData && (s.pointMode == PointRenderMode::GpuPoint || s.mesh.tris.empty())) {
        glPointSize(2.0f);
        glBegin(GL_POINTS);
        for(int i = 0; i < s.mesh.points; ++i) {
            const size_t p = static_cast<size_t>(i) * 3u;
            glColor3ub(s.mesh.rgb[p + 0], s.mesh.rgb[p + 1], s.mesh.rgb[p + 2]);
            glVertex3f(s.mesh.xyz[p + 0], s.mesh.xyz[p + 1], s.mesh.xyz[p + 2]);
        }
        glEnd();
    }
    return vp;
}

int runCpuFallbackLoop(const std::shared_ptr<ob::Pipeline> &pipeline, const std::shared_ptr<ob::Align> &align) {
    std::cout << "[Fallback] OpenGL unavailable. Running CPU fallback mode (no GPU viewer)." << std::endl;
    std::cout << "[Fallback] Press Q or ESC in this console to exit." << std::endl;

    std::vector<uint8_t> rgb;
    std::vector<uint8_t> depthPseudo;
    int rgbW = 0, rgbH = 0, depthW = 0, depthH = 0;
    OBCameraParam cameraParam = {};
    bool cameraParamReady = false;
    GpuMesh mesh;
    int drawPoints = 0;

    FpsMeter fpsColor, fpsDepth, fpsPoint, fpsLog;

    while(true) {
#ifdef _WIN32
        if(_kbhit()) {
            const int c = _getch();
            if(c == 27 || c == 'q' || c == 'Q') break;
        }
#endif
        auto fs = pipeline->waitForFrames(50);
        if(!fs) continue;
        std::shared_ptr<ob::FrameSet> alignedFrameset;
        try {
            auto aligned = align ? align->process(fs) : fs;
            alignedFrameset = aligned ? aligned->as<ob::FrameSet>() : nullptr;
        } catch(...) {
            alignedFrameset = nullptr;
        }
        if(!alignedFrameset) alignedFrameset = fs;

        auto colorFrame = alignedFrameset->colorFrame();
        auto depthFrame = alignedFrameset->depthFrame();

        if(!cameraParamReady) {
            try {
                cameraParam = pipeline->getCameraParam();
                cameraParamReady = true;
            } catch(...) {
                cameraParamReady = false;
            }
        }

        if(colorFrame && colorFrameToRgbBytes(colorFrame, rgb, rgbW, rgbH)) {
            fpsColor.tick();
        }
        if(depthFrame && depthToPseudoRgb(depthFrame, depthPseudo, depthW, depthH)) {
            fpsDepth.tick();
        }
        if(colorFrame && depthFrame && cameraParamReady && !rgb.empty()) {
            if(buildMeshFromDepthColor(depthFrame, rgb, rgbW, rgbH, cameraParam, mesh)) {
                // CPUフォールバックは間引き前提で表示相当点数を維持
                int decim = 1;
                while(mesh.points / (decim * decim) > 6000) ++decim;
                drawPoints = std::max(1, mesh.points / (decim * decim));
                fpsPoint.tick();
            }
        }

        fpsLog.tick();
        if(fpsLog.frameCount == 0) {
            std::cout << "[Fallback FPS] color=" << std::fixed << std::setprecision(1) << fpsColor.fps
                      << " depth=" << fpsDepth.fps
                      << " point=" << fpsPoint.fps
                      << " drawPts=" << drawPoints
                      << " mode=CPU" << std::endl;
        }
    }
    return 0;
}

}  // namespace

int main() try {
    std::cout << "STM Femto Bolt Viewer ver1 unified GLFW window (no OpenCV UI)" << std::endl;

    auto config = std::make_shared<ob::Config>();
    config->enableVideoStream(OB_STREAM_DEPTH, 640, 576, 30, OB_FORMAT_Y16);
    config->enableVideoStream(OB_STREAM_COLOR, 1280, 720, 30, OB_FORMAT_RGB);
    config->setFrameAggregateOutputMode(OB_FRAME_AGGREGATE_OUTPUT_FULL_FRAME_REQUIRE);

    auto pipeline = std::make_shared<ob::Pipeline>();
    pipeline->enableFrameSync();
    pipeline->start(config);
    auto align = std::make_shared<ob::Align>(OB_STREAM_COLOR);

    if(!glfwInit()) {
        std::cerr << "GLFW init failed. Switching to CPU fallback." << std::endl;
        const int ret = runCpuFallbackLoop(pipeline, align);
        pipeline->stop();
        return ret;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    GLFWwindow *window = glfwCreateWindow(kInitWinW, kInitWinH, kWindowTitle, nullptr, nullptr);
    if(!window) {
        std::cerr << "GLFW window create failed. Switching to CPU fallback." << std::endl;
        glfwTerminate();
        const int ret = runCpuFallbackLoop(pipeline, align);
        pipeline->stop();
        return ret;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
#ifdef _WIN32
    initGlTextFont();
#endif

    AppState state;
    glfwSetWindowUserPointer(window, &state);
    glfwSetMouseButtonCallback(window, onMouseButton);
    glfwSetCursorPosCallback(window, onCursorPos);
    glfwSetScrollCallback(window, onScroll);

    GLuint texRgb = createRgbTexture();
    GLuint texDepth = createRgbTexture();
    GLuint texPointCpu = createRgbTexture();

    std::vector<uint8_t> rgb;
    std::vector<uint8_t> depthPseudo;
    std::vector<uint8_t> cpuPointPreview;
    int rgbW = 0, rgbH = 0, depthW = 0, depthH = 0;
    OBCameraParam cameraParam = {};
    bool cameraParamReady = false;

    FpsMeter fpsColor, fpsDepth, fpsPoint, fpsLog;
    int latestPoints = 0;

    std::cout << "Controls: left drag rotate (right pane), right drag pan, wheel zoom, r reset, m mode-cycle(GPU MESH/GPU POINT/CPU POINT), q/ESC exit." << std::endl;

    while(!glfwWindowShouldClose(window)) {
        auto fs = pipeline->waitForFrames(100);
        if(fs) {
            std::shared_ptr<ob::FrameSet> alignedFrameset;
            try {
                auto aligned = align->process(fs);
                alignedFrameset = aligned ? aligned->as<ob::FrameSet>() : nullptr;
            } catch(...) {
                alignedFrameset = nullptr;
            }
            if(!alignedFrameset) alignedFrameset = fs;

            auto colorFrame = alignedFrameset->colorFrame();
            auto depthFrame = alignedFrameset->depthFrame();

            if(!cameraParamReady) {
                try {
                    cameraParam = pipeline->getCameraParam();
                    cameraParamReady = true;
                } catch(...) {
                    cameraParamReady = false;
                }
            }

            if(colorFrame && colorFrameToRgbBytes(colorFrame, rgb, rgbW, rgbH)) {
                uploadRgbTexture(texRgb, rgb, rgbW, rgbH);
                fpsColor.tick();
                state.colorW = rgbW;
                state.colorH = rgbH;
                state.colorFmt = obFormatToText(colorFrame->format());
            }
            if(depthFrame && depthToPseudoRgb(depthFrame, depthPseudo, depthW, depthH)) {
                uploadRgbTexture(texDepth, depthPseudo, depthW, depthH);
                fpsDepth.tick();
                state.depthW = depthW;
                state.depthH = depthH;
                state.depthFmt = obFormatToText(depthFrame->format());
            }
            if(colorFrame && depthFrame && cameraParamReady && !rgb.empty()) {
                if(buildMeshFromDepthColor(depthFrame, rgb, rgbW, rgbH, cameraParam, state.mesh)) {
                    latestPoints = state.mesh.points;
                    fpsPoint.tick();
                }
            }
        }

        glfwPollEvents();

        const bool mNow = glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS;
        const bool rNow = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
        if(mNow && !state.mPrev) {
            if(state.pointMode == PointRenderMode::GpuMesh) state.pointMode = PointRenderMode::GpuPoint;
            else if(state.pointMode == PointRenderMode::GpuPoint) state.pointMode = PointRenderMode::CpuPoint;
            else state.pointMode = PointRenderMode::GpuMesh;
        }
        if(rNow && !state.rPrev) {
            state.view = ViewerControl{};
        }
        state.mPrev = mNow;
        state.rPrev = rNow;

        if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) break;

        glfwGetFramebufferSize(window, &state.fbW, &state.fbH);
        state.fbW = std::max(1, state.fbW);
        state.fbH = std::max(1, state.fbH);

        glClearColor(0.05f, 0.04f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const int paneW0 = state.fbW / 3;
        const int paneW1 = state.fbW / 3;
        const int paneW2 = state.fbW - paneW0 - paneW1;

        Viewport vpRgb = drawTexturePanel(texRgb, 0, 0, paneW0, state.fbH, 16.0f / 9.0f);
        Viewport vpDepth = drawTexturePanel(texDepth, paneW0, 0, paneW1, state.fbH, 16.0f / 9.0f);

        Viewport vpPoint;
        if(state.pointMode == PointRenderMode::CpuPoint) {
            const int cpuW = 640;
            const int cpuH = 360;
            if(buildCpuPointPreview(state, cpuW, cpuH, cpuPointPreview)) {
                uploadRgbTexture(texPointCpu, cpuPointPreview, cpuW, cpuH);
            }
            vpPoint = drawTexturePanel(texPointCpu, paneW0 + paneW1, 0, paneW2, state.fbH, 16.0f / 9.0f);
        } else {
            vpPoint = drawPointPanel(state, paneW0 + paneW1, 0, paneW2, state.fbH, 16.0f / 9.0f);
        }

#ifdef _WIN32
        std::ostringstream t1;
        t1 << "RGB " << state.colorW << "x" << state.colorH << " " << state.colorFmt
           << " | FPS " << std::fixed << std::setprecision(1) << fpsColor.fps;
        drawText2D(vpRgb, 0.02f, 0.96f, t1.str(), 1.0f, 1.0f, 1.0f);

        std::ostringstream t2;
        t2 << "DEPTH " << state.depthW << "x" << state.depthH << " " << state.depthFmt
           << " | FPS " << std::fixed << std::setprecision(1) << fpsDepth.fps;
        drawText2D(vpDepth, 0.02f, 0.96f, t2.str(), 1.0f, 1.0f, 1.0f);

        std::ostringstream t3a;
        t3a << "XYZRGB " << pointModeText(state.pointMode)
            << " | pts " << state.mesh.points
            << " | FPS " << std::fixed << std::setprecision(1) << fpsPoint.fps;
        drawText2D(vpPoint, 0.02f, 0.96f, t3a.str(), 1.0f, 1.0f, 1.0f);

        std::ostringstream t3b;
        t3b << "render GLFW+OpenGL";
        drawText2D(vpPoint, 0.02f, 0.90f, t3b.str(), 0.85f, 0.85f, 0.85f);
#endif

        glfwSwapBuffers(window);

        fpsLog.tick();
        if(fpsLog.frameCount == 0) {
            std::cout << "[FPS] color=" << std::fixed << std::setprecision(1) << fpsColor.fps
                      << " depth=" << fpsDepth.fps
                      << " point=" << fpsPoint.fps
                      << " drawPts=" << latestPoints << std::endl;
        }
    }

    glDeleteTextures(1, &texRgb);
    glDeleteTextures(1, &texDepth);
    glDeleteTextures(1, &texPointCpu);
    glfwDestroyWindow(window);
    glfwTerminate();
    pipeline->stop();
    return 0;
} catch(const ob::Error &e) {
    std::cerr << "Orbbec error: function=" << e.getName() << " args=" << e.getArgs() << " message=" << e.getMessage() << " type=" << e.getExceptionType() << std::endl;
    return 1;
} catch(const std::exception &e) {
    std::cerr << "std::exception: " << e.what() << std::endl;
    return 1;
} catch(...) {
    std::cerr << "unknown exception" << std::endl;
    return 1;
}
