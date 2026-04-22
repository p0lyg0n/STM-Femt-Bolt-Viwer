#include "i18n.h"

#include <cstring>

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
    /* ViewVsyncOn          */ {u8"VSync: ON",             u8"VSync: ON",                u8"VSync: ON"},
    /* ViewVsyncOff         */ {u8"VSync: OFF",            u8"VSync: OFF",               u8"VSync: OFF"},
    /* ViewVsyncHint        */ {u8"OFF で応答が軽くなる",  u8"OFF = snappier drag",       u8"OFF로 하면 반응이 빨라짐"},

    // ---- STREAM ----
    /* StreamDepthPrefix    */ {u8"Depth: ",              u8"Depth: ",                  u8"Depth: "},
    /* StreamColorPrefix    */ {u8"Color: ",              u8"Color: ",                  u8"Color: "},
    /* StreamFpsPrefix      */ {u8"FPS: ",                u8"FPS: ",                    u8"FPS: "},
    /* StreamPresetHint     */ {u8"クリックで次のプリセット / 全カメラ同時",
                                u8"click to cycle preset / applied to all cameras",
                                u8"클릭하여 다음 프리셋 / 모든 카메라 동시"},

    // ---- Depth 1024 forces FPS=15 modal ----
    /* Depth1024ModalTitle  */ {u8"FPS を 15 に変更しました",
                                u8"FPS changed to 15",
                                u8"FPS를 15로 변경했습니다"},
    /* Depth1024ModalMessage*/ {u8"Femto Bolt の Depth 1024×1024 は 15 FPS のみ対応です。\nFPS を自動で 15 に変更しました。",
                                u8"Femto Bolt's 1024×1024 depth mode only supports 15 FPS.\nFPS has been set to 15 automatically.",
                                u8"Femto Bolt의 1024×1024 Depth는 15 FPS만 지원합니다.\nFPS를 자동으로 15로 변경했습니다."},
    /* ModalOkButton        */ {u8"OK",                    u8"OK",                       u8"확인"},

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
    /* TipRenderFps         */ {u8"UI の描画フレームレート\nウィンドウ全体が 1 秒に何回描き直されているかを表します\nカメラのフレームレートではありません (それは各カメラの FPS 表示を見てください)\n通常は 60 前後が目安",
                                u8"UI render frame rate\nHow many times per second the whole window is redrawn.\nThis is NOT the camera frame rate — see each camera's FPS label for that.\nTypically hovers around 60.",
                                u8"UI 렌더링 프레임 레이트\n윈도우 전체가 초당 몇 번 다시 그려지는지 나타냅니다\n카메라 프레임 레이트와는 다릅니다 (각 카메라의 FPS 표시를 확인하세요)\n보통 60 전후"},
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
    /* TipVsync             */ {u8"垂直同期 (VSync) の ON/OFF を切替\n"
                                u8"ON: 60Hz でキャップ (標準)。表示は安定\n"
                                u8"OFF: モニタ同期せず描画。点群ドラッグなどの応答が軽くなるが、\n"
                                u8"画面の一部で 1〜2px のティアリング (段差) が出ることがある。\n"
                                u8"GPU 消費電力・温度がわずかに上がる (+10〜25W)。",
                                u8"Toggle vertical sync (VSync).\n"
                                u8"ON: capped to monitor refresh (60Hz). Stable visuals.\n"
                                u8"OFF: snappier mouse drag at the cost of occasional 1-2px tearing.\n"
                                u8"GPU draws a bit more power (+10-25W).",
                                u8"수직 동기화 (VSync) 전환\n"
                                u8"ON: 모니터 주사율에 제한 (60Hz). 표시 안정적.\n"
                                u8"OFF: 마우스 드래그 반응이 빨라지지만 1-2px 티어링이 생길 수 있음.\n"
                                u8"GPU 소비 전력·온도가 조금 증가 (+10-25W)."},
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

    // ---- Device panel tooltips ----
    /* TipDevIndex          */ {u8"カメラの通し番号とシリアル番号\n複数台接続時の識別に使います",
                                u8"Camera index and serial number\nUsed to identify each camera when multiple are connected",
                                u8"카메라 인덱스와 시리얼 번호\n여러 대 연결 시 식별에 사용됩니다"},
    /* TipDevStatus         */ {u8"LIVE: カメラからフレームを受信中\nDISC: 切断または未受信",
                                u8"LIVE: receiving frames from the camera\nDISC: disconnected or no frames",
                                u8"LIVE: 카메라로부터 프레임 수신 중\nDISC: 연결 끊김 또는 수신 없음"},
    /* TipDevUsb            */ {u8"このカメラが接続されているUSBホストコントローラー\n「shared N」は同じコントローラーをN台で共有 (帯域不足の原因)",
                                u8"USB host controller this camera is connected to.\n\"shared N\" means N cameras share this controller (bandwidth bottleneck).",
                                u8"이 카메라가 연결된 USB 호스트 컨트롤러\n\"shared N\"은 같은 컨트롤러를 N대가 공유 (대역폭 병목)"},
    /* TipDevImu            */ {u8"カメラ内蔵の加速度センサー (m/s²)\nX/Y/Z 軸ごとの加速度を表示",
                                u8"Built-in accelerometer (m/s²)\nShows acceleration per X/Y/Z axis",
                                u8"내장 가속도 센서 (m/s²)\nX/Y/Z 축별 가속도 표시"},
    /* TipDevFps            */ {u8"実際にカメラから届いているフレームレート\nColor/Depth が設定値より低い場合は帯域や負荷を疑います",
                                u8"Actual frame rate received from the camera.\nIf Color/Depth is below target, suspect USB bandwidth or CPU load.",
                                u8"카메라로부터 실제로 수신 중인 프레임 레이트\n설정값보다 낮으면 USB 대역 또는 부하를 확인"},
    /* TipDevTemp           */ {u8"カメラ内部温度 (°C)\n"
                                u8"CPU: メイン基板\n"
                                u8"IR : 赤外線受光センサー (撮影側 / Depth の元になる CMOS)\n"
                                u8"LDM: 赤外線レーザー発光モジュール (照射側)",
                                u8"Internal camera temperatures (°C)\n"
                                u8"CPU: main board\n"
                                u8"IR : IR imaging sensor (receiver side / CMOS that produces depth)\n"
                                u8"LDM: Laser Diode Module (IR emitter side)",
                                u8"카메라 내부 온도 (°C)\n"
                                u8"CPU: 메인보드\n"
                                u8"IR : IR 수광 센서 (촬영측 / Depth 생성 CMOS)\n"
                                u8"LDM: 레이저 발광 모듈 (IR 조사측)"},
    /* TipDevRes            */ {u8"現在のストリーム解像度\n左側: Color, 右側: Depth (センサー生出力)",
                                u8"Current stream resolution\nLeft: Color, Right: Depth (raw sensor)",
                                u8"현재 스트림 해상도\n왼쪽: Color, 오른쪽: Depth (센서 원본)"},
    /* TipDevPts            */ {u8"3D 点群の点数と点群描画のFPS\n「pts」はこのフレームで表示された有効点の数",
                                u8"Point-cloud point count and point-render FPS\n\"pts\" is the number of valid points drawn this frame",
                                u8"포인트 클라우드 점 개수와 포인트 렌더 FPS\n\"pts\"는 이 프레임에 표시된 유효 점 개수"},
    /* TipPaneRgb           */ {u8"RGB カラー画像\nカメラから送られてくる可視光カラー映像",
                                u8"RGB color image\nVisible-light color video from the camera",
                                u8"RGB 컬러 영상\n카메라에서 보내는 가시광 컬러 영상"},
    /* TipPaneDepth         */ {u8"深度画像\n近いほど明るい黄色、遠いほど暗い紫で表示\n黒は計測できなかった領域\n\nクリックで IR 画像に切替",
                                u8"Depth image\nNearer = brighter yellow, farther = darker purple\nBlack means no depth measurement\n\nClick to switch to IR",
                                u8"깊이 영상\n가까울수록 밝은 노랑, 멀수록 어두운 보라\n검정은 측정 실패 영역\n\n클릭하면 IR 영상으로 전환"},
    /* TipPaneIr            */ {u8"赤外線 (IR) 画像\n赤外線受光センサー (撮影側) が受けた反射光の強度\n白いほど強く反射。暗所でも LDM が照射しているため見える\n\nクリックで Depth 画像に戻る",
                                u8"Infrared (IR) image\nBrightness of reflected IR captured by the receiver sensor.\nBrighter = stronger reflection. Visible even in the dark because the LDM emits IR.\n\nClick to switch back to Depth",
                                u8"적외선 (IR) 영상\nIR 수광 센서가 받은 반사광 강도.\n밝을수록 반사가 강함. LDM이 조사하므로 어두운 곳에서도 보임.\n\n클릭하면 Depth로 돌아감"},
    /* TipPanePoint         */ {u8"3D 点群表示 (XYZRGB)\n左ドラッグ: 回転 / 右ドラッグ: 平行移動 / ホイール: ズーム\nM キーで MESH/POINT/CPU POINT 切替",
                                u8"3D point cloud (XYZRGB)\nLeft-drag: rotate / Right-drag: pan / Wheel: zoom\nM key cycles MESH/POINT/CPU POINT",
                                u8"3D 포인트 클라우드 (XYZRGB)\n왼쪽 드래그: 회전 / 오른쪽 드래그: 팬 / 휠: 줌\nM 키로 MESH/POINT/CPU POINT 전환"},

    // ---- Device panel disconnection overlay ----
    /* DevDisconnected      */ {u8"DISCONNECTED",         u8"DISCONNECTED",             u8"연결 끊김"},
    /* DevCameraUnplugged   */ {u8"カメラが抜けました",     u8"Camera unplugged",         u8"카메라가 분리됨"},
    /* DevReconnectsAuto    */ {u8"自動で再接続します",    u8"Reconnects automatically", u8"자동으로 재연결됩니다"},
    /* DevDepthStopped      */ {u8"DEPTH 停止中",          u8"DEPTH stopped",            u8"DEPTH 정지"},
    /* DevPointStopped      */ {u8"POINT 停止中",          u8"POINT stopped",            u8"POINT 정지"},
    /* DevWaitingReconnect  */ {u8"再接続を待機中",        u8"Waiting reconnect",        u8"재연결 대기 중"},
    /* DevRecoverHint       */ {u8"戻らない場合は [アプリを再起動] で復旧してください",
                                u8"If it doesn't return, click [Restart App] to recover",
                                u8"복구되지 않으면 [앱 재시작] 버튼을 눌러주세요"},
};

} // namespace

Lang getLang() { return g_lang; }
void setLang(Lang l) { g_lang = l; }

Lang langFromCode(const char *code) {
    if(!code) return Lang::Japanese;
    if(std::strcmp(code, "en") == 0) return Lang::English;
    if(std::strcmp(code, "ko") == 0) return Lang::Korean;
    return Lang::Japanese;
}

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

} // namespace i18n
