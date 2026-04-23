{ pkgs, lib, ... }:
let
  defaultSdkDir = "C:\\Program Files\\OrbbecSDK 2.7.6";
in
{
  packages = [
    pkgs.cmake
    pkgs.ninja
    pkgs.curl
    pkgs.unzip
    pkgs.gnutar
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
        if [ "''${OS:-}" = "Windows_NT" ]; then
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
          echo "[dev-doctor] Non-Windows host detected: $(uname -s 2>/dev/null || echo unknown)"
          echo "[dev-doctor] Checking shared developer tooling only."

          if ! command -v cmake >/dev/null 2>&1; then
            echo "[dev-doctor] cmake is not available."
            exit 1
          fi
          if ! command -v ninja >/dev/null 2>&1; then
            echo "[dev-doctor] ninja is not available."
            exit 1
          fi

          cmake --version | head -n 1
          ninja --version
          echo "[dev-doctor] Shared tooling checks passed."
          echo "[dev-doctor] Windows build still requires Visual Studio Build Tools + Orbbec SDK."
          exit 0
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

          $missingPrereqs = @()

          $vcvars = "C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
          if (!(Test-Path $vcvars)) {
            $missingPrereqs += "Visual Studio Build Tools not found at: $vcvars"
          }

          $cmake = "C:\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
          if (!(Test-Path $cmake)) {
            $missingPrereqs += "MSVC bundled cmake.exe not found at: $cmake"
          }

          if ($missingPrereqs.Count -gt 0) {
            foreach ($message in $missingPrereqs) {
              Write-Error $message
            }
            exit 1
          }

          Write-Output "Doctor checks passed."
        '
      '';
    };

    "dev-build" = {
      description = "Build with the existing build.ps1 script";
      exec = ''
        set -e -o pipefail

        is_windows=0
        if [ "''${OS:-}" = "Windows_NT" ]; then
          is_windows=1
        fi
        case "$(uname -s 2>/dev/null || true)" in
          MINGW*|MSYS*|CYGWIN*|Windows_NT*) is_windows=1 ;;
        esac

        if [ "$is_windows" -ne 1 ]; then
          echo "[dev-build] Windows binary build is currently supported only on Windows hosts."
          echo "[dev-build] Run dev-doctor for cross-platform tooling checks."
          exit 1
        fi

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
        if [ "''${OS:-}" = "Windows_NT" ]; then
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

    "dev-download-sdks" = {
      description = "Download SDK dependencies for cross-platform setup";
      exec = ''
        set -e -o pipefail

        sdk_root=".devenv/sdks"
        mkdir -p "$sdk_root"

        echo "[dev-download-sdks] sdk_root=$sdk_root"
        vcpkg_dir="$sdk_root/vcpkg"
        if [ -d "$vcpkg_dir/.git" ]; then
          echo "[dev-download-sdks] vcpkg already exists: $vcpkg_dir"
          git -C "$vcpkg_dir" fetch --depth 1 origin master
          git -C "$vcpkg_dir" checkout -q FETCH_HEAD
        else
          echo "[dev-download-sdks] downloading vcpkg..."
          git clone --depth 1 https://github.com/microsoft/vcpkg.git "$vcpkg_dir"
        fi

        is_windows=0
        if [ "''${OS:-}" = "Windows_NT" ]; then
          is_windows=1
        fi
        case "$(uname -s 2>/dev/null || true)" in
          MINGW*|MSYS*|CYGWIN*|Windows_NT*) is_windows=1 ;;
        esac

        if [ "$is_windows" -eq 1 ]; then
          echo "[dev-download-sdks] bootstrapping vcpkg (Windows)..."
          if command -v cmd.exe >/dev/null 2>&1; then
            cmd.exe /c "$vcpkg_dir\\bootstrap-vcpkg.bat -disableMetrics"
          else
            echo "[dev-download-sdks] cmd.exe not found. Run bootstrap-vcpkg.bat manually."
            exit 1
          fi
          vcpkg_bin="$vcpkg_dir/vcpkg.exe"
        else
          echo "[dev-download-sdks] bootstrapping vcpkg (Unix)..."
          sh "$vcpkg_dir/bootstrap-vcpkg.sh" -disableMetrics
          vcpkg_bin="$vcpkg_dir/vcpkg"
        fi

        if [ ! -x "$vcpkg_bin" ] && [ ! -f "$vcpkg_bin" ]; then
          echo "[dev-download-sdks] vcpkg executable not found: $vcpkg_bin"
          exit 1
        fi

        triplet="''${VCPKG_TARGET_TRIPLET:-}"
        if [ -z "$triplet" ]; then
          if [ "$is_windows" -eq 1 ]; then
            triplet="x64-windows"
          else
            case "$(uname -s 2>/dev/null || true)" in
              Darwin)
                if [ "$(uname -m 2>/dev/null || true)" = "arm64" ]; then
                  triplet="arm64-osx"
                else
                  triplet="x64-osx"
                fi
                ;;
              Linux)
                if [ "$(uname -m 2>/dev/null || true)" = "aarch64" ]; then
                  triplet="arm64-linux"
                else
                  triplet="x64-linux"
                fi
                ;;
              *)
                triplet="x64-linux"
                ;;
            esac
          fi
        fi

        echo "[dev-download-sdks] installing vcpkg dependencies with triplet=$triplet"
        "$vcpkg_bin" install --triplet "$triplet" --x-manifest-root "$PWD"
        echo "[dev-download-sdks] vcpkg install completed."
        echo "[dev-download-sdks] VCPKG_ROOT=$vcpkg_dir"

        orbbec_url="''${ORBBEC_SDK_URL:-}"
        if [ -z "$orbbec_url" ]; then
          echo "[dev-download-sdks] ORBBEC_SDK_URL is not set, so Orbbec SDK download is skipped."
          echo "[dev-download-sdks] If needed, set ORBBEC_SDK_URL in .env.local and rerun."
          echo "[dev-download-sdks] Examples (v2.7.6):"
          echo "[dev-download-sdks]   macOS:   https://github.com/orbbec/OrbbecSDK_v2/releases/download/v2.7.6/OrbbecSDK_v2.7.6_202602022028_d712cda_macOS.zip"
          echo "[dev-download-sdks]   Windows: https://github.com/orbbec/OrbbecSDK_v2/releases/download/v2.7.6/OrbbecSDK_v2.7.6_202602022027_d712cda_win_x64.zip"
          echo "[dev-download-sdks]   Linux:   https://github.com/orbbec/OrbbecSDK_v2/releases/download/v2.7.6/OrbbecSDK_v2.7.6_202602021228_d712cda_linux_x86_64.zip"
          exit 0
        fi

        orbbec_dir="$sdk_root/orbbec"
        mkdir -p "$orbbec_dir"
        archive_name="$(basename "$orbbec_url")"
        archive_path="$orbbec_dir/$archive_name"

        echo "[dev-download-sdks] downloading Orbbec SDK archive..."
        curl -fL --retry 3 "$orbbec_url" -o "$archive_path"

        case "$archive_name" in
          *.zip)
            extract_dir="$orbbec_dir/extracted"
            rm -rf "$extract_dir"
            mkdir -p "$extract_dir"
            unzip -q "$archive_path" -d "$extract_dir"
            echo "[dev-download-sdks] extracted to: $extract_dir"
            echo "[dev-download-sdks] Set ORBBEC_SDK_DIR to extracted SDK root (contains include/lib)."
            ;;
          *.tar.gz|*.tgz|*.tar.xz|*.tar)
            extract_dir="$orbbec_dir/extracted"
            rm -rf "$extract_dir"
            mkdir -p "$extract_dir"
            tar -xf "$archive_path" -C "$extract_dir"
            echo "[dev-download-sdks] extracted to: $extract_dir"
            echo "[dev-download-sdks] Set ORBBEC_SDK_DIR to extracted SDK root (contains include/lib)."
            ;;
          *)
            echo "[dev-download-sdks] downloaded installer/archive to: $archive_path"
            echo "[dev-download-sdks] Unsupported auto-extract format. Install manually and set ORBBEC_SDK_DIR."
            ;;
        esac
      '';
    };
  };

  enterShell = ''
    echo "STM Femto Bolt Viewer devenv loaded."
    echo "Available commands: dev-doctor, dev-download-sdks, dev-build, dev-run, dev-package"
  '';
}
