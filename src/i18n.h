#pragma once

namespace i18n {

enum class Lang {
    Japanese = 0,
    English  = 1,
    Korean   = 2,
};

enum class S {
    // Sidebar structure
    SidebarMonitor,
    SectionView,
    SectionStream,
    SectionUsbTopology,
    SectionGpu,
    SectionRecovery,

    // VIEW section
    ViewModePrefix,
    ViewModeHint,
    ViewResetBtn,
    ViewResetHint,
    ViewVsyncOn,
    ViewVsyncOff,
    ViewVsyncHint,

    // STREAM section
    StreamDepthPrefix,
    StreamColorPrefix,
    StreamFpsPrefix,
    StreamPresetHint,

    // Modal: Depth 1024x1024 forces FPS=15
    Depth1024ModalTitle,
    Depth1024ModalMessage,
    ModalOkButton,

    // USB TOPOLOGY
    UsbEmpty,
    UsbShared,

    // RECOVERY
    RecoveryUsbResetBtn,
    RecoveryUsbResetBusy,
    RecoveryUsbResetHint,
    RecoveryRestartBtn,
    RecoveryRestartHint,

    // Tooltips (may include \n for multi-line)
    TipLang,
    TipRenderFps,
    TipViewMode,
    TipViewReset,
    TipVsync,
    TipStreamDepth,
    TipStreamColor,
    TipStreamFps,
    TipUsbTopology,
    TipGpu,
    TipUsbReset,
    TipRestart,

    // Device panel header
    DevLive,
    DevDisc,
    DevUsbMissing,
    DevImuWaiting,
    DevTempNoData,
    DevSharedSuffix,

    // Device panel tooltips
    TipDevIndex,
    TipDevStatus,
    TipDevUsb,
    TipDevImu,
    TipDevFps,
    TipDevTemp,
    TipDevRes,
    TipDevPts,
    TipPaneRgb,
    TipPaneDepth,
    TipPaneIr,
    TipPanePoint,

    // Device panel disconnection overlay
    DevDisconnected,
    DevCameraUnplugged,
    DevReconnectsAuto,
    DevDepthStopped,
    DevPointStopped,
    DevWaitingReconnect,
    DevRecoverHint,

    _Count
};

Lang getLang();
void setLang(Lang l);
const char *L(S key);

const char *langCode(Lang l);     // "ja" | "en" | "ko"
Lang        langFromCode(const char *code);
const char *langLabel(Lang l);    // "日本語" | "English" | "한국어"

} // namespace i18n
