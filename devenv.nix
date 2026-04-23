{ pkgs, lib, ... }:
let
  defaultSdkDir = "C:\\Program Files\\OrbbecSDK 2.7.6";
in
{
  packages = [
    pkgs.cmake
    pkgs.ninja
    pkgs.git
    pkgs.powershell
  ];

  env.ORBBEC_SDK_DIR = lib.mkDefault defaultSdkDir;

  scripts = {
    "dev-doctor" = {
      description = "Validate Windows build prerequisites and ORBBEC_SDK_DIR";
      exec = ''
        set -e -o pipefail

        is_windows=0
        if [ "${OS:-}" = "Windows_NT" ]; then
          is_windows=1
        fi
        case "$(uname -s 2>/dev/null || true)" in
          MINGW*|MSYS*|CYGWIN*|Windows_NT*) is_windows=1 ;;
        esac

        sdk_dir="$ORBBEC_SDK_DIR"
        if [ -z "$sdk_dir" ]; then
          sdk_dir="${defaultSdkDir}"
        fi
        export ORBBEC_SDK_DIR="$sdk_dir"

        echo "[dev-doctor] ORBBEC_SDK_DIR=$ORBBEC_SDK_DIR"

        if [ "$is_windows" -ne 1 ]; then
          echo "[dev-doctor] This repository can only be built on Windows hosts."
          echo "[dev-doctor] Current platform: $(uname -s 2>/dev/null || echo unknown)"
          exit 1
        fi

        ps_exe=""
        if command -v pwsh >/dev/null 2>&1; then
          ps_exe="pwsh"
        elif command -v powershell >/dev/null 2>&1; then
          ps_exe="powershell"
        else
          echo "[dev-doctor] PowerShell is not available (pwsh/powershell)."
          exit 1
        fi

        "$ps_exe" -NoLogo -NoProfile -Command '
          $sdk = $env:ORBBEC_SDK_DIR
          if ([string]::IsNullOrWhiteSpace($sdk)) {
            Write-Error "ORBBEC_SDK_DIR is empty."
            exit 1
          }

          $header = Join-Path $sdk "include\libobsensor\ObSensor.hpp"
          if (!(Test-Path $header)) {
            Write-Error "Orbbec SDK header not found: $header"
            exit 1
          }

          $vcvars = "C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
          if (!(Test-Path $vcvars)) {
            Write-Warning "Visual Studio Build Tools not found at: $vcvars"
          }

          $cmake = "C:\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
          if (!(Test-Path $cmake)) {
            Write-Warning "MSVC bundled cmake.exe not found at: $cmake"
          }

          Write-Output "Doctor checks passed."
        '
      '';
    };

    "dev-build" = {
      description = "Build with the existing build.ps1 script";
      exec = ''
        set -e -o pipefail
        dev-doctor

        if command -v pwsh >/dev/null 2>&1; then
          pwsh -NoLogo -NoProfile -ExecutionPolicy Bypass -File ./build.ps1
        elif command -v powershell >/dev/null 2>&1; then
          powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File ./build.ps1
        else
          echo "[dev-build] PowerShell is not available (pwsh/powershell)."
          exit 1
        fi
      '';
    };

    "dev-run" = {
      description = "Run the built executable";
      exec = ''
        set -e -o pipefail

        is_windows=0
        if [ "${OS:-}" = "Windows_NT" ]; then
          is_windows=1
        fi
        case "$(uname -s 2>/dev/null || true)" in
          MINGW*|MSYS*|CYGWIN*|Windows_NT*) is_windows=1 ;;
        esac

        if [ "$is_windows" -ne 1 ]; then
          echo "[dev-run] This command is only available on Windows hosts."
          exit 1
        fi

        exe_path="./build/stm_femto_bolt_viewer.exe"
        if [ ! -f "$exe_path" ]; then
          echo "[dev-run] Executable not found: $exe_path"
          echo "[dev-run] Run dev-build first."
          exit 1
        fi

        "$exe_path"
      '';
    };

    "dev-package" = {
      description = "Create release zip in release/";
      exec = ''
        set -e -o pipefail
        dev-doctor

        if command -v pwsh >/dev/null 2>&1; then
          pwsh -NoLogo -NoProfile -ExecutionPolicy Bypass -File ./scripts/package_release.ps1 -BuildDir ./build -OutputDir ./release
        elif command -v powershell >/dev/null 2>&1; then
          powershell -NoLogo -NoProfile -ExecutionPolicy Bypass -File ./scripts/package_release.ps1 -BuildDir ./build -OutputDir ./release
        else
          echo "[dev-package] PowerShell is not available (pwsh/powershell)."
          exit 1
        fi
      '';
    };
  };

  enterShell = ''
    echo "STM Femto Bolt Viewer devenv loaded."
    echo "Available commands: dev-doctor, dev-build, dev-run, dev-package"
  '';
}
