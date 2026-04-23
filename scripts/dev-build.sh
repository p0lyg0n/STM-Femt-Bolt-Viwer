#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${BUILD_DIR:-$root_dir/build}"
generator="${CMAKE_GENERATOR:-Ninja}"

detect_default_triplet() {
  local system arch
  system="$(uname -s)"
  arch="$(uname -m)"
  case "$system" in
    Darwin)
      if [ "$arch" = "arm64" ]; then
        printf '%s\n' "arm64-osx"
      else
        printf '%s\n' "x64-osx"
      fi
      ;;
    Linux)
      if [ "$arch" = "aarch64" ]; then
        printf '%s\n' "arm64-linux"
      else
        printf '%s\n' "x64-linux"
      fi
      ;;
    *)
      printf '%s\n' "x64-linux"
      ;;
  esac
}

resolve_orbbec_sdk_dir() {
  if [ -n "${ORBBEC_SDK_DIR:-}" ] && [ -f "${ORBBEC_SDK_DIR}/include/libobsensor/ObSensor.hpp" ]; then
    printf '%s\n' "${ORBBEC_SDK_DIR}"
    return 0
  fi

  local extracted_root candidate header_path
  extracted_root="$root_dir/.devenv/sdks/orbbec/extracted"
  if [ -f "$extracted_root/include/libobsensor/ObSensor.hpp" ]; then
    printf '%s\n' "$extracted_root"
    return 0
  fi

  header_path="$(find "$extracted_root" -mindepth 1 -maxdepth 4 -type f -path '*/include/libobsensor/ObSensor.hpp' -print -quit 2>/dev/null || true)"
  if [ -n "$header_path" ]; then
    candidate="$(cd "$(dirname "$header_path")/../.." && pwd)"
    printf '%s\n' "$candidate"
    return 0
  fi

  return 1
}

orbbec_sdk_dir="$(resolve_orbbec_sdk_dir || true)"
if [ -z "$orbbec_sdk_dir" ]; then
  echo "[dev-build] Orbbec SDK not found."
  echo "[dev-build] Set ORBBEC_SDK_DIR or run dev-download-sdks with ORBBEC_SDK_URL first."
  exit 1
fi

export ORBBEC_SDK_DIR="$orbbec_sdk_dir"

rm -rf "$build_dir"

configure_args=(
  -S "$root_dir"
  -B "$build_dir"
  -G "$generator"
  -DCMAKE_BUILD_TYPE=Release
  "-DORBBEC_SDK_DIR=$orbbec_sdk_dir"
  "-DCMAKE_PREFIX_PATH=$orbbec_sdk_dir;$orbbec_sdk_dir/lib"
)

vcpkg_root="${VCPKG_ROOT:-}"
if [ -z "$vcpkg_root" ] && [ -d "$root_dir/.devenv/sdks/vcpkg" ]; then
  vcpkg_root="$root_dir/.devenv/sdks/vcpkg"
fi

if [ -n "$vcpkg_root" ] && [ -f "$vcpkg_root/scripts/buildsystems/vcpkg.cmake" ]; then
  triplet="${VCPKG_TARGET_TRIPLET:-$(detect_default_triplet)}"
  echo "[dev-build] Using vcpkg: VCPKG_ROOT=$vcpkg_root, triplet=$triplet"
  configure_args+=(
    "-DCMAKE_TOOLCHAIN_FILE=$vcpkg_root/scripts/buildsystems/vcpkg.cmake"
    -DVCPKG_MANIFEST_MODE=ON
    "-DVCPKG_TARGET_TRIPLET=$triplet"
  )
else
  echo "[dev-build] VCPKG_ROOT not set; proceeding without vcpkg toolchain."
fi

echo "[dev-build] Configuring with ORBBEC_SDK_DIR=$orbbec_sdk_dir"
cmake "${configure_args[@]}"
cmake --build "$build_dir" --config Release

echo
echo "Build completed."
echo "Run: $build_dir/stm_femto_bolt_viewer"
