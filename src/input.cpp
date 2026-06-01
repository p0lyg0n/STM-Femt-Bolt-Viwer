#include "input.h"
#include "frame_processing.h"

#include <algorithm>
#include <mutex>

#include <imgui_impl_glfw.h>
#include <imgui.h>

namespace {

void cyclePointRenderMode(PointRenderMode &mode) {
    if(mode == PointRenderMode::GpuMesh) mode = PointRenderMode::GpuPoint;
    else if(mode == PointRenderMode::GpuPoint) mode = PointRenderMode::CpuPoint;
    else mode = PointRenderMode::GpuMesh;
}

} // namespace

void onMouseButton(GLFWwindow *window, int button, int action, int mods) {
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    if(ImGui::GetIO().WantCaptureMouse) return;
    auto *runtime = reinterpret_cast<AppRuntime *>(glfwGetWindowUserPointer(window));
    if(!runtime) return;
    if(action == GLFW_RELEASE) {
        for(auto &s : runtime->sessions) {
            if(button == GLFW_MOUSE_BUTTON_LEFT) s->viewState.mouse.rotating = false;
            if(button == GLFW_MOUSE_BUTTON_RIGHT) s->viewState.mouse.panning = false;
        }
        return;
    }
    double x = 0.0, y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    // In solo mode the whole window is the chosen camera's point pane.
    if(runtime->soloSessionIndex >= 0 && runtime->soloSessionIndex < static_cast<int>(runtime->sessions.size())) {
        runtime->activeSessionIndex = runtime->soloSessionIndex;
        auto &session = *runtime->sessions[static_cast<size_t>(runtime->soloSessionIndex)];
        if(button == GLFW_MOUSE_BUTTON_LEFT) session.viewState.mouse.rotating = true;
        if(button == GLFW_MOUSE_BUTTON_RIGHT) session.viewState.mouse.panning = true;
        session.viewState.mouse.lastX = x;
        session.viewState.mouse.lastY = y;
        return;
    }
    const int sessionIndex = sessionIndexFromCursorPos(*runtime, x, y);
    if(sessionIndex < 0) return;
    runtime->activeSessionIndex = sessionIndex;
    auto &session = *runtime->sessions[static_cast<size_t>(sessionIndex)];
    if(!isCursorInsideSessionPointPane(*runtime, static_cast<size_t>(sessionIndex), x, y)) return;
    if(button == GLFW_MOUSE_BUTTON_LEFT) session.viewState.mouse.rotating = true;
    if(button == GLFW_MOUSE_BUTTON_RIGHT) session.viewState.mouse.panning = true;
    session.viewState.mouse.lastX = x;
    session.viewState.mouse.lastY = y;
}

void onCursorPos(GLFWwindow *window, double x, double y) {
    ImGui_ImplGlfw_CursorPosCallback(window, x, y);
    auto *runtime = reinterpret_cast<AppRuntime *>(glfwGetWindowUserPointer(window));
    if(!runtime || runtime->sessions.empty()) return;
    const int sessionIndex = std::clamp(runtime->activeSessionIndex, 0, static_cast<int>(runtime->sessions.size()) - 1);
    auto &session = *runtime->sessions[static_cast<size_t>(sessionIndex)];
    if(!session.viewState.mouse.rotating && !session.viewState.mouse.panning) {
        session.viewState.mouse.lastX = x;
        session.viewState.mouse.lastY = y;
        return;
    }
    const double dx = x - session.viewState.mouse.lastX;
    const double dy = y - session.viewState.mouse.lastY;
    if(session.viewState.mouse.rotating) {
        session.viewState.view.yawDeg += static_cast<float>(dx * kYawSensitivity);
        session.viewState.view.pitchDeg += static_cast<float>(dy * kPitchSensitivity);
        session.viewState.view.pitchDeg = std::clamp(session.viewState.view.pitchDeg, -kPitchClampDeg, kPitchClampDeg);
    } else if(session.viewState.mouse.panning) {
        session.viewState.view.panX += static_cast<float>(dx);
        session.viewState.view.panY += static_cast<float>(dy);
    }
    session.viewState.mouse.lastX = x;
    session.viewState.mouse.lastY = y;
}

void onScroll(GLFWwindow *window, double xoffset, double yoffset) {
    ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);
    if(ImGui::GetIO().WantCaptureMouse) return;
    auto *runtime = reinterpret_cast<AppRuntime *>(glfwGetWindowUserPointer(window));
    if(!runtime || runtime->sessions.empty()) return;
    double x = 0.0, y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    int sessionIndex;
    if(runtime->soloSessionIndex >= 0 && runtime->soloSessionIndex < static_cast<int>(runtime->sessions.size())) {
        sessionIndex = runtime->soloSessionIndex;  // whole window = solo camera's pane
    } else {
        sessionIndex = sessionIndexFromCursorPos(*runtime, x, y);
        if(sessionIndex < 0) return;
        if(!isCursorInsideSessionPointPane(*runtime, static_cast<size_t>(sessionIndex), x, y)) return;
    }
    auto &session = *runtime->sessions[static_cast<size_t>(sessionIndex)];
    const float zoomRatio = 1.0f + static_cast<float>(yoffset) * kZoomStepScale;
    session.viewState.view.zoom = std::clamp(session.viewState.view.zoom * zoomRatio, kZoomMin, kZoomMax);
}

bool isExitKeyPressed(GLFWwindow *window) {
    return glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS ||
           glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;
}

void applyHotkeysToActiveSession(AppRuntime &runtime, GLFWwindow *window) {
    if(runtime.sessions.empty()) return;
    const int activeIndex = std::clamp(runtime.activeSessionIndex, 0, static_cast<int>(runtime.sessions.size()) - 1);
    auto &state = runtime.sessions[static_cast<size_t>(activeIndex)]->viewState;
    const bool isMDown = glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS;
    const bool isRDown = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
    if(isMDown && !state.wasMKeyDown) cycleAllSessionsPointMode(runtime);
    if(isRDown && !state.wasRKeyDown) resetAllSessionsView(runtime);
    state.wasMKeyDown = isMDown;
    state.wasRKeyDown = isRDown;
}

void cycleAllSessionsPointMode(AppRuntime &runtime) {
    if(runtime.sessions.empty()) return;
    PointRenderMode next = runtime.sessions.front()->viewState.pointMode;
    cyclePointRenderMode(next);
    for(auto &s : runtime.sessions) {
        if(s) s->viewState.pointMode = next;
    }
}

void resetAllSessionsView(AppRuntime &runtime) {
    for(auto &s : runtime.sessions) {
        if(s) s->viewState.view = ViewerControl{};
    }
}

void setAllSessionsView(AppRuntime &runtime, float yawDeg, float pitchDeg) {
    pitchDeg = std::clamp(pitchDeg, -kPitchClampDeg, kPitchClampDeg);
    for(auto &s : runtime.sessions) {
        if(!s) continue;
        ViewerControl &v = s->viewState.view;
        v.yawDeg = yawDeg;
        v.pitchDeg = pitchDeg;
        v.panX = 0.0f;
        v.panY = 0.0f;
        // zoom is preserved so presets don't fight the user's framing
    }
}

void updateAllSessionsLeveling(AppRuntime &runtime) {
    const LevelMode mode = runtime.levelMode;
    const bool refit = runtime.levelRecomputeRequested;
    for(auto &s : runtime.sessions) {
        if(!s) continue;
        CameraViewState &vs = s->viewState;

        if(mode == LevelMode::Off) {
            vs.levelEnabled = false;
            vs.levelOk = false;
            continue;
        }

        vs.levelEnabled = true;

        if(mode == LevelMode::Imu) {
            float ax = 0, ay = 0, az = 0; bool ready = false;
            {
                std::lock_guard<std::mutex> g(s->imuMutex);
                ax = s->lastAccel.x; ay = s->lastAccel.y; az = s->lastAccel.z;
                ready = s->imuReady;
            }
            const float prevTx = vs.levelMat[12], prevTy = vs.levelMat[13], prevTz = vs.levelMat[14];
            float mtx[16];
            if(ready && computeImuLevelMatrix(ax, ay, az, s->accelToOptRot, mtx)) {
                float tx = 0.0f, ty = 0.0f, tz = 0.0f;
                if(estimateFloorSnap(mtx, vs.mesh, tx, ty, tz)) {
                    // Center + snap the floor onto the grid; smooth so it doesn't
                    // jitter frame-to-frame as the cloud's points change.
                    if(vs.levelOk) {
                        tx = prevTx * 0.8f + tx * 0.2f;
                        ty = prevTy * 0.8f + ty * 0.2f;
                        tz = prevTz * 0.8f + tz * 0.2f;
                    }
                    mtx[12] = tx; mtx[13] = ty; mtx[14] = tz;
                }
                std::copy(mtx, mtx + 16, vs.levelMat);
                vs.levelOk = true;
            } else {
                vs.levelOk = false;
            }
        } else { // LevelMode::Floor — recompute only on request (mode switch / button)
            if(refit) {
                float mtx[16];
                // computeFloorLevelMatrix already bakes the floor->grid snap.
                if(vs.mesh.hasData && computeFloorLevelMatrix(vs.mesh, mtx)) {
                    std::copy(mtx, mtx + 16, vs.levelMat);
                    vs.levelOk = true;
                } else {
                    vs.levelOk = false;
                }
            }
        }
    }
    runtime.levelRecomputeRequested = false;
}
