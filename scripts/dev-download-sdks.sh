#!/usr/bin/env bash
set -euo pipefail

sdk_root=".devenv/sdks"
mkdir -p "$sdk_root"

host_os_raw="$(uname -s 2>/dev/null || printf 'unknown')"
host_arch_raw="$(uname -m 2>/dev/null || printf 'unknown')"

host_os="$host_os_raw"
case "$host_os_raw" in
  Darwin) host_os="macos" ;;
  Linux) host_os="linux" ;;
  MINGW*|MSYS*|CYGWIN*|Windows_NT) host_os="windows" ;;
esac

host_arch="$host_arch_raw"
case "$host_arch_raw" in
  aarch64) host_arch="arm64" ;;
  amd64) host_arch="x86_64" ;;
esac

host_key="${host_os}/${host_arch}"

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
if [ "${OS:-}" = "Windows_NT" ]; then
  is_windows=1
fi
case "$host_os_raw" in
  MINGW*|MSYS*|CYGWIN*|Windows_NT*) is_windows=1 ;;
esac

vcpkg_tools_dir="$vcpkg_dir/downloads/tools"
vcpkg_tools_host_key_file="$vcpkg_dir/.dev-download-sdks-host-key"
stored_host_key=""
if [ -f "$vcpkg_tools_host_key_file" ]; then
  stored_host_key="$(cat "$vcpkg_tools_host_key_file")"
fi

if [ -n "$stored_host_key" ] && [ "$stored_host_key" != "$host_key" ]; then
  echo "[dev-download-sdks] host changed from $stored_host_key to $host_key"
  echo "[dev-download-sdks] clearing vcpkg tool cache to avoid cross-arch tool reuse"
  rm -rf "$vcpkg_tools_dir"
elif [ -z "$stored_host_key" ] && [ -d "$vcpkg_tools_dir" ]; then
  echo "[dev-download-sdks] vcpkg tool cache host stamp missing; clearing once for safety"
  rm -rf "$vcpkg_tools_dir"
fi

printf '%s\n' "$host_key" > "$vcpkg_tools_host_key_file"

if [ "$is_windows" -eq 1 ]; then
  echo "[dev-download-sdks] bootstrapping vcpkg (Windows)..."
  if command -v cmd.exe >/dev/null 2>&1; then
    vcpkg_dir_win="$vcpkg_dir"
    if command -v cygpath >/dev/null 2>&1; then
      vcpkg_dir_win="$(cygpath -w "$vcpkg_dir")"
    fi
    cmd.exe /c "\"$vcpkg_dir_win\\bootstrap-vcpkg.bat\" -disableMetrics"
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

triplet="${VCPKG_TARGET_TRIPLET:-}"
if [ -z "$triplet" ]; then
  if [ "$is_windows" -eq 1 ]; then
    triplet="x64-windows"
  else
    case "$host_os_raw" in
      Darwin)
        if [ "$host_arch" = "arm64" ]; then
          triplet="arm64-osx"
        else
          triplet="x64-osx"
        fi
        ;;
      Linux)
        if [ "$host_arch" = "arm64" ]; then
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

orbbec_url="${ORBBEC_SDK_URL:-}"
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
    unzip_status=0
    unzip -q "$archive_path" -d "$extract_dir" || unzip_status=$?
    if [ "$unzip_status" -gt 1 ]; then
      echo "[dev-download-sdks] unzip failed with exit code $unzip_status"
      exit "$unzip_status"
    fi
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
