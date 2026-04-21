#include "i18n.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace i18n {

namespace {

Lang g_lang = Lang::Japanese;

// Rows: StringId. Columns: [JA, EN, KO].
// Tooltip strings may contain \n for multi-line.
const char *kStrings[(int)S::_Count][3] = {
    // ---- Sidebar structure ----
    /* SidebarMonitor       */ {u8"Monitor",              u8"Monitor",                  u8"모니터"},
    /* SectionView          */ {u8"VIEW",                 u8"VIEW",                     u8"VIEW"},
    /* SectionStream        */ {u8"STREAM",               u8"STREAM",                   u8"STREAM"},
    /* SectionUsbTopology   */ {u8"USB TOPOLOGY",         u8"USB TOPOLOGY",             u8"USB TOPOLOGY"},
    /* SectionGpu           */ {u8"GPU",                  u8"GPU",                      u8"GPU"},
    /* SectionRecovery      */ {u8"RECOVERY",             u8"RECOVERY",                 u8"RECOVERY"},

    // ---- VIEW ----
    /* ViewModePrefix       */ {u8"表示モード: ",          u8"Mode: ",                   u8"모드: "},
    /* ViewModeHint         */ {u8"MESH → POINT → CPU POINT  (M)",
                                u8"MESH → POINT → CPU POINT  (M)",
                                u8"MESH → POINT → CPU POINT  (M)"},
    /* ViewResetBtn         */ {u8"視点をリセット",        u8"Reset View",               u8"시점 초기화"},
    /* ViewResetHint        */ {u8"角度・ZOOM・パン 初期化  (R)",
                                u8"angle / zoom / pan  (R)",
                                u8"각도 · 줌 · 팬 초기화  (R)"},

    // ---- STREAM ----
    /* StreamDepthPrefix    */ {u8"Depth: ",              u8"Depth: ",                  u8"Depth: "},
    /* StreamColorPrefix    */ {u8"Color: ",              u8"Color: ",                  u8"Color: "},
    /* StreamFpsPrefix      */ {u8"FPS: ",                u8"FPS: ",                    u8"FPS: "},
    /* StreamPresetHint     */ {u8"クリックで次のプリセット / 全カメラ同時",
                                u8"click to cycle preset / applied to all cameras",
                                u8"클릭하여 다음 프리셋 / 모든 카메라 동시"},

    // ---- USB TOPOLOGY ----
    /* UsbEmpty             */ {u8"  (Empty)",            u8"  (Empty)",                u8"  (비어 있음)"},
    /* UsbShared            */ {u8"shared by multiple cameras",
                                u8"shared by multiple cameras",
                                u8"여러 카메라가 공유 중"},

    // ---- RECOVERY ----
    /* RecoveryUsbResetBtn  */ {u8"USBをリセット",         u8"Reset USB",                u8"USB 리셋"},
    /* RecoveryUsbResetBusy */ {u8"処理中...",             u8"Working...",               u8"처리 중..."},
    /* RecoveryUsbResetHint */ {u8"カメラが認識されない時に",
                                u8"when a camera is not detected",
                                u8"카메라가 인식되지 않을 때"},
    /* RecoveryRestartBtn   */ {u8"アプリを再起動",        u8"Restart App",              u8"앱 재시작"},
    /* RecoveryRestartHint  */ {u8"USBリセット後はこれを押す",
                                u8"press this after USB reset",
                                u8"USB 리셋 후 이 버튼을 누르세요"},

    // ---- Tooltips ----
    /* TipLang              */ {u8"UI言語を切り替えます\n日本語 / English / 한국어",
                                u8"Switch UI language\n日本語 / English / 한국어",
                                u8"UI 언어 변경\n日本語 / English / 한국어"},
    /* TipViewMode          */ {u8"点群の描画モードを切替\n"
                                u8"GPU MESH:  三角形メッシュ (詳細・重め)\n"
                                u8"GPU POINT: 点群 (軽量)\n"
                                u8"CPU POINT: OpenGL非対応環境向け\n"
                                u8"ショートカット: M キー",
                                u8"Cycle point-cloud render mode\n"
                                u8"GPU MESH:  triangle mesh (detailed)\n"
                                u8"GPU POINT: point cloud (lightweight)\n"
                                u8"CPU POINT: fallback for no-GL environments\n"
                                u8"Shortcut: M key",
                                u8"포인트 클라우드 렌더링 모드 전환\n"
                                u8"GPU MESH:  삼각형 메시 (고품질)\n"
                                u8"GPU POINT: 점 클라우드 (경량)\n"
                                u8"CPU POINT: OpenGL 미지원 환경용\n"
                                u8"단축키: M 키"},
    /* TipViewReset         */ {u8"3Dビューの角度・ズーム・パンを初期状態に戻します\nショートカット: R キー",
                                u8"Reset 3D view angle / zoom / pan\nShortcut: R key",
                                u8"3D 뷰의 각도 · 줌 · 팬을 초기 상태로 되돌립니다\n단축키: R 키"},
    /* TipStreamDepth       */ {u8"Depth ストリーム解像度を切替\n"
                                u8"320x288 → 640x576 → 512x512 → 1024x1024\n"
                                u8"全カメラに同じ設定が適用され、再接続されます",
                                u8"Cycle depth stream resolution\n"
                                u8"320x288 → 640x576 → 512x512 → 1024x1024\n"
                                u8"Applied to all cameras (stream restarts)",
                                u8"Depth 스트림 해상도 전환\n"
                                u8"320x288 → 640x576 → 512x512 → 1024x1024\n"
                                u8"모든 카메라에 적용 (스트림 재시작)"},
    /* TipStreamColor       */ {u8"Color ストリーム解像度を切替\n"
                                u8"640x480 → 1280x720 → 1920x1080\n"
                                u8"全カメラに同じ設定が適用され、再接続されます",
                                u8"Cycle color stream resolution\n"
                                u8"640x480 → 1280x720 → 1920x1080\n"
                                u8"Applied to all cameras (stream restarts)",
                                u8"Color 스트림 해상도 전환\n"
                                u8"640x480 → 1280x720 → 1920x1080\n"
                                u8"모든 카메라에 적용 (스트림 재시작)"},
    /* TipStreamFps         */ {u8"センサーのフレームレートを切替\n5 → 15 → 30\n全カメラに同じ設定が適用されます",
                                u8"Cycle sensor frame rate\n5 → 15 → 30\nApplied to all cameras",
                                u8"센서 프레임 레이트 전환\n5 → 15 → 30\n모든 카메라에 적용"},
    /* TipUsbTopology       */ {u8"各カメラがどのUSBコントローラーに接続されているかを表示\n赤字は1つのコントローラーに複数台のカメラが接続されている状態で、帯域不足の原因になります",
                                u8"Shows which USB controller each camera sits on.\nRed text means multiple cameras share a controller — bandwidth is limited.",
                                u8"각 카메라가 어느 USB 컨트롤러에 연결되어 있는지 표시\n빨간색은 하나의 컨트롤러를 여러 카메라가 공유하는 상태로 대역폭 부족의 원인이 됩니다"},
    /* TipGpu               */ {u8"描画に使われているOpenGL実装\nRenderer / Vendor / Version",
                                u8"OpenGL implementation used for rendering\nRenderer / Vendor / Version",
                                u8"렌더링에 사용 중인 OpenGL 구현\nRenderer / Vendor / Version"},
    /* TipUsbReset          */ {u8"USBホストコントローラーを一時的に無効→有効にして、\nカメラが認識されない状態から復旧を試みます\n完了後は「アプリを再起動」を押してください",
                                u8"Briefly disables & re-enables the USB host controller\nto recover from a camera-not-detected state.\nAfter it finishes, press \"Restart App\".",
                                u8"USB 호스트 컨트롤러를 일시적으로 비활성→활성화하여\n카메라가 인식되지 않을 때 복구를 시도합니다\n완료 후 \"앱 재시작\"을 누르세요"},
    /* TipRestart           */ {u8"このアプリを一度終了し、同じ場所で起動し直します\nUSBリセット後のセッション作り直しに使います",
                                u8"Quit this app and re-launch it from the same location\nUseful to rebuild sessions after a USB reset",
                                u8"이 앱을 종료하고 같은 위치에서 다시 시작합니다\nUSB 리셋 후 세션을 다시 만들 때 사용합니다"},

    // ---- Device header ----
    /* DevLive              */ {u8"● LIVE",               u8"● LIVE",                   u8"● LIVE"},
    /* DevDisc              */ {u8"● DISC",               u8"● DISC",                   u8"● DISC"},
    /* DevUsbMissing        */ {u8"USB: -",               u8"USB: -",                   u8"USB: -"},
    /* DevImuWaiting        */ {u8"IMU: Waiting...",      u8"IMU: Waiting...",          u8"IMU: 대기 중..."},
    /* DevTempNoData        */ {u8"TEMP: --",             u8"TEMP: --",                 u8"TEMP: --"},
    /* DevSharedSuffix      */ {u8" / shared ",           u8" / shared ",               u8" / 공유 "},

    // ---- Device panel disconnection overlay ----
    /* DevDisconnected      */ {u8"DISCONNECTED",         u8"DISCONNECTED",             u8"연결 끊김"},
    /* DevCameraUnplugged   */ {u8"カメラが抜けました",     u8"Camera unplugged",         u8"카메라가 분리됨"},
    /* DevReconnectsAuto    */ {u8"自動で再接続します",    u8"Reconnects automatically", u8"자동으로 재연결됩니다"},
    /* DevDepthStopped      */ {u8"DEPTH 停止中",          u8"DEPTH stopped",            u8"DEPTH 정지"},
    /* DevPointStopped      */ {u8"POINT 停止中",          u8"POINT stopped",            u8"POINT 정지"},
    /* DevWaitingReconnect  */ {u8"再接続を待機中",        u8"Waiting reconnect",        u8"재연결 대기 중"},
};

std::string prefFilePath() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if(n == 0 || n >= MAX_PATH) return "settings.ini";
    std::string p(buf, n);
    const size_t sep = p.find_last_of("\\/");
    if(sep == std::string::npos) return "settings.ini";
    return p.substr(0, sep + 1) + "settings.ini";
#else
    return "settings.ini";
#endif
}

Lang langFromCode(const char *code) {
    if(!code) return Lang::Japanese;
    if(std::strcmp(code, "en") == 0) return Lang::English;
    if(std::strcmp(code, "ko") == 0) return Lang::Korean;
    return Lang::Japanese;
}

} // namespace

Lang getLang() { return g_lang; }
void setLang(Lang l) { g_lang = l; }

const char *L(S key) {
    const int idx = (int)key;
    if(idx < 0 || idx >= (int)S::_Count) return "";
    return kStrings[idx][(int)g_lang];
}

const char *langCode(Lang l) {
    switch(l) {
        case Lang::English: return "en";
        case Lang::Korean:  return "ko";
        default:            return "ja";
    }
}
const char *langLabel(Lang l) {
    switch(l) {
        case Lang::English: return "English";
        case Lang::Korean:  return u8"한국어";
        default:            return u8"日本語";
    }
}

void loadPreferenceFromExeDir() {
    const std::string path = prefFilePath();
    std::ifstream f(path);
    if(!f.good()) return;
    std::string line;
    while(std::getline(f, line)) {
        // trim CR
        while(!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) line.pop_back();
        const std::string prefix = "lang=";
        if(line.rfind(prefix, 0) == 0) {
            g_lang = langFromCode(line.c_str() + prefix.size());
            return;
        }
    }
}

void savePreferenceToExeDir() {
    const std::string path = prefFilePath();
    std::ofstream f(path, std::ios::trunc);
    if(!f.good()) return;
    f << "lang=" << langCode(g_lang) << "\n";
}

} // namespace i18n
