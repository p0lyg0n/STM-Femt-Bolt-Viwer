#include "render.h"
#include "input.h"
#include "frame_processing.h"
#include "camera_session.h"
#include "i18n.h"

#include <algorithm>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>

namespace {

// Shows a wrapped tooltip when the preceding ImGui item is hovered.
// Used to attach HELP text to buttons and section headers.
void tooltipOnHover(const char *text) {
    if(!ImGui::IsItemHovered() || !text || !text[0]) return;
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

// A compact square toggle button used by the language switcher row.
// Pressed state is drawn with a highlighted background so the current language is visible.
bool langPillButton(const char *label, bool active, const ImVec2 &size) {
    if(active) {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.22f, 0.45f, 0.82f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.55f, 0.92f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.18f, 0.38f, 0.70f, 1.0f));
    }
    const bool pressed = ImGui::Button(label, size);
    if(active) ImGui::PopStyleColor(3);
    return pressed;
}

} // namespace

void renderSessionSlot(
    const std::shared_ptr<CameraSession> &session,
    AppRuntime &runtime,
    size_t sessionIndex,
    const SystemUsbTopology &usbTopology,
    const std::unordered_map<std::string, int> &controllerUsage) {
    if(!session) return;
    auto &state = session->viewState;
    state.framebufferW = runtime.framebufferW;

    constexpr int kRowHeaderH = kSessionRowHeaderH;
    constexpr int kRowPad = kSessionRowPad;
    constexpr int kPaneGap = kSessionPaneGap;

    int slotX = 0, slotY = 0, slotW = 0, slotH = 0;
    sessionCellBounds(runtime, sessionIndex, slotX, slotY, slotW, slotH);

    const int contentX = slotX + kRowPad;
    const int contentW = std::max(1, slotW - (kRowPad * 2));
    const int paneW0 = std::max(1, (contentW - (kPaneGap * 2)) / 3);
    const int paneW1 = paneW0;
    const int paneW2 = std::max(1, contentW - paneW0 - paneW1 - (kPaneGap * 2));
    const int idealPaneH = std::max(1, static_cast<int>(static_cast<float>(paneW0) / kPanelAspectRatio));
    const int contentH = std::min(idealPaneH, std::max(1, slotH - kRowHeaderH - (kRowPad * 2)));
    const int contentY = slotY + slotH - kRowHeaderH - kRowPad - contentH;

    state.framebufferH = contentH;
    const bool isDisconnected = session->disconnected.load();
    const UsbInfo *usbInfo = nullptr;
    const auto usbIt = !session->serialNumber.empty() ? usbTopology.deviceMap.find(session->serialNumber) : usbTopology.deviceMap.end();
    if(usbIt != usbTopology.deviceMap.end()) {
        usbInfo = &usbIt->second;
    }
    const int controllerSharedCount = usbInfo ? (controllerUsage.count(usbInfo->controllerId) ? controllerUsage.at(usbInfo->controllerId) : 0) : 0;
    Viewport vpRgb{contentX, contentY, paneW0, contentH};
    Viewport vpDepth{contentX + paneW0 + kPaneGap, contentY, paneW1, contentH};
    Viewport vpPoint{contentX + paneW0 + paneW1 + (kPaneGap * 2), contentY, paneW2, contentH};

    if(!isDisconnected) {
        vpRgb = drawTexturePane(session->texRgb, contentX, contentY, paneW0, contentH, kPanelAspectRatio);
        // Middle pane: Depth by default, IR when runtime.showIr is toggled on by clicking it.
        const GLuint middleTex = runtime.showIr ? session->texIr : session->texDepth;
        vpDepth = drawTexturePane(middleTex, contentX + paneW0 + kPaneGap, contentY, paneW1, contentH, kPanelAspectRatio);

        if(state.pointMode == PointRenderMode::CpuPoint) {
            if(renderCpuPointPanelImage(state, kCpuPreviewW, kCpuPreviewH, session->cpuPointPreview)) {
                uploadRgbTexture(session->texPointCpu, session->cpuPointPreview, kCpuPreviewW, kCpuPreviewH);
            }
            vpPoint = drawTexturePane(session->texPointCpu, contentX + paneW0 + paneW1 + (kPaneGap * 2), contentY, paneW2, contentH, kPanelAspectRatio);
        } else {
            vpPoint = drawPointPane(state, contentX + paneW0 + paneW1 + (kPaneGap * 2), contentY, paneW2, contentH, kPanelAspectRatio);
        }
    }

    {
        ImFont *fontL = runtime.fontLarge  ? runtime.fontLarge  : ImGui::GetFont();
        ImFont *fontN = runtime.fontNormal ? runtime.fontNormal : ImGui::GetFont();
        ImFont *fontS = runtime.fontSmall  ? runtime.fontSmall  : ImGui::GetFont();
        // Draw device header + pane labels on the background draw list so that
        // ImGui windows (notably tooltips) render ON TOP of them instead of being covered.
        ImDrawList *dl = ImGui::GetBackgroundDrawList();

        // Shows a wrapped tooltip if the mouse is hovering the given screen-space rect.
        // Used to attach HELP text to manually-drawn device header / pane labels.
        // Suppressed while any mouse button is down so it does not interrupt
        // interactive point-cloud drag / pan / zoom.
        const auto hoverTip = [&](const ImVec2 &tl, const ImVec2 &br, const char *tip) {
            if(!tip || !tip[0]) return;
            if(ImGui::IsAnyMouseDown()) return;
            if(ImGui::IsMouseHoveringRect({tl.x - 2.0f, tl.y - 2.0f},
                                          {br.x + 2.0f, br.y + 2.0f}, false)) {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24.0f);
                ImGui::TextUnformatted(tip);
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
        };
        // AddText at `pos` plus a hover-rect tooltip sized to the drawn text.
        const auto addTextTip = [&](ImFont *f, float sz, ImVec2 pos, ImU32 col,
                                     const char *text, const char *tip) {
            dl->AddText(f, sz, pos, col, text);
            ImVec2 ts = f->CalcTextSizeA(sz, FLT_MAX, 0.0f, text);
            hoverTip(pos, {pos.x + ts.x, pos.y + ts.y}, tip);
        };

        // ---- Session header background ----
        const float hScreenY = (float)(runtime.framebufferH - slotY - slotH);
        const ImVec2 hTL = {(float)slotX,          hScreenY};
        const ImVec2 hBR = {(float)(slotX + slotW), hScreenY + (float)kRowHeaderH};
        dl->AddRectFilled(hTL, hBR, IM_COL32(12, 15, 22, 235));
        dl->AddLine({hTL.x, hBR.y - 1}, {hBR.x, hBR.y - 1}, IM_COL32(40, 46, 60, 255));

        float hx = hTL.x + 14.0f;
        float hy = hTL.y + 10.0f;
        const float lhL = 38.0f;
        const float lhS = 28.0f;
        // Three columns for the second row (USB | IMU | TEMP). IMU at 36% and
        // TEMP at 66% keep the device name row readable and give each column
        // enough room at typical widths.
        const float col2x = hTL.x + (float)slotW * 0.36f;
        const float col3x = hTL.x + (float)slotW * 0.66f;

        // Row 1: Device N  SN: xxx  [● LIVE / ● DISC]
        {
            std::string devStr = "Device " + std::to_string(session->deviceIndex);
            ImVec2 devSz = fontL->CalcTextSizeA(30.0f, FLT_MAX, 0.0f, devStr.c_str());
            dl->AddText(fontL, 30.0f, {hx, hy}, IM_COL32(255, 228, 90, 255), devStr.c_str());
            float devRight = hx + devSz.x;
            if(!session->serialNumber.empty()) {
                std::string snStr = "SN: " + session->serialNumber;
                ImVec2 snSz = fontS->CalcTextSizeA(20.0f, FLT_MAX, 0.0f, snStr.c_str());
                dl->AddText(fontS, 20.0f, {hx + devSz.x + 14.0f, hy + 9.0f}, IM_COL32(150, 150, 165, 255), snStr.c_str());
                devRight = hx + devSz.x + 14.0f + snSz.x;
            }
            // Device + SN → TipDevIndex
            hoverTip({hx, hy}, {devRight, hy + devSz.y}, i18n::L(i18n::S::TipDevIndex));

            const char *statusStr  = isDisconnected ? i18n::L(i18n::S::DevDisc) : i18n::L(i18n::S::DevLive);
            const ImU32 statusCol  = isDisconnected ? IM_COL32(240, 60, 60, 255) : IM_COL32(50, 220, 70, 255);
            ImVec2 stSz = fontS->CalcTextSizeA(20.0f, FLT_MAX, 0.0f, statusStr);
            const ImVec2 stPos = {hBR.x - stSz.x - 14.0f, hy + 9.0f};
            dl->AddText(fontS, 20.0f, stPos, statusCol, statusStr);
            hoverTip(stPos, {stPos.x + stSz.x, stPos.y + stSz.y}, i18n::L(i18n::S::TipDevStatus));
        }
        hy += lhL;

        // Row 2: USB | IMU | TEMP on a single line (3 columns).
        {
            std::string usbStr;
            if(usbInfo) {
                std::ostringstream uss;
                uss << "USB: " << formatControllerDisplayName(usbTopology, usbInfo->controllerId, normalizeUsbControllerName(usbInfo->controllerName));
                if(controllerSharedCount > 1) uss << i18n::L(i18n::S::DevSharedSuffix) << controllerSharedCount;
                usbStr = uss.str();
            } else {
                usbStr = i18n::L(i18n::S::DevUsbMissing);
            }
            addTextTip(fontS, 20.0f, {hx, hy}, IM_COL32(200, 200, 200, 255), usbStr.c_str(), i18n::L(i18n::S::TipDevUsb));

            OBAccelValue accel; bool imuOk;
            { std::lock_guard<std::mutex> g(session->imuMutex); accel = session->lastAccel; imuOk = session->imuReady; }
            std::string imuStr;
            if(imuOk) {
                std::ostringstream imuSS;
                imuSS << "IMU X:" << std::fixed << std::setprecision(2) << accel.x
                      << " Y:" << accel.y << " Z:" << accel.z;
                imuStr = imuSS.str();
            } else {
                imuStr = i18n::L(i18n::S::DevImuWaiting);
            }
            addTextTip(fontS, 20.0f, {col2x, hy}, IM_COL32(170, 170, 180, 255), imuStr.c_str(), i18n::L(i18n::S::TipDevImu));

            float cpuT, irT, ldmT; bool tempOk;
            { std::lock_guard<std::mutex> g(session->tempMutex); cpuT = session->cpuTemp; irT = session->irTemp; ldmT = session->ldmTemp; tempOk = session->tempReady; }
            std::string tempStr;
            if(tempOk) {
                std::ostringstream tempSS;
                tempSS << "TEMP CPU:" << std::fixed << std::setprecision(1) << cpuT
                       << " IR:" << irT << " LDM:" << ldmT;
                tempStr = tempSS.str();
            } else {
                tempStr = i18n::L(i18n::S::DevTempNoData);
            }
            addTextTip(fontS, 20.0f, {col3x, hy}, IM_COL32(170, 170, 180, 255), tempStr.c_str(), i18n::L(i18n::S::TipDevTemp));
        }

        // ---- Pane labels (top-left of each pane) ----
        // A semi-transparent dark pill is drawn behind the text so that labels stay
        // readable regardless of what the camera is showing (bright window, white
        // walls, etc.). Hover detection covers the whole pane viewport.
        constexpr float kLabelFontSize = 22.0f;
        constexpr float kLabelPadX = 7.0f;
        constexpr float kLabelPadY = 3.0f;
        constexpr float kLabelRound = 4.0f;
        const ImU32 kLabelBg = IM_COL32(0, 0, 0, 170);
        const auto drawLabelPill = [&](float px, float py, const char *text, ImU32 textCol) {
            const ImVec2 ts = fontS->CalcTextSizeA(kLabelFontSize, FLT_MAX, 0.0f, text);
            const ImVec2 tl{px - kLabelPadX, py - kLabelPadY};
            const ImVec2 br{px + ts.x + kLabelPadX, py + ts.y + kLabelPadY};
            dl->AddRectFilled(tl, br, kLabelBg, kLabelRound);
            dl->AddText(fontS, kLabelFontSize, {px, py}, textCol, text);
        };
        const auto paneLabel = [&](const Viewport &vp, const std::string &txt, ImU32 col, const char *tip) {
            const float px = (float)vp.x + 10.0f;
            const float py = (float)(runtime.framebufferH - vp.y - vp.h) + 8.0f;
            drawLabelPill(px, py, txt.c_str(), col);
            const float paneTop    = (float)(runtime.framebufferH - vp.y - vp.h);
            const float paneBottom = (float)(runtime.framebufferH - vp.y);
            hoverTip({(float)vp.x, paneTop}, {(float)(vp.x + vp.w), paneBottom}, tip);
        };

        std::ostringstream label1;
        label1 << "RGB ";
        if(isDisconnected) { label1 << "--x-- --"; }
        else { label1 << state.colorW << "x" << state.colorH << " " << state.colorFmt; }
        label1 << "  FPS " << std::fixed << std::setprecision(1) << session->fpsColor.fps;
        paneLabel(vpRgb, label1.str(), IM_COL32(255, 255, 255, 255), i18n::L(i18n::S::TipPaneRgb));

        std::ostringstream label2;
        const bool middleIsIr = runtime.showIr;
        label2 << (middleIsIr ? "IR " : "DEPTH ");
        if(isDisconnected) { label2 << "--x-- --"; }
        else if(middleIsIr) {
            label2 << session->streamSettings.depthW << "x" << session->streamSettings.depthH << " " << state.irFmt;
        } else {
            label2 << session->streamSettings.depthW << "x" << session->streamSettings.depthH << " " << state.depthFmt;
        }
        label2 << "  FPS " << std::fixed << std::setprecision(1)
               << (middleIsIr ? session->fpsIr.fps : session->fpsDepth.fps);
        paneLabel(vpDepth, label2.str(), IM_COL32(255, 255, 255, 255),
                  i18n::L(middleIsIr ? i18n::S::TipPaneIr : i18n::S::TipPaneDepth));

        // Click anywhere on the middle pane to toggle Depth ⇄ IR.
        {
            const float paneTop    = (float)(runtime.framebufferH - vpDepth.y - vpDepth.h);
            const float paneBottom = (float)(runtime.framebufferH - vpDepth.y);
            const ImVec2 tl{(float)vpDepth.x, paneTop};
            const ImVec2 br{(float)(vpDepth.x + vpDepth.w), paneBottom};
            if(ImGui::IsMouseHoveringRect(tl, br, false) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                runtime.showIr = !runtime.showIr;
            }
        }

        std::ostringstream label3;
        if(isDisconnected)                                          { label3 << "POINT [XYZRGB]"; }
        else if(state.pointMode == PointRenderMode::GpuMesh)       { label3 << "MESH [XYZRGB]"; }
        else if(state.pointMode == PointRenderMode::GpuPoint)      { label3 << "POINT [XYZRGB]"; }
        else                                                        { label3 << "CPU POINT [XYZRGB]"; }
        paneLabel(vpPoint, label3.str(), IM_COL32(255, 255, 255, 255), i18n::L(i18n::S::TipPanePoint));

        std::ostringstream ptStat;
        ptStat << "pts " << session->latestPoints << "  FPS " << std::fixed << std::setprecision(1) << session->fpsPoint.fps;
        {
            const float px = (float)vpPoint.x + 10.0f;
            const float py = (float)(runtime.framebufferH - vpPoint.y - vpPoint.h) + 8.0f + kLabelFontSize + (kLabelPadY * 2) + 4.0f;
            drawLabelPill(px, py, ptStat.str().c_str(), IM_COL32(230, 230, 255, 255));
        }

        // ---- Disconnection overlay messages ----
        if(isDisconnected) {
            drawFilledRect(vpRgb, 0.10f, 0.10f, 0.10f, 0.58f);
            drawFilledRect(vpDepth, 0.10f, 0.10f, 0.10f, 0.58f);
            drawFilledRect(vpPoint, 0.10f, 0.10f, 0.10f, 0.58f);
            const auto discMsg = [&](const Viewport &vp, const char *l1, const char *l2, const char *l3) {
                const float cx = (float)vp.x + (float)vp.w * 0.5f;
                const float cy = (float)(runtime.framebufferH - vp.y) - (float)vp.h * 0.5f;
                auto sz1 = fontN->CalcTextSizeA(23.0f, FLT_MAX, 0.0f, l1);
                auto sz2 = fontS->CalcTextSizeA(20.0f, FLT_MAX, 0.0f, l2);
                auto sz3 = fontS->CalcTextSizeA(20.0f, FLT_MAX, 0.0f, l3);
                dl->AddText(fontN, 23.0f, {cx - sz1.x * 0.5f, cy - 36.0f}, IM_COL32(240, 240, 240, 255), l1);
                dl->AddText(fontS, 20.0f, {cx - sz2.x * 0.5f, cy - 4.0f},  IM_COL32(200, 200, 200, 255), l2);
                dl->AddText(fontS, 20.0f, {cx - sz3.x * 0.5f, cy + 16.0f}, IM_COL32(175, 175, 175, 255), l3);
            };
            discMsg(vpRgb,   i18n::L(i18n::S::DevDisconnected), i18n::L(i18n::S::DevCameraUnplugged), i18n::L(i18n::S::DevReconnectsAuto));
            discMsg(vpDepth, i18n::L(i18n::S::DevDisconnected), i18n::L(i18n::S::DevDepthStopped),  i18n::L(i18n::S::DevWaitingReconnect));
            discMsg(vpPoint, i18n::L(i18n::S::DevDisconnected), i18n::L(i18n::S::DevPointStopped),  i18n::L(i18n::S::DevWaitingReconnect));
        }
    }
}

void renderSidebar(AppRuntime &runtime) {
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoFocusOnAppearing;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(4, 3));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.04f, 0.06f, 0.97f));
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)kSidebarW, (float)runtime.framebufferH));

    ImGui::Begin("##sidebar", nullptr, flags);

    ImFont *fontL = runtime.fontLarge  ? runtime.fontLarge  : ImGui::GetFont();
    ImFont *fontN = runtime.fontNormal ? runtime.fontNormal : ImGui::GetFont();
    ImFont *fontS = runtime.fontSmall  ? runtime.fontSmall  : ImGui::GetFont();

    // Helpers and colors used by multiple sections
    const ImVec4 kSectionHeaderCol = ImVec4(0.38f, 0.88f, 0.52f, 1.0f);
    const auto drawSectionSeparator = []() {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    };
    const auto wrapText = [](const std::string &text, size_t maxChars) {
        std::vector<std::string> lines;
        std::istringstream stream(text);
        std::string word, line;
        while(stream >> word) {
            std::string candidate = line.empty() ? word : (line + " " + word);
            if(!line.empty() && candidate.size() > maxChars) {
                lines.push_back(line);
                line = word;
            } else {
                line = candidate;
            }
        }
        if(!line.empty()) lines.push_back(line);
        if(lines.empty()) lines.push_back(text);
        return lines;
    };

    // Topology snapshots are used by both USB TOPOLOGY section and by the
    // session slot renderer (via the caller), so grab once.
    const SystemUsbTopology usbTopology       = snapshotUsbTopology(runtime);
    const auto              controllerUsage   = snapshotControllerUsage(runtime);

    //
    // ===== Title =====
    //
    ImGui::PushFont(fontL);
    ImGui::TextColored(ImVec4(0.92f, 0.92f, 0.97f, 1.0f), "STM2 Femto Bolt Viewer");
    ImGui::PopFont();

    const double renderFps = runtime.sessions.empty() ? 0.0 : runtime.sessions.front()->fpsLog.fps;
    ImGui::PushFont(fontS);
    ImGui::Text("%.0f FPS", renderFps);
    tooltipOnHover(i18n::L(i18n::S::TipRenderFps));
    ImGui::SameLine();
    ImGui::TextDisabled("%s", i18n::L(i18n::S::SidebarMonitor));
    ImGui::PopFont();

    //
    // ===== Language switcher (JA / EN / KO) =====
    //
    ImGui::Spacing();
    {
        const i18n::Lang current = i18n::getLang();
        const float avail = ImGui::GetContentRegionAvail().x;
        const ImVec2 btnSize = ImVec2((avail - 12.0f) / 3.0f, 28.0f);

        ImGui::PushFont(fontS);
        if(langPillButton(i18n::langLabel(i18n::Lang::Japanese), current == i18n::Lang::Japanese, btnSize)) {
            i18n::setLang(i18n::Lang::Japanese);
        }
        tooltipOnHover(i18n::L(i18n::S::TipLang));
        ImGui::SameLine();
        if(langPillButton(i18n::langLabel(i18n::Lang::English), current == i18n::Lang::English, btnSize)) {
            i18n::setLang(i18n::Lang::English);
        }
        tooltipOnHover(i18n::L(i18n::S::TipLang));
        ImGui::SameLine();
        if(langPillButton(i18n::langLabel(i18n::Lang::Korean), current == i18n::Lang::Korean, btnSize)) {
            i18n::setLang(i18n::Lang::Korean);
        }
        tooltipOnHover(i18n::L(i18n::S::TipLang));
        ImGui::PopFont();
    }

    drawSectionSeparator();

    //
    // ===== VIEW (display controls) =====
    //
    ImGui::TextColored(kSectionHeaderCol, "%s", i18n::L(i18n::S::SectionView));
    {
        const PointRenderMode currentMode = runtime.sessions.empty()
            ? PointRenderMode::GpuMesh
            : runtime.sessions.front()->viewState.pointMode;
        std::string modeLabel = std::string(i18n::L(i18n::S::ViewModePrefix)) + toPointModeText(currentMode);
        if(ImGui::Button(modeLabel.c_str(), ImVec2(-1, 36))) {
            cycleAllSessionsPointMode(runtime);
        }
        tooltipOnHover(i18n::L(i18n::S::TipViewMode));
        ImGui::PushFont(fontS);
        ImGui::TextDisabled("%s", i18n::L(i18n::S::ViewModeHint));
        ImGui::PopFont();

        ImGui::Spacing();

        if(ImGui::Button(i18n::L(i18n::S::ViewResetBtn), ImVec2(-1, 36))) {
            resetAllSessionsView(runtime);
        }
        tooltipOnHover(i18n::L(i18n::S::TipViewReset));
        ImGui::PushFont(fontS);
        ImGui::TextDisabled("%s", i18n::L(i18n::S::ViewResetHint));
        ImGui::PopFont();

        ImGui::Spacing();

        const char *vsyncLabel = runtime.vsync ? i18n::L(i18n::S::ViewVsyncOn)
                                               : i18n::L(i18n::S::ViewVsyncOff);
        if(ImGui::Button(vsyncLabel, ImVec2(-1, 28))) {
            runtime.vsync = !runtime.vsync;
            glfwSwapInterval(runtime.vsync ? 1 : 0);
        }
        tooltipOnHover(i18n::L(i18n::S::TipVsync));
        ImGui::PushFont(fontS);
        ImGui::TextDisabled("%s", i18n::L(i18n::S::ViewVsyncHint));
        ImGui::PopFont();
    }

    drawSectionSeparator();

    //
    // ===== STREAM (sensor resolution / fps presets) =====
    //
    ImGui::TextColored(kSectionHeaderCol, "%s", i18n::L(i18n::S::SectionStream));
    {
        static const std::pair<int,int> kDepthPresets[] = {
            {320, 288}, {640, 576}, {512, 512}, {1024, 1024}
        };
        static const std::pair<int,int> kColorPresets[] = {
            {640, 480}, {1280, 720}, {1920, 1080}
        };
        static const int kFpsPresets[] = { 5, 15, 30 };

        const auto nextIndex = [](const auto *arr, size_t n, int curW, int curH) -> size_t {
            for(size_t i = 0; i < n; ++i) {
                if(arr[i].first == curW && arr[i].second == curH) return (i + 1) % n;
            }
            return 0;
        };
        const auto nextFpsIndex = [](int curFps) -> size_t {
            constexpr size_t n = sizeof(kFpsPresets)/sizeof(int);
            for(size_t i = 0; i < n; ++i) {
                if(kFpsPresets[i] == curFps) return (i + 1) % n;
            }
            return 0;
        };

        StreamSettings &s = runtime.streamSettings;

        char colorLabel[96];
        std::snprintf(colorLabel, sizeof(colorLabel), "%s%d x %d",
                      i18n::L(i18n::S::StreamColorPrefix), s.colorW, s.colorH);
        if(ImGui::Button(colorLabel, ImVec2(-1, 36))) {
            size_t idx = nextIndex(kColorPresets, 3, s.colorW, s.colorH);
            s.colorW = kColorPresets[idx].first;
            s.colorH = kColorPresets[idx].second;
            applyStreamSettingsToAllSessions(runtime);
        }
        tooltipOnHover(i18n::L(i18n::S::TipStreamColor));

        ImGui::Spacing();

        // When the user picks 1024x1024 depth we also force FPS to 15 because
        // Femto Bolt's WFOV unbinned mode does not support higher frame rates.
        // The modal below informs the user of the automatic FPS change.
        static bool openFps15Modal = false;

        char depthLabel[96];
        std::snprintf(depthLabel, sizeof(depthLabel), "%s%d x %d",
                      i18n::L(i18n::S::StreamDepthPrefix), s.depthW, s.depthH);
        if(ImGui::Button(depthLabel, ImVec2(-1, 36))) {
            size_t idx = nextIndex(kDepthPresets, 4, s.depthW, s.depthH);
            s.depthW = kDepthPresets[idx].first;
            s.depthH = kDepthPresets[idx].second;
            if(s.depthW == 1024 && s.depthH == 1024 && s.fps != 15) {
                s.fps = 15;
                openFps15Modal = true;
            }
            applyStreamSettingsToAllSessions(runtime);
        }
        tooltipOnHover(i18n::L(i18n::S::TipStreamDepth));

        if(openFps15Modal) {
            ImGui::OpenPopup("##fps15_required");
            openFps15Modal = false;
        }
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if(ImGui::BeginPopupModal(i18n::L(i18n::S::Depth1024ModalTitle), nullptr,
                                  ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::TextUnformatted(i18n::L(i18n::S::Depth1024ModalMessage));
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            const float w = ImGui::GetContentRegionAvail().x;
            const float btnW = 120.0f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (w - btnW) * 0.5f);
            if(ImGui::Button(i18n::L(i18n::S::ModalOkButton), ImVec2(btnW, 0)) ||
               ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Spacing();

        char fpsLabel[48];
        std::snprintf(fpsLabel, sizeof(fpsLabel), "%s%d", i18n::L(i18n::S::StreamFpsPrefix), s.fps);
        if(ImGui::Button(fpsLabel, ImVec2(-1, 36))) {
            size_t idx = nextFpsIndex(s.fps);
            s.fps = kFpsPresets[idx];
            applyStreamSettingsToAllSessions(runtime);
        }
        tooltipOnHover(i18n::L(i18n::S::TipStreamFps));

        ImGui::PushFont(fontS);
        ImGui::TextDisabled("%s", i18n::L(i18n::S::StreamPresetHint));
        ImGui::PopFont();
    }

    drawSectionSeparator();

    //
    // ===== USB TOPOLOGY (which camera is on which controller) =====
    //
    ImGui::TextColored(kSectionHeaderCol, "%s", i18n::L(i18n::S::SectionUsbTopology));
    tooltipOnHover(i18n::L(i18n::S::TipUsbTopology));
    ImGui::PushFont(fontS);
    for(const auto &controllerId : usbTopology.controllers) {
        const auto nameIt = usbTopology.controllerNames.find(controllerId);
        const std::string rawName = (nameIt != usbTopology.controllerNames.end()) ? nameIt->second : "Unknown Controller";
        const std::string label = formatControllerDisplayName(usbTopology, controllerId, normalizeUsbControllerName(rawName));
        const int usage = controllerUsage.count(controllerId) ? controllerUsage.at(controllerId) : 0;
        const bool shared = usage > 1;
        const bool empty  = usage == 0;
        const ImVec4 ctrlColor = shared
            ? ImVec4(1.0f, 0.35f, 0.35f, 1.0f)
            : ImVec4(0.72f, 0.92f, 0.92f, 1.0f);
        for(const auto &ln : wrapText(label, 24)) {
            ImGui::TextColored(ctrlColor, "%s", ln.c_str());
        }
        if(empty) {
            ImGui::TextDisabled("%s", i18n::L(i18n::S::UsbEmpty));
        } else {
            for(const auto &sess : runtime.sessions) {
                if(!sess || sess->serialNumber.empty()) continue;
                const auto usbIt = usbTopology.deviceMap.find(sess->serialNumber);
                if(usbIt == usbTopology.deviceMap.end()) continue;
                if(usbIt->second.controllerId != controllerId) continue;
                ImGui::TextColored(ImVec4(0.55f, 1.0f, 0.55f, 1.0f), "  > Device %d", sess->deviceIndex);
            }
        }
        if(shared) {
            for(const auto &ln : wrapText(i18n::L(i18n::S::UsbShared), 22)) {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", ln.c_str());
            }
        }
        ImGui::Spacing();
    }
    ImGui::PopFont();

    drawSectionSeparator();

    //
    // ===== GPU (OpenGL vendor/renderer/version) =====
    //
    ImGui::TextColored(kSectionHeaderCol, "%s", i18n::L(i18n::S::SectionGpu));
    tooltipOnHover(i18n::L(i18n::S::TipGpu));
    {
        const auto clip = [](const std::string &s, size_t mx) -> std::string {
            return s.size() > mx ? s.substr(0, mx - 2) + ".." : s;
        };
        ImGui::PushFont(fontS);
        ImGui::TextColored(ImVec4(0.80f, 0.80f, 0.90f, 1.0f), "%s", clip(runtime.glRenderer, 28).c_str());
        ImGui::TextDisabled("%s", clip(runtime.glVendor,   28).c_str());
        ImGui::TextDisabled("%s", clip(runtime.glVersion,  28).c_str());
        ImGui::PopFont();
    }

    // Bottom-aligned recovery panel
    {
        constexpr float kResetPanelH = 240.0f;
        const float bottomY = ImGui::GetWindowHeight() - kResetPanelH - 10.0f;
        if(ImGui::GetCursorPosY() < bottomY) {
            ImGui::SetCursorPosY(bottomY);
        }
        ImGui::Separator();
        ImGui::Spacing();

        const bool busy = isUsbResetBusy();

        ImGui::TextColored(ImVec4(0.95f, 0.60f, 0.30f, 1.0f), "%s", i18n::L(i18n::S::SectionRecovery));
        ImGui::Spacing();

        if(busy) ImGui::BeginDisabled();
        if(ImGui::Button(busy ? i18n::L(i18n::S::RecoveryUsbResetBusy) : i18n::L(i18n::S::RecoveryUsbResetBtn), ImVec2(-1, 38))) {
            requestAllUsbHostReset();
        }
        tooltipOnHover(i18n::L(i18n::S::TipUsbReset));
        if(busy) ImGui::EndDisabled();
        ImGui::PushFont(fontS);
        ImGui::TextDisabled("%s", i18n::L(i18n::S::RecoveryUsbResetHint));
        ImGui::PopFont();

        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.35f, 0.20f, 0.10f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.50f, 0.30f, 0.15f, 1.0f));
        if(ImGui::Button(i18n::L(i18n::S::RecoveryRestartBtn), ImVec2(-1, 38))) {
            restartApplication();
        }
        ImGui::PopStyleColor(2);
        tooltipOnHover(i18n::L(i18n::S::TipRestart));
        ImGui::PushFont(fontS);
        ImGui::TextDisabled("%s", i18n::L(i18n::S::RecoveryRestartHint));
        ImGui::PopFont();
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}
