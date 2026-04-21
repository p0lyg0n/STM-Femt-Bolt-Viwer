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

    // STREAM section
    StreamDepthPrefix,
    StreamColorPrefix,
    StreamFpsPrefix,
    StreamPresetHint,

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
    TipViewMode,
    TipViewReset,
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

    // Device panel disconnection overlay
    DevDisconnected,
    DevCameraUnplugged,
    DevReconnectsAuto,
    DevDepthStopped,
    DevPointStopped,
    DevWaitingReconnect,

    _Count
};

Lang getLang();
void setLang(Lang l);
const char *L(S key);

const char *langCode(Lang l);   // "ja" | "en" | "ko"
const char *langLabel(Lang l);  // "日本語" | "English" | "한국어"

void loadPreferenceFromExeDir();
void savePreferenceToExeDir();

} // namespace i18n
