#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RAW="${1:-$ROOT/data/tar/linux-5.10/raw}"
OUT="${2:-$ROOT/data/tar/linux-5.10/processed}"
VERSION_COUNT="${3:-3}"
EXISTING_DIR="${4:-}"
ALL_VERSIONS=(
    "5.10.1" "5.10.2" "5.10.3" "5.10.4" "5.10.5"
    "5.10.6" "5.10.7" "5.10.8" "5.10.9" "5.10.10"
)
if (( VERSION_COUNT < 2 || VERSION_COUNT > ${#ALL_VERSIONS[@]} )); then
    echo "Version count must be between 2 and ${#ALL_VERSIONS[@]}" >&2
    exit 1
fi
VERSIONS=("${ALL_VERSIONS[@]:0:VERSION_COUNT}")

mkdir -p "$OUT"
RAW="$(cd "$RAW" && pwd)"
OUT="$(cd "$OUT" && pwd)"
if [[ -n "$EXISTING_DIR" ]]; then
    EXISTING_DIR="$(cd "$EXISTING_DIR" && pwd)"
fi
manifest="$OUT/files.txt"
: > "$manifest"
for version in "${VERSIONS[@]}"; do
    source="$RAW/linux-${version}.tar.xz"
    target="$OUT/linux-${version}.tar"
    if [[ ! -s "$source" ]]; then
        echo "Missing source archive: $source" >&2
        exit 1
    fi
    input="$target"
    if [[ ! -s "$target" && -n "$EXISTING_DIR" &&
        -s "$EXISTING_DIR/linux-${version}.tar" ]]; then
        input="$EXISTING_DIR/linux-${version}.tar"
    elif [[ ! -s "$target" ]]; then
        xz --decompress --stdout "$source" > "$target"
    fi
    tar -tf "$input" >/dev/null
    printf '%s\n' "$input" >> "$manifest"
done
du -h "$OUT"/*.tar 2>/dev/null || true
cat "$manifest"
