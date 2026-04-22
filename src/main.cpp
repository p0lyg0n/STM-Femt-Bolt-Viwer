#include "types.h"
#include "usb_topology.h"
#include "camera_session.h"
#include "gl_utils.h"
#include "render.h"
#include "input.h"
#include "i18n.h"
#include "app_settings.h"
#include "log_util.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl2.h>

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <conio.h>
#endif

namespace {

int estimateDecimatedPointCount(const GpuMesh &mesh, int targetPoints) {
    int decimation = 1;
    while(mesh.points / (decimation * decimation) > targetPoints) ++decimation;
    return std::max(1, mesh.points / (decimation * decimation));
}

int runCpuFallbackMode(const std::shared_ptr<ob::Pipeline> &pipeline, const std::shared_ptr<ob::Align> &align) {
    std::cout << logc::yellow << "[WARN]" << logc::reset
              << logc::yellow << " [Fallback] OpenGL unavailable. Running CPU fallback mode (no GPU viewer)." << logc::reset << std::endl;
    std::cout << logc::yellow << "[WARN]" << logc::reset
              << logc::yellow << " [Fallback] Press Q or ESC in this console to exit." << logc::reset << std::endl;

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
        auto alignedFrameset = getAlignedFrameSet(fs, align);
        if(!alignedFrameset) continue;

        auto colorFrame = alignedFrameset->colorFrame();
        auto depthFrame = alignedFrameset->depthFrame();

        tryFetchCameraParam(pipeline, cameraParamReady, cameraParam);

        if(colorFrame && convertColorFrameToRgb(colorFrame, rgb, rgbW, rgbH)) {
            fpsColor.tick();
        }
        if(depthFrame && convertDepthFrameToPseudoRgb(depthFrame, depthPseudo, depthW, depthH)) {
            fpsDepth.tick();
        }
        if(colorFrame && depthFrame && cameraParamReady && !rgb.empty()) {
            if(rebuildMeshFromAlignedDepthColor(depthFrame, rgb, rgbW, rgbH, cameraParam, mesh)) {
                // CPUフォールバックは間引き前提で表示相当点数を維持
                drawPoints = estimateDecimatedPointCount(mesh, kCpuFallbackTargetPoints);
                fpsPoint.tick();
            }
        }

        fpsLog.tick();
        if(fpsLog.frameCount == 0) {
            std::cout << logc::brightYellow << "[Fallback]" << logc::reset
                      << logc::dim << " color=" << logc::reset << std::fixed << std::setprecision(1) << fpsColor.fps
                      << logc::dim << " depth=" << logc::reset << fpsDepth.fps
                      << logc::dim << " point=" << logc::reset << fpsPoint.fps
                      << logc::dim << " drawPts=" << logc::reset << drawPoints
                      << logc::dim << " mode=" << logc::reset << "CPU" << std::endl;
        }
    }
    return 0;
}

} // namespace

int main() try {
    logc::enableVirtualTerminal();

    std::cout << logc::brightCyan << logc::bold
              << "==================================================\n"
              << "  STM2 Femto Bolt Viewer\n"
              << "==================================================" << logc::reset << "\n";

    // Load persisted user preferences (language + display mode + stream preset).
    const app_settings::AppSettings loadedSettings = app_settings::load();
    i18n::setLang(loadedSettings.lang);
    std::cout << logc::cyan << "[INFO]" << logc::reset
              << " language=" << i18n::langCode(loadedSettings.lang)
              << "  mode=" << toPointModeText(loadedSettings.pointMode)
              << "  depth=" << loadedSettings.stream.depthW << "x" << loadedSettings.stream.depthH
              << "  color=" << loadedSettings.stream.colorW << "x" << loadedSettings.stream.colorH
              << "  fps=" << loadedSettings.stream.fps
              << (loadedSettings.showIr ? "  showIr=on" : "")
              << std::endl;

    ob::Context context;
    auto deviceList = context.queryDeviceList();
    if(!deviceList || getDeviceListCount(deviceList) == 0) {
        std::cerr << logc::brightRed << logc::bold << "[ERR ]" << logc::reset
                  << logc::red << " No Orbbec device found." << logc::reset << std::endl;
        return 1;
    }

    auto sessions = createCameraSessionsFromDeviceList(deviceList, 4);
    if(sessions.empty()) {
        std::cerr << logc::brightRed << logc::bold << "[ERR ]" << logc::reset
                  << logc::red << " Failed to create camera sessions." << logc::reset << std::endl;
        return 1;
    }
    std::cout << logc::brightGreen << "[OK  ]" << logc::reset
              << " " << sessions.size() << " camera(s) detected." << std::endl;

    // Apply loaded stream preset to every session BEFORE startCameraSession so the
    // pipeline starts with the last-used resolution/fps instead of the built-in defaults.
    for(const auto &session : sessions) {
        if(!session) continue;
        session->streamSettings        = loadedSettings.stream;
        session->viewState.pointMode   = loadedSettings.pointMode;
    }

    if(!glfwInit()) {
        std::cerr << logc::yellow << "[WARN]" << logc::reset
                  << logc::yellow << " GLFW init failed. Switching to CPU fallback." << logc::reset << std::endl;
        auto fallbackPipeline = sessions.front()->pipeline;
        auto fallbackAlign = sessions.front()->align;
        startCameraSession(sessions.front());
        const int ret = runCpuFallbackMode(fallbackPipeline, fallbackAlign);
        fallbackPipeline->stop();
        return ret;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    const int windowW = 1280;
    const int windowH = (sessions.size() > 2) ? 1080 : ((sessions.size() > 1) ? 900 : 720);
    GLFWwindow *window = glfwCreateWindow(windowW, windowH, kWindowTitle, nullptr, nullptr);
    if(!window) {
        std::cerr << logc::yellow << "[WARN]" << logc::reset
                  << logc::yellow << " GLFW window create failed. Switching to CPU fallback." << logc::reset << std::endl;
        glfwTerminate();
        auto fallbackPipeline = sessions.front()->pipeline;
        auto fallbackAlign = sessions.front()->align;
        startCameraSession(sessions.front());
        const int ret = runCpuFallbackMode(fallbackPipeline, fallbackAlign);
        fallbackPipeline->stop();
        return ret;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(loadedSettings.vsync ? 1 : 0);

    // Show the UI background color immediately so the window never flashes white.
    glClearColor(0.05f, 0.04f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(window);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(window);

    AppRuntime runtime;
    runtime.streamSettings = loadedSettings.stream;
    runtime.showIr         = loadedSettings.showIr;
    runtime.vsync          = loadedSettings.vsync;
    {
        const char *v   = reinterpret_cast<const char *>(glGetString(GL_VENDOR));
        const char *r   = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
        const char *ver = reinterpret_cast<const char *>(glGetString(GL_VERSION));
        runtime.glVendor   = v   ? v   : "Unknown";
        runtime.glRenderer = r   ? r   : "Unknown";
        runtime.glVersion  = ver ? ver : "Unknown";
        std::cout << logc::cyan << "[INFO]" << logc::reset << " GL Vendor:   " << runtime.glVendor   << "\n"
                  << logc::cyan << "[INFO]" << logc::reset << " GL Renderer: " << runtime.glRenderer << "\n"
                  << logc::cyan << "[INFO]" << logc::reset << " GL Version:  " << runtime.glVersion  << "\n";
    }
    runtime.activeSessionIndex = 0;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    {
        ImGuiStyle &style = ImGui::GetStyle();
        ImGui::StyleColorsDark();
        style.WindowRounding   = 0.0f;
        style.FrameRounding    = 3.0f;
        style.WindowBorderSize = 0.0f;
        style.ItemSpacing      = ImVec2(6, 4);
        style.Colors[ImGuiCol_WindowBg]  = ImVec4(0.04f, 0.04f, 0.06f, 0.97f);
        style.Colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.22f, 0.28f, 1.0f);
    }
    {
        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = nullptr;

        // Japanese glyphs (Meiryo covers Latin + Japanese) + extra symbols (●, ■, •, ⚠, →, °).
        // Korean glyphs are loaded from Malgun Gothic via ImGui's font merge mode below
        // because Meiryo does not contain Hangul.
        static ImVector<ImWchar> jpGlyphRanges;
        if(jpGlyphRanges.empty()) {
            ImFontGlyphRangesBuilder builder;
            builder.AddRanges(io.Fonts->GetGlyphRangesJapanese());
            builder.AddChar(0x25CF); // ●
            builder.AddChar(0x25A0); // ■
            builder.AddChar(0x2022); // •
            builder.AddChar(0x26A0); // ⚠
            builder.AddChar(0x2192); // →
            builder.AddChar(0x00B0); // °
            builder.BuildRanges(&jpGlyphRanges);
        }
        static ImVector<ImWchar> krGlyphRanges;
        if(krGlyphRanges.empty()) {
            ImFontGlyphRangesBuilder builder;
            builder.AddRanges(io.Fonts->GetGlyphRangesKorean());
            builder.BuildRanges(&krGlyphRanges);
        }

        const char *jpFontPath = "C:\\Windows\\Fonts\\meiryo.ttc";
        const char *krFontPath = "C:\\Windows\\Fonts\\malgun.ttf";
        const char *latinFontPath = "C:\\Windows\\Fonts\\segoeui.ttf";
        const bool hasJp = std::ifstream(jpFontPath).good();
        const bool hasKr = std::ifstream(krFontPath).good();
        const bool hasLatin = std::ifstream(latinFontPath).good();

        auto loadFontWithMerge = [&](float size) -> ImFont * {
            ImFont *f = nullptr;
            if(hasJp) {
                f = io.Fonts->AddFontFromFileTTF(jpFontPath, size, nullptr, jpGlyphRanges.Data);
            } else if(hasLatin) {
                f = io.Fonts->AddFontFromFileTTF(latinFontPath, size);
            }
            if(f && hasKr) {
                ImFontConfig cfg;
                cfg.MergeMode = true;
                cfg.PixelSnapH = true;
                io.Fonts->AddFontFromFileTTF(krFontPath, size, &cfg, krGlyphRanges.Data);
            }
            return f;
        };

        runtime.fontSmall  = loadFontWithMerge(20.0f);
        runtime.fontNormal = loadFontWithMerge(23.0f);
        runtime.fontLarge  = loadFontWithMerge(30.0f);
    }
    ImGui_ImplGlfw_InitForOpenGL(window, false);
    ImGui_ImplOpenGL2_Init();
    glfwSetWindowUserPointer(window, &runtime);

    // Startup splash: shown while cameras are initializing so the window is never blank.
    {
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        glClearColor(0.05f, 0.04f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        ImDrawList *dl = ImGui::GetForegroundDrawList();
        ImFont *fontL = runtime.fontLarge  ? runtime.fontLarge  : ImGui::GetFont();
        ImFont *fontS = runtime.fontSmall  ? runtime.fontSmall  : ImGui::GetFont();

        const char *title = "STM2 Femto Bolt Viewer";
        const char *msg   = "起動中... カメラを初期化しています";
        const char *sub   = "Initializing cameras, please wait.";
        ImVec2 tSz = fontL->CalcTextSizeA(26.0f, FLT_MAX, 0.0f, title);
        ImVec2 mSz = fontL->CalcTextSizeA(20.0f, FLT_MAX, 0.0f, msg);
        ImVec2 sSz = fontS->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, sub);
        const float cx = (float)fbW * 0.5f;
        const float cy = (float)fbH * 0.5f;
        dl->AddText(fontL, 26.0f, {cx - tSz.x * 0.5f, cy - 56.0f}, IM_COL32(255, 228, 90, 255), title);
        dl->AddText(fontL, 20.0f, {cx - mSz.x * 0.5f, cy - 10.0f}, IM_COL32(230, 230, 240, 255), msg);
        dl->AddText(fontS, 14.0f, {cx - sSz.x * 0.5f, cy + 22.0f}, IM_COL32(170, 170, 185, 255), sub);

        ImGui::Render();
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    for(const auto &session : sessions) {
        initCameraSessionTextures(session);
    }

    for(const auto &session : sessions) {
        try {
            startCameraSession(session);
        } catch(const std::exception &e) {
            std::cerr << logc::brightRed << logc::bold << "[ERR ]" << logc::reset
                      << logc::red << " Failed to start camera session " << session->deviceIndex << ": " << e.what() << logc::reset << std::endl;
        } catch(...) {
            std::cerr << logc::brightRed << logc::bold << "[ERR ]" << logc::reset
                      << logc::red << " Failed to start camera session " << session->deviceIndex << ": unknown error" << logc::reset << std::endl;
        }
    }

    runtime.sessions = sessions;
    glfwSetMouseButtonCallback(window, onMouseButton);
    glfwSetCursorPosCallback(window, onCursorPos);
    glfwSetScrollCallback(window, onScroll);

    registerDeviceHotplugHandler(context, runtime);
    startUsbTopologyWorker(context, runtime);

    std::cout << logc::cyan << "[INFO]" << logc::reset
              << " Controls: left drag rotate / right drag pan / wheel zoom / r reset / m mode / q ESC exit" << std::endl;
    std::cout << logc::cyan << "[INFO]" << logc::reset
              << " Layout: 1-2台 縦分割 / 3-4台 縦 1xN (短い窓では 2x2 に自動切替)" << std::endl;
    std::cout << logc::gray
              << "--------------------------------------------------" << logc::reset << std::endl;

    while(!glfwWindowShouldClose(window)) {
        for(const auto &session : sessions) {
            // A camera unplug can cause the Orbbec SDK to throw from inside
            // frame handling or the timeout-triggered restart path. Swallow
            // and log rather than letting it tear down the whole main loop —
            // the USB topology worker will mark the session disconnected and
            // recover when the device returns.
            try {
                updateSessionFromFrames(session);
            } catch(const ob::Error &e) {
                std::cerr << logc::yellow << "[WARN]" << logc::reset
                          << logc::yellow << " updateSessionFromFrames Orbbec error: "
                          << e.getName() << " " << e.getMessage() << logc::reset << std::endl;
            } catch(const std::exception &e) {
                std::cerr << logc::yellow << "[WARN]" << logc::reset
                          << logc::yellow << " updateSessionFromFrames exception: "
                          << e.what() << logc::reset << std::endl;
            } catch(...) {
                std::cerr << logc::yellow << "[WARN]" << logc::reset
                          << logc::yellow << " updateSessionFromFrames unknown exception" << logc::reset << std::endl;
            }
            try {
                pollDeviceTemperature(session);
            } catch(...) {}
        }

        glfwPollEvents();
        applyHotkeysToActiveSession(runtime, window);
        if(isExitKeyPressed(window)) break;
        updateRuntimeFramebufferSize(window, runtime);

        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        glClearColor(0.05f, 0.04f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const SystemUsbTopology usbTopology = snapshotUsbTopology(runtime);
        const std::unordered_map<std::string, int> controllerUsage = snapshotControllerUsage(runtime);
        renderSidebar(runtime);
        for(size_t i = 0; i < sessions.size(); ++i) {
            renderSessionSlot(sessions[i], runtime, i, usbTopology, controllerUsage);
        }

        ImGui::Render();
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        for(const auto &session : sessions) {
            session->fpsLog.tick();
            if(session->fpsLog.frameCount == 0) {
                std::cout << logc::brightYellow << "[Device " << session->deviceIndex << "]" << logc::reset
                          << logc::dim << " color=" << logc::reset << std::fixed << std::setprecision(1) << session->fpsColor.fps
                          << logc::dim << " depth=" << logc::reset << session->fpsDepth.fps
                          << logc::dim << " point=" << logc::reset << session->fpsPoint.fps
                          << logc::dim << " drawPts=" << logc::reset << session->latestPoints
                          << logc::dim << " mode=" << logc::reset << toPointModeText(session->viewState.pointMode) << std::endl;
            }
        }
    }

    // --- Shutdown ---
    // The window has been closed (or q/ESC pressed). Persist user settings
    // FIRST so they are never lost, then exit fast.
    //
    // Why the abrupt exit: ob::Pipeline destruction calls the SDK's blocking
    // stop() which waits for the USB subsystem to acknowledge. If a camera
    // is in a bad USB state at exit time this can stall the process for
    // several seconds, making the app look frozen even though the window is
    // gone. For a user-driven UI close we don't need a graceful Orbbec
    // teardown — the OS will reclaim all handles/threads when the process
    // exits. We do still stop the topology worker so it isn't left scanning
    // after we've freed shared state.
    {
        app_settings::AppSettings saved;
        saved.lang      = i18n::getLang();
        saved.stream    = runtime.streamSettings;
        saved.pointMode = sessions.empty() ? loadedSettings.pointMode
                                           : sessions.front()->viewState.pointMode;
        saved.showIr    = runtime.showIr;
        saved.vsync     = runtime.vsync;
        app_settings::save(saved);
    }
    stopUsbTopologyWorker(runtime);

    std::cout.flush();
    std::cerr.flush();
    std::_Exit(0);
} catch(const ob::Error &e) {
    std::cerr << logc::brightRed << logc::bold << "[ERR ]" << logc::reset
              << logc::red << " Orbbec error: function=" << e.getName() << " args=" << e.getArgs()
              << " message=" << e.getMessage() << " type=" << e.getExceptionType() << logc::reset << std::endl;
    return 1;
} catch(const std::exception &e) {
    std::cerr << logc::brightRed << logc::bold << "[ERR ]" << logc::reset
              << logc::red << " std::exception: " << e.what() << logc::reset << std::endl;
    return 1;
} catch(...) {
    std::cerr << logc::brightRed << logc::bold << "[ERR ]" << logc::reset
              << logc::red << " unknown exception" << logc::reset << std::endl;
    return 1;
}
