#include "gl_utils.h"

#include <algorithm>
#include <cmath>
#include <vector>

// ---------------------------------------------------------------------------
// File-local helpers
// ---------------------------------------------------------------------------

namespace {

float degreesToRadians(float degree) {
    return degree * kPi / 180.0f;
}

void applyViewTransform(const ViewerControl &view, float &x, float &y, float &z) {
    const float yaw   = degreesToRadians(view.yawDeg);
    const float pitch = degreesToRadians(view.pitchDeg);
    const float cy = std::cos(yaw),   sy = std::sin(yaw);
    const float cx = std::cos(pitch), sx = std::sin(pitch);
    // Ry
    const float x1 = cy * x + sy * z;
    const float y1 = y;
    const float z1 = -sy * x + cy * z;
    // Rx
    x = x1;
    y = cx * y1 - sx * z1;
    z = sx * y1 + cx * z1;
    x += view.panX * kPanToWorldScale;
    y += -view.panY * kPanToWorldScale;
    z += kCameraBaseOffsetZ;
}

void drawRectOutline(const Viewport &vp, float r, float g, float b, float a = 1.0f) {
    glViewport(vp.x, vp.y, vp.w, vp.h);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glColor4f(r, g, b, a);
    glBegin(GL_LINE_LOOP);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(1.0f, 0.0f);
    glVertex2f(1.0f, 1.0f);
    glVertex2f(0.0f, 1.0f);
    glEnd();
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glDisable(GL_BLEND);
}

void drawButtonBox(const Viewport &vp, float fillR, float fillG, float fillB, float fillA, float borderR, float borderG, float borderB) {
    drawFilledRect(vp, fillR, fillG, fillB, fillA);
    drawRectOutline(vp, borderR, borderG, borderB, 1.0f);
}

} // namespace

// ---------------------------------------------------------------------------
// Public functions
// ---------------------------------------------------------------------------

GLuint createRgbGlTexture() {
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

void drawFilledRect(const Viewport &vp, float r, float g, float b, float a) {
    glViewport(vp.x, vp.y, vp.w, vp.h);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(1.0f, 0.0f);
    glVertex2f(1.0f, 1.0f);
    glVertex2f(0.0f, 1.0f);
    glEnd();
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glDisable(GL_BLEND);
}

Viewport fitViewportToAspect(int x, int y, int w, int h, float aspect) {
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

Viewport mainContentViewport(const AppRuntime &runtime) {
    const int x = std::min(kSidebarW, std::max(0, runtime.framebufferW));
    const int w = std::max(1, runtime.framebufferW - x);
    return Viewport{x, 0, w, std::max(1, runtime.framebufferH)};
}

Viewport drawTexturePane(GLuint tex, int x, int y, int w, int h, float targetAspect) {
    Viewport vp = fitViewportToAspect(x, y, w, h, targetAspect);
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

Viewport drawPointPane(const CameraViewState &s, int x, int y, int w, int h, float targetAspect) {
    Viewport vp = fitViewportToAspect(x, y, w, h, targetAspect);
    glViewport(vp.x, vp.y, vp.w, vp.h);
    glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    const float aspect = static_cast<float>(vp.w) / static_cast<float>(std::max(1, vp.h));
    const float nearZ = kNearClipZ;
    const float farZ = kFarClipZ;
    const float fovDeg = kBaseFovDeg / std::clamp(s.view.zoom, kZoomMin, kZoomMax);
    const float top = nearZ * std::tan(degreesToRadians(fovDeg) * 0.5f);
    const float right = top * aspect;
    glFrustum(-right, right, -top, top, nearZ, farZ);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(s.view.panX * kPanToWorldScale, -s.view.panY * kPanToWorldScale, kCameraBaseOffsetZ);
    glRotatef(s.view.pitchDeg, 1.0f, 0.0f, 0.0f);
    glRotatef(s.view.yawDeg, 0.0f, 1.0f, 0.0f);

    // Unity風の参照ガイド: 床グリッド(XZ) + XYZ軸ライン
    {
        const float gridY = kGridY;
        const float gridHalf = kGridHalfExtent;
        const float step = kGridStep;
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
        const float axisLen = kAxisLength;
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

bool renderCpuPointPanelImage(const CameraViewState &s, int w, int h, std::vector<uint8_t> &rgbOut) {
    if(w <= 0 || h <= 0 || !s.mesh.hasData || s.mesh.points <= 0) return false;
    rgbOut.assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 3u, 0);
    std::vector<float> zbuf(static_cast<size_t>(w) * static_cast<size_t>(h), 1e9f);

    const float aspect = static_cast<float>(w) / static_cast<float>(h);
    const float fovDeg = kBaseFovDeg / std::clamp(s.view.zoom, kZoomMin, kZoomMax);
    const float tanHalf = std::tan(degreesToRadians(fovDeg) * 0.5f);

    for(int i = 0; i < s.mesh.points; ++i) {
        const size_t p = static_cast<size_t>(i) * 3u;
        float x = s.mesh.xyz[p + 0];
        float y = s.mesh.xyz[p + 1];
        float z = s.mesh.xyz[p + 2];
        applyViewTransform(s.view, x, y, z);
        if(z >= -kNearClipZ || z <= -kFarClipZ) continue;

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
        applyViewTransform(s.view, x, y, z);
        if(z >= -kNearClipZ || z <= -kFarClipZ) return false;
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
    const float gridY = kGridY;
    const float gridHalf = kGridHalfExtent;
    const float step = kGridStep;
    for(float p = -gridHalf; p <= gridHalf + 1e-6f; p += step) {
        const bool major = std::fabs(std::fmod(std::fabs(p), 0.5f)) < 1e-4f;
        const uint8_t c = static_cast<uint8_t>(major ? 84 : 52);
        drawWorldLine(-gridHalf, gridY, p, gridHalf, gridY, p, c, c, c, 48);
        drawWorldLine(p, gridY, -gridHalf, p, gridY, gridHalf, c, c, c, 48);
    }

    drawWorldLine(0.0f, gridY, 0.0f, kAxisLength, gridY, 0.0f, 255, 50, 50, 20);              // X
    drawWorldLine(0.0f, gridY, 0.0f, 0.0f, gridY + kAxisLength, 0.0f, 50, 255, 50, 20);       // Y
    drawWorldLine(0.0f, gridY, 0.0f, 0.0f, gridY, -kAxisLength, 50, 120, 255, 20);            // Z

    return true;
}

namespace {

// Computes the minimum height a session cell needs to show its header (Device /
// USB / IMU / TEMP) plus three 16:9 panes side by side at the given cell width.
// Panes are width-driven (cellW/3) with aspect fixed to kPanelAspectRatio, so
// their height follows. Any cellH above this value would be wasted empty space
// below the panes — we pack multiple cells vertically using this natural size.
int naturalSessionCellHeight(int cellW) {
    const int paneW = std::max(1, (cellW - (kSessionPaneGap * 2)) / 3);
    const int paneH = std::max(1, static_cast<int>(static_cast<float>(paneW) / kPanelAspectRatio));
    return kSessionRowHeaderH + paneH + (kSessionRowPad * 2);
}

} // namespace

void sessionRowBounds(const AppRuntime &runtime, size_t sessionIndex, int &rowY, int &rowH) {
    const int count = std::max<int>(1, static_cast<int>(runtime.sessions.size()));
    constexpr int kSessionGap = 12;
    const int totalGap = (count > 1) ? (count - 1) * kSessionGap : 0;
    const auto mainVp = mainContentViewport(runtime);
    const int availableH = std::max(1, mainVp.h - totalGap);
    // Use the natural (content-sized) cell height when it fits — this packs
    // rows up to the top with no empty band below each camera. If the window is
    // too short to fit them naturally, fall back to dividing the space equally.
    const int natural = naturalSessionCellHeight(mainVp.w);
    const int rowHBase = std::max(1, std::min(natural, availableH / count));
    rowY = std::max(0, mainVp.h - (static_cast<int>(sessionIndex) + 1) * rowHBase - static_cast<int>(sessionIndex) * kSessionGap);
    rowH = rowHBase;
    rowY += mainVp.y;
}

void sessionCellBounds(const AppRuntime &runtime, size_t sessionIndex, int &cellX, int &cellY, int &cellW, int &cellH) {
    const int count = std::max<int>(1, static_cast<int>(runtime.sessions.size()));
    if(!useGridLayout(runtime)) {
        sessionRowBounds(runtime, sessionIndex, cellY, cellH);
        const auto mainVp = mainContentViewport(runtime);
        cellX = mainVp.x;
        cellW = mainVp.w;
        return;
    }

    constexpr int kCellGapX = 12;
    constexpr int kCellGapY = 18;
    const auto mainVp = mainContentViewport(runtime);
    const int cols = 2;
    const int rows = std::max<int>(1, (count + cols - 1) / cols);
    const int totalGapX = (cols - 1) * kCellGapX;
    const int totalGapY = (rows - 1) * kCellGapY;
    cellW = std::max(1, (mainVp.w - totalGapX) / cols);
    // Same natural-height packing as the vertical layout: cell height is the
    // minimum needed to show header + 16:9 panes at the current cellW, capped
    // by the available space per row.
    const int natural = naturalSessionCellHeight(cellW);
    cellH = std::max(1, std::min(natural, (mainVp.h - totalGapY) / rows));

    const int row = static_cast<int>(sessionIndex) / cols;
    const int col = static_cast<int>(sessionIndex) % cols;
    cellX = mainVp.x + col * (cellW + kCellGapX);
    cellY = mainVp.y + std::max(0, mainVp.h - (row + 1) * cellH - row * kCellGapY);
}

bool useGridLayout(const AppRuntime &runtime) {
    const int count = static_cast<int>(runtime.sessions.size());
    if(count <= 2) return false;
    // For 3-4 cameras the default layout is 1xN vertical (wider panes, fewer
    // columns). Only fall back to a 2x2 grid when the window is too short to
    // give each vertical cell a usable pane height.
    constexpr int kVerticalGap = 12;
    constexpr int kMinPaneHForVertical = 80;
    const auto mainVp = mainContentViewport(runtime);
    const int availH = std::max(1, mainVp.h - (count - 1) * kVerticalGap);
    const int perCellH = availH / count;
    const int contentH = perCellH - kSessionRowHeaderH - (kSessionRowPad * 2);
    return contentH < kMinPaneHForVertical;
}

int sessionIndexFromCursorY(const AppRuntime &runtime, double cursorY) {
    if(runtime.sessions.empty()) return -1;
    const double yGl = static_cast<double>(runtime.framebufferH) - cursorY;
    for(size_t i = 0; i < runtime.sessions.size(); ++i) {
        int rowY = 0;
        int rowH = 0;
        sessionRowBounds(runtime, i, rowY, rowH);
        if(yGl >= rowY && yGl < (rowY + rowH)) return static_cast<int>(i);
    }
    return -1;
}

int sessionIndexFromCursorPos(const AppRuntime &runtime, double cursorX, double cursorY) {
    if(runtime.sessions.empty()) return -1;
    const double yGl = static_cast<double>(runtime.framebufferH) - cursorY;
    for(size_t i = 0; i < runtime.sessions.size(); ++i) {
        int cellX = 0, cellY = 0, cellW = 0, cellH = 0;
        sessionCellBounds(runtime, i, cellX, cellY, cellW, cellH);
        if(cursorX >= cellX && cursorX < (cellX + cellW) && yGl >= cellY && yGl < (cellY + cellH)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool isCursorInsideSessionPointPane(const AppRuntime &runtime, size_t sessionIndex, double cursorX, double cursorY) {
    if(sessionIndex >= runtime.sessions.size()) return false;
    int cellX = 0, cellY = 0, cellW = 0, cellH = 0;
    sessionCellBounds(runtime, sessionIndex, cellX, cellY, cellW, cellH);
    const double yGl = static_cast<double>(runtime.framebufferH) - cursorY;
    constexpr int kRowPad = kSessionRowPad;
    constexpr int kRowHeaderH = kSessionRowHeaderH;
    constexpr int kPaneGap = kSessionPaneGap;
    const int paneW0 = std::max(1, (cellW - (kPaneGap * 2)) / 3);
    const int paneW1 = paneW0;
    const int paneW2 = std::max(1, cellW - paneW0 - paneW1 - (kPaneGap * 2));
    const int pointX = cellX + paneW0 + paneW1 + (kPaneGap * 2);
    const int idealPaneH = std::max(1, static_cast<int>(static_cast<float>(paneW0) / kPanelAspectRatio));
    const int contentH = std::min(idealPaneH, std::max(1, cellH - kRowHeaderH - (kRowPad * 2)));
    const int contentY = cellY + cellH - kRowHeaderH - kRowPad - contentH;
    const Viewport pointVp = fitViewportToAspect(pointX, contentY, paneW2, contentH, kPanelAspectRatio);
    return yGl >= pointVp.y && yGl < (pointVp.y + pointVp.h) && cursorX >= pointVp.x && cursorX < (pointVp.x + pointVp.w);
}

void initCameraSessionTextures(const std::shared_ptr<CameraSession> &session) {
    if(!session) return;
    if(session->texRgb == 0) session->texRgb = createRgbGlTexture();
    if(session->texDepth == 0) session->texDepth = createRgbGlTexture();
    if(session->texIr == 0) session->texIr = createRgbGlTexture();
    if(session->texPointCpu == 0) session->texPointCpu = createRgbGlTexture();
}

void updateRuntimeFramebufferSize(GLFWwindow *window, AppRuntime &runtime) {
    glfwGetFramebufferSize(window, &runtime.framebufferW, &runtime.framebufferH);
    runtime.framebufferW = std::max(1, runtime.framebufferW);
    runtime.framebufferH = std::max(1, runtime.framebufferH);
}
