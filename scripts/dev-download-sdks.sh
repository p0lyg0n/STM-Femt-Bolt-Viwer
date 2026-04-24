#!/usr/bin/env bash
set -euo pipefail

sdk_root=".devenv/sdks"
mkdir -p "$sdk_root"

script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
orbbec_checksum_manifest="$script_dir/orbbec-sdk-checksums.txt"

known_orbbec_sha256() {
  local archive_name="$1"
  if [ ! -f "$orbbec_checksum_manifest" ]; then
    return 1
  fi
  awk -v target="$archive_name" '
    /^[[:space:]]*#/ || /^[[:space:]]*$/ { next }
    $2 == target { print $1; found=1; exit }
    END { if (!found) exit 1 }
  ' "$orbbec_checksum_manifest"
}

is_official_orbbec_release_url() {
  case "$1" in
    https://github.com/orbbec/OrbbecSDK_v2/releases/download/*)
      return 0
      ;;
    *)
      return 1
      ;;
  esac
}

sha256_file() {
  if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | awk '{print $1}'
    return 0
  fi
  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$1" | awk '{print $1}'
    return 0
  fi
  if command -v pwsh >/dev/null 2>&1; then
    HASH_TARGET_PATH="$1" pwsh -NoLogo -NoProfile -Command "(Get-FileHash -Algorithm SHA256 $env:HASH_TARGET_PATH).Hash.ToLowerInvariant()" | tr -d '\r'
    return 0
  fi
  if command -v powershell.exe >/dev/null 2>&1; then
    HASH_TARGET_PATH="$1" powershell.exe -NoProfile -Command "(Get-FileHash -Algorithm SHA256 $env:HASH_TARGET_PATH).Hash.ToLowerInvariant()" | tr -d '\r'
    return 0
  fi
  echo "[dev-download-sdks] Unable to find a SHA-256 tool (sha256sum/shasum/pwsh/Get-FileHash)." >&2
  return 1
}

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
  if [ "$(git -C "$vcpkg_dir" rev-parse --is-shallow-repository 2>/dev/null || echo false)" = "true" ]; then
    echo "[dev-download-sdks] converting shallow vcpkg checkout to full history for builtin-baseline support"
    git -C "$vcpkg_dir" fetch --unshallow origin
  else
    git -C "$vcpkg_dir" fetch origin
  fi
  git -C "$vcpkg_dir" fetch origin master
  git -C "$vcpkg_dir" checkout -q FETCH_HEAD
else
  echo "[dev-download-sdks] downloading vcpkg..."
  git clone https://github.com/microsoft/vcpkg.git "$vcpkg_dir"
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
  echo "[dev-download-sdks] Custom archives should also set ORBBEC_SDK_SHA256."
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
expected_sha256="${ORBBEC_SDK_SHA256:-}"
if [ -z "$expected_sha256" ]; then
  expected_sha256="$(known_orbbec_sha256 "$archive_name" || true)"
fi

if [ -z "$expected_sha256" ]; then
  if is_official_orbbec_release_url "$orbbec_url"; then
    echo "[dev-download-sdks] No SHA-256 checksum is known for official Orbbec release archive $archive_name." >&2
    echo "[dev-download-sdks] Update $orbbec_checksum_manifest or set ORBBEC_SDK_SHA256." >&2
    exit 1
  fi
  echo "[dev-download-sdks] Warning: no SHA-256 checksum is known for custom archive $archive_name." >&2
  echo "[dev-download-sdks] Continuing without checksum verification. Set ORBBEC_SDK_SHA256 to verify custom archives." >&2
else
  echo "[dev-download-sdks] expecting sha256=$expected_sha256"
fi

echo "[dev-download-sdks] downloading Orbbec SDK archive..."
curl -fL --retry 3 "$orbbec_url" -o "$archive_path"
if [ -n "$expected_sha256" ]; then
  actual_sha256="$(sha256_file "$archive_path")"
  if [ "$actual_sha256" != "$expected_sha256" ]; then
    echo "[dev-download-sdks] SHA-256 mismatch for $archive_name" >&2
    echo "[dev-download-sdks] expected: $expected_sha256" >&2
    echo "[dev-download-sdks] actual:   $actual_sha256" >&2
    exit 1
  fi
  echo "[dev-download-sdks] SHA-256 verified for $archive_name"
else
  echo "[dev-download-sdks] checksum verification skipped for $archive_name"
fi

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
