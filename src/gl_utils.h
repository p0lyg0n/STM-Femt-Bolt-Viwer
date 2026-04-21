#pragma once

#include "types.h"

// ---------------------------------------------------------------------------
// Texture helpers
// ---------------------------------------------------------------------------

GLuint createRgbGlTexture();
void uploadRgbTexture(GLuint tex, const std::vector<uint8_t> &rgb, int w, int h);

// ---------------------------------------------------------------------------
// GL drawing primitives
// ---------------------------------------------------------------------------

// Draw a filled (solid) rectangle over the given viewport (renamed from drawSolidOverlay).
void drawFilledRect(const Viewport &vp, float r, float g, float b, float a);

// ---------------------------------------------------------------------------
// Viewport computation
// ---------------------------------------------------------------------------

// Fit a viewport into a rect while preserving the target aspect ratio (renamed from computeAspectViewport).
Viewport fitViewportToAspect(int x, int y, int w, int h, float aspect);

// Return the viewport for the main content area (right of sidebar).
Viewport mainContentViewport(const AppRuntime &runtime);

// ---------------------------------------------------------------------------
// Pane rendering
// ---------------------------------------------------------------------------

Viewport drawTexturePane(GLuint tex, int x, int y, int w, int h, float targetAspect = kPanelAspectRatio);
Viewport drawPointPane(const CameraViewState &s, int x, int y, int w, int h, float targetAspect = kPanelAspectRatio);

// ---------------------------------------------------------------------------
// CPU point cloud preview
// ---------------------------------------------------------------------------

bool renderCpuPointPanelImage(const CameraViewState &s, int w, int h, std::vector<uint8_t> &rgbOut);

// ---------------------------------------------------------------------------
// Session layout helpers
// ---------------------------------------------------------------------------

void sessionRowBounds(const AppRuntime &runtime, size_t sessionIndex, int &rowY, int &rowH);
void sessionCellBounds(const AppRuntime &runtime, size_t sessionIndex, int &cellX, int &cellY, int &cellW, int &cellH);
bool useGridLayout(const AppRuntime &runtime);

int sessionIndexFromCursorY(const AppRuntime &runtime, double cursorY);
int sessionIndexFromCursorPos(const AppRuntime &runtime, double cursorX, double cursorY);
bool isCursorInsideSessionPointPane(const AppRuntime &runtime, size_t sessionIndex, double cursorX, double cursorY);

// ---------------------------------------------------------------------------
// Texture initialisation
// ---------------------------------------------------------------------------

void initCameraSessionTextures(const std::shared_ptr<CameraSession> &session);

// ---------------------------------------------------------------------------
// Framebuffer size update
// ---------------------------------------------------------------------------

void updateRuntimeFramebufferSize(GLFWwindow *window, AppRuntime &runtime);
