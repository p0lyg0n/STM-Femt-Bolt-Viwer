#pragma once

#include "types.h"
#include "usb_topology_format.h"

// ---------------------------------------------------------------------------
// USB topology worker
// ---------------------------------------------------------------------------

void startUsbTopologyWorker(ob::Context &context, AppRuntime &runtime);
void stopUsbTopologyWorker(AppRuntime &runtime);

// ---------------------------------------------------------------------------
// Thread-safe snapshots
// ---------------------------------------------------------------------------

SystemUsbTopology snapshotUsbTopology(const AppRuntime &runtime);
std::unordered_map<std::string, int> snapshotControllerUsage(const AppRuntime &runtime);

// ---------------------------------------------------------------------------
// USB recovery (Windows) — restore stuck USB stack without OS reboot
// ---------------------------------------------------------------------------

// Disable+Enable every USB Root Hub (xHCI's immediate children), trigger
// pnputil /restart-device on every xHCI controller, force PnP tree rescan,
// and kick any remaining unknown/error USB devices. Runs elevated (UAC).
// Non-blocking — returns immediately, work happens in a detached thread.
// Writes a diagnostic log to %TEMP%\stm_femto_usb_reset.log.
void requestAllUsbHostReset();

// True while a reset is still running in the background.
bool isUsbResetBusy();

// Relaunch the current executable and exit this process. Required after a
// USB reset because the Orbbec SDK only enumerates devices at startup.
void restartApplication();
