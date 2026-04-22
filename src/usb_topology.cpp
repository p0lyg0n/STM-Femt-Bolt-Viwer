#include "usb_topology.h"
#include "camera_session.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <string>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

std::string trimCopy(const std::string &value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if(begin == std::string::npos) return "";
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::string extractPortNumber(const std::string &info) {
    if(info.empty()) return "";
    const size_t pos = info.find('#');
    if(pos == std::string::npos) return "";
    std::string num = info.substr(pos + 1);
    size_t end = num.find_first_not_of("0123456789");
    if(end != std::string::npos) num = num.substr(0, end);
    size_t first = num.find_first_not_of('0');
    return (first != std::string::npos) ? num.substr(first) : "0";
}

SystemUsbTopology queryUsbTopology() {
    SystemUsbTopology result;
#ifdef _WIN32
    const std::string cmd = R"(powershell -NoProfile -ExecutionPolicy Bypass -Command "$ErrorActionPreference='SilentlyContinue'; $dict = @{}; Get-CimInstance Win32_USBController | ForEach-Object { $id=$_.DeviceID; $nm=$_.Name; $dict[$id] = $nm; Write-Output ('CTRL:' + $id + '|' + $nm) }; Get-CimInstance Win32_USBControllerDevice | ForEach-Object { $did = $_.Dependent.DeviceID; if($did -match 'VID_2BC5') { $sn = ($did.Split('\'))[-1]; $cid = $_.Antecedent.DeviceID; $cnm = $dict[$cid]; if($cnm) { $cap = (Get-CimInstance Win32_PnPEntity -Filter \"DeviceID = '$($did.Replace('\', '\\'))'\").Caption; Write-Output ('SN:' + $sn + '|ID:' + $cid + '|NM:' + $cnm + '|PORT:' + $cap) } } }")";
    FILE *pipe = _popen(cmd.c_str(), "r");
    if(!pipe) return result;
    std::array<char, 2048> buf{};
    while(fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr) {
        const std::string line = trimCopy(buf.data());
        if(line.empty()) continue;
        if(line.rfind("CTRL:", 0) == 0) {
            const size_t sep = line.find('|');
            if(sep != std::string::npos) {
                const std::string controllerId = line.substr(5, sep - 5);
                const std::string controllerName = line.substr(sep + 1);
                result.controllers.push_back(controllerId);
                result.controllerNames[controllerId] = controllerName;
            }
        } else if(line.rfind("SN:", 0) == 0) {
            const size_t idPos = line.find("|ID:");
            const size_t nmPos = line.find("|NM:");
            const size_t ptPos = line.find("|PORT:");
            if(idPos != std::string::npos && nmPos != std::string::npos && ptPos != std::string::npos) {
                std::string serial = line.substr(3, idPos - 3);
                const size_t amp = serial.find('&');
                if(amp != std::string::npos) serial = serial.substr(0, amp);
                const std::string controllerId = line.substr(idPos + 4, nmPos - (idPos + 4));
                const std::string controllerName = line.substr(nmPos + 4, ptPos - (nmPos + 4));
                const std::string portInfo = line.substr(ptPos + 6);
                result.deviceMap[serial] = UsbInfo{controllerId, controllerName, "Root Hub", portInfo};
            }
        }
    }
    _pclose(pipe);
#endif
    return result;
}

std::unordered_map<std::string, int> buildControllerUsage(
    const std::vector<std::shared_ptr<CameraSession>> &sessions,
    const SystemUsbTopology &usbMap) {
    std::unordered_map<std::string, int> usage;
    for(const auto &session : sessions) {
        if(!session || session->serialNumber.empty()) continue;
        auto it = usbMap.deviceMap.find(session->serialNumber);
        if(it != usbMap.deviceMap.end() && !it->second.controllerId.empty()) {
            usage[it->second.controllerId]++;
        }
    }
    return usage;
}

void publishUsbTopology(AppRuntime &runtime, const SystemUsbTopology &usbTopology) {
    std::lock_guard<std::mutex> guard(runtime.usbTopologyMutex);
    runtime.usbTopology = usbTopology;
    runtime.controllerUsage = buildControllerUsage(runtime.sessions, runtime.usbTopology);
}

} // namespace

// ---------------------------------------------------------------------------
// Public functions
// ---------------------------------------------------------------------------

std::string normalizeUsbControllerName(std::string name) {
    auto replaceAll = [&name](const std::string &from, const std::string &to) {
        if(from.empty()) return;
        size_t pos = 0;
        while((pos = name.find(from, pos)) != std::string::npos) {
            name.replace(pos, from.size(), to);
            pos += to.size();
        }
    };

    replaceAll("eXtensible Host Controller", "xHCI");
    replaceAll(" - 1.10 (Microsoft)", "");

    auto trimLeft = [](std::string &s) {
        while(!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
            s.erase(s.begin());
        }
    };
    auto trimRight = [](std::string &s) {
        while(!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
            s.pop_back();
        }
    };
    auto collapseSpaces = [](std::string &s) {
        std::string out;
        out.reserve(s.size());
        bool prevSpace = false;
        for(unsigned char ch : s) {
            const bool isSpace = std::isspace(ch) != 0;
            if(isSpace) {
                if(!prevSpace) out.push_back(' ');
            } else {
                out.push_back(static_cast<char>(ch));
            }
            prevSpace = isSpace;
        }
        s.swap(out);
    };

    trimLeft(name);
    trimRight(name);
    collapseSpaces(name);
    return name;
}

std::string formatControllerDisplayName(const SystemUsbTopology &usbMap, const std::string &controllerId, const std::string &fallbackName) {
    if(controllerId.empty()) return fallbackName.empty() ? "Unknown Controller" : fallbackName;
    auto it = std::find(usbMap.controllers.begin(), usbMap.controllers.end(), controllerId);
    int controllerIndex = (it == usbMap.controllers.end()) ? -1 : static_cast<int>(std::distance(usbMap.controllers.begin(), it)) + 1;
    const std::string name = fallbackName.empty() ? "Unknown Controller" : fallbackName;
    if(controllerIndex > 0) {
        return "#" + std::to_string(controllerIndex) + " " + name;
    }
    return name;
}

void startUsbTopologyWorker(ob::Context &context, AppRuntime &runtime) {
    runtime.usbTopologyStop.store(false);
    runtime.usbTopologyThread = std::thread([&context, &runtime]() {
        while(!runtime.usbTopologyStop.load()) {
            try {
                publishUsbTopology(runtime, queryUsbTopology());
            } catch(...) {
            }
            std::shared_ptr<ob::DeviceList> deviceList;
            try {
                deviceList = context.queryDeviceList();
            } catch(...) {
            }
            if(deviceList) {
                for(const auto &session : runtime.sessions) {
                    if(!session || session->serialNumber.empty()) continue;
                    auto matchedDevice = findDeviceBySerial(deviceList, session->serialNumber);
                    // Take the session lifecycle lock so we never race with the
                    // Orbbec hotplug callback on the same session (double
                    // pipeline->stop, double attach, etc.).
                    std::lock_guard<std::mutex> lk(session->lifecycleMutex);
                    if(matchedDevice) {
                        if(session->disconnected.load()) {
                            logSession(session, "USB device detected again; reattaching");
                            attachSessionDevice(session, matchedDevice);
                            session->reconnecting.store(true);
                            try {
                                startCameraSession(session);
                            } catch(const std::exception &e) {
                                logSession(session, std::string("restart failed after USB return: ") + e.what());
                                session->disconnected.store(true);
                            } catch(...) {
                                logSession(session, "restart failed after USB return: unknown error");
                                session->disconnected.store(true);
                            }
                        }
                    } else if(!session->disconnected.load()) {
                        disconnectSession(session, "USB device removed; marking session disconnected");
                    }
                }
            }
            // Poll every ~500ms so a physical unplug is caught before the
            // main thread's 8s frame-timeout path tries to restart a dead device.
            for(int i = 0; i < 5 && !runtime.usbTopologyStop.load(); ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    });
}

void stopUsbTopologyWorker(AppRuntime &runtime) {
    runtime.usbTopologyStop.store(true);
    if(runtime.usbTopologyThread.joinable()) {
        runtime.usbTopologyThread.join();
    }
}

SystemUsbTopology snapshotUsbTopology(const AppRuntime &runtime) {
    std::lock_guard<std::mutex> guard(runtime.usbTopologyMutex);
    return runtime.usbTopology;
}

std::unordered_map<std::string, int> snapshotControllerUsage(const AppRuntime &runtime) {
    std::lock_guard<std::mutex> guard(runtime.usbTopologyMutex);
    return runtime.controllerUsage;
}

// ---------------------------------------------------------------------------
// USB PnP reset
// ---------------------------------------------------------------------------

namespace {

std::atomic<bool> g_usbResetBusy{false};

#ifdef _WIN32
// Launch PowerShell elevated, truly hidden.
// Trick: write the script to a temp .ps1 file, then launch conhost.exe via
// ShellExecuteEx with "runas" + SW_HIDE. The -File form plus a script file
// avoids the brief console flash that `-Command "..."` tends to produce.
bool runElevatedPowerShell(const std::wstring &psCommand) {
    // Build script body with all output streams silenced.
    const std::wstring scriptBody =
        L"$ErrorActionPreference = 'SilentlyContinue'\r\n"
        L"$ProgressPreference   = 'SilentlyContinue'\r\n"
        L"try {\r\n"
        + psCommand + L"\r\n"
        L"} catch { }\r\n";

    // Write script to %TEMP%\stm_femto_usb_reset.ps1
    wchar_t tempDir[MAX_PATH] = {0};
    if(GetTempPathW(MAX_PATH, tempDir) == 0) return false;
    std::wstring scriptPath = std::wstring(tempDir) + L"stm_femto_usb_reset.ps1";

    HANDLE hFile = CreateFileW(scriptPath.c_str(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if(hFile == INVALID_HANDLE_VALUE) return false;
    // Write UTF-8 with BOM so PowerShell reads Unicode correctly
    const unsigned char bom[3] = { 0xEF, 0xBB, 0xBF };
    DWORD written = 0;
    WriteFile(hFile, bom, sizeof(bom), &written, nullptr);
    const int utf8Len = WideCharToMultiByte(CP_UTF8, 0, scriptBody.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if(utf8Len > 0) {
        std::string utf8(utf8Len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, scriptBody.c_str(), -1, utf8.data(), utf8Len, nullptr, nullptr);
        WriteFile(hFile, utf8.data(), (DWORD)utf8.size(), &written, nullptr);
    }
    CloseHandle(hFile);

    const std::wstring args =
        L"-NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -File \""
        + scriptPath + L"\"";

    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask  = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"runas";
    sei.lpFile = L"powershell.exe";
    sei.lpParameters = args.c_str();
    sei.nShow = SW_HIDE;

    if(!ShellExecuteExW(&sei)) {
        DeleteFileW(scriptPath.c_str());
        return false;
    }
    if(sei.hProcess) {
        WaitForSingleObject(sei.hProcess, 30000);
        CloseHandle(sei.hProcess);
    }
    DeleteFileW(scriptPath.c_str());
    return true;
}
#endif

} // namespace

bool isUsbResetBusy() {
    return g_usbResetBusy.load();
}

void requestAllUsbHostReset() {
#ifdef _WIN32
    if(g_usbResetBusy.exchange(true)) return;
    std::thread([] {
        // Strategy: AMD/Intel xHCI root controllers often REFUSE Disable
        // ("not supported"). Target the USB Root Hub layer instead - the
        // direct children of the xHCI controllers. Disable+Enable on the root
        // hubs forces a full re-enumeration of all downstream ports, which is
        // what's needed to recover stuck descriptor-request-failure devices.
        // Additionally issue `pnputil /scan-devices` to force PnP tree rescan
        // and kick any lingering unknown/error USB devices.
        // Log written to %TEMP%\stm_femto_usb_reset.log (UTF-8, English).
        const std::wstring cmd =
            L"$log = \"$env:TEMP\\stm_femto_usb_reset.log\"; "
            L"function L($m){ $ts=(Get-Date).ToString('HH:mm:ss'); "
            L"  Add-Content -Encoding UTF8 -Path $log -Value \"[$ts] $m\" }; "
            L"L '=== hard reset start ==='; "

            // --- Collect Root Hubs (children of xHCI) ---
            L"$hubs = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue | Where-Object { "
            L"$_.InstanceId -match 'USB\\\\ROOT_HUB' "
            L"}; "
            L"L (\"Root hubs found: $($hubs.Count)\"); "
            L"foreach($h in $hubs){ L (\"  hub: $($h.InstanceId)  status=$($h.Status)\") }; "

            // --- Disable root hubs (works even when xHCI refuses) ---
            L"$disabledOk = @(); "
            L"try { "
            L"  foreach($h in $hubs){ "
            L"    try { Disable-PnpDevice -InstanceId $h.InstanceId -Confirm:$false -ErrorAction Stop *> $null; "
            L"          $disabledOk += $h; L \"  DISABLE ok: $($h.InstanceId)\" } "
            L"    catch { L \"  DISABLE FAIL: $($h.InstanceId) -> $($_.Exception.Message)\" } "
            L"  }; "
            L"  Start-Sleep -Seconds 3; "
            L"} finally { "
            L"  foreach($h in $hubs){ "
            L"    try { Enable-PnpDevice -InstanceId $h.InstanceId -Confirm:$false -ErrorAction Stop *> $null; "
            L"          L \"  ENABLE ok:  $($h.InstanceId)\" } "
            L"    catch { L \"  ENABLE FAIL: $($h.InstanceId) -> $($_.Exception.Message)\" } "
            L"  } "
            L"}; "

            // --- Fallback: pnputil /restart-device on xHCI controllers ---
            L"$xhci = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue | Where-Object { "
            L"$_.FriendlyName -match 'Host Controller' -or $_.FriendlyName -match 'xHCI' "
            L"}; "
            L"L (\"xHCI controllers: $($xhci.Count) (trying pnputil /restart-device)\"); "
            L"foreach($c in $xhci){ "
            L"  $iid = $c.InstanceId; "
            L"  $out = & pnputil /restart-device \"$iid\" 2>&1; "
            L"  L \"  pnputil restart: $iid -> $($out -join ' | ')\" "
            L"}; "

            // --- Force PnP tree rescan ---
            L"Start-Sleep -Milliseconds 500; "
            L"$scan = & pnputil /scan-devices 2>&1; "
            L"L \"pnputil scan: $($scan -join ' | ')\"; "

            // --- Kick unknown/error USB devices ---
            L"Start-Sleep -Milliseconds 800; "
            L"$unk = Get-PnpDevice -PresentOnly -Status Error,Unknown -ErrorAction SilentlyContinue | Where-Object { "
            L"$_.InstanceId -like 'USB*' "
            L"}; "
            L"L (\"Unknown/Error USB devices: $($unk.Count)\"); "
            L"foreach($u in $unk){ "
            L"  try { Disable-PnpDevice -InstanceId $u.InstanceId -Confirm:$false -ErrorAction SilentlyContinue *> $null; "
            L"        Start-Sleep -Milliseconds 400; "
            L"        Enable-PnpDevice  -InstanceId $u.InstanceId -Confirm:$false -ErrorAction SilentlyContinue *> $null; "
            L"        L \"  kicked unknown: $($u.InstanceId)\" } catch {} "
            L"}; "
            L"L '=== hard reset end ===';";
        runElevatedPowerShell(cmd);
        g_usbResetBusy.store(false);
    }).detach();
#else
    (void)g_usbResetBusy;
#endif
}

void restartApplication() {
#ifdef _WIN32
    wchar_t exePath[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if(len == 0 || len >= MAX_PATH) {
        std::exit(0);
        return;
    }
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask  = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    sei.lpVerb = L"open";
    sei.lpFile = exePath;
    sei.nShow = SW_SHOW;
    ShellExecuteExW(&sei);
    if(sei.hProcess) CloseHandle(sei.hProcess);
    std::exit(0);
#else
    std::exit(0);
#endif
}

