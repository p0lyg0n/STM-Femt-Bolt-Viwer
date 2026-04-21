#pragma once

#include "types.h"
#include "gl_utils.h"

// ---------------------------------------------------------------------------
// GLFW input callbacks
// ---------------------------------------------------------------------------

void onMouseButton(GLFWwindow *window, int button, int action, int mods);
void onCursorPos(GLFWwindow *window, double x, double y);
void onScroll(GLFWwindow *window, double xoffset, double yoffset);

// ---------------------------------------------------------------------------
// Keyboard helpers
// ---------------------------------------------------------------------------

bool isExitKeyPressed(GLFWwindow *window);
void applyHotkeysToActiveSession(AppRuntime &runtime, GLFWwindow *window);

// Cycle the point-cloud render mode (MESH → POINT → CPU POINT → MESH)
// for every camera session simultaneously. Used by the sidebar button.
void cycleAllSessionsPointMode(AppRuntime &runtime);

// Reset the 3D viewer (yaw / pitch / zoom / pan) on every camera session.
// Equivalent to pressing R but applied to all cameras at once.
void resetAllSessionsView(AppRuntime &runtime);
