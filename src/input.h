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

// Snap the 3D view to a preset orientation (yaw/pitch in degrees) on every
// camera session and recenter the pan — used by the Top/Front/Side/Bottom view
// buttons. Zoom is preserved.
void setAllSessionsView(AppRuntime &runtime, float yawDeg, float pitchDeg);

// Refresh each camera's floor-leveling correction according to runtime.levelMode.
// IMU mode recomputes every frame from the latest accelerometer reading; Floor
// mode recomputes only when runtime.levelRecomputeRequested is set (mode switch
// or the "re-fit" button). Off clears the correction. Call once per frame.
void updateAllSessionsLeveling(AppRuntime &runtime);
