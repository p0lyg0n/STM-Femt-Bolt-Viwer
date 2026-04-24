#!/usr/bin/env bash
#
# Resolves the SHA-256 checksum for a known Orbbec SDK archive name or URL.
# Uses scripts/orbbec-sdk-checksums.txt as the single source of truth.

set -euo pipefail

if [ "$#" -ne 1 ]; then
  echo "usage: $0 <archive-name-or-url>" >&2
  exit 2
fi

input_target="$1"
archive_name="$input_target"

case "$archive_name" in
  http://*|https://*|file://*)
    archive_name="${archive_name##*/}"
    ;;
esac

script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
manifest_path="$script_dir/orbbec-sdk-checksums.txt"

if [ ! -f "$manifest_path" ]; then
  echo "Missing checksum manifest: $manifest_path" >&2
  exit 1
fi

awk -v target="$archive_name" '
  /^[[:space:]]*#/ || /^[[:space:]]*$/ { next }
  $2 == target { print $1; found=1; exit }
  END { if (!found) exit 1 }
' "$manifest_path"
