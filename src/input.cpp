#include "input.h"

#include <algorithm>

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
    const int sessionIndex = sessionIndexFromCursorPos(*runtime, x, y);
    if(sessionIndex < 0) return;
    if(!isCursorInsideSessionPointPane(*runtime, static_cast<size_t>(sessionIndex), x, y)) return;
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
