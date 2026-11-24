#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:-$ROOT/data/tar/linux-5.10/raw}"
BASE_URL="https://cdn.kernel.org/pub/linux/kernel/v5.x"
VERSION_COUNT="${2:-3}"
DOWNLOAD_JOBS="${DOWNLOAD_JOBS:-8}"
CHUNK_BYTES=$((8 * 1024 * 1024))
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
curl --fail --location --retry 3 --silent --show-error \
    "$BASE_URL/sha256sums.asc" -o "$OUT/sha256sums.asc"

for version in "${VERSIONS[@]}"; do
    archive="linux-${version}.tar.xz"
    expected="$(awk -v file="$archive" '$2 == file {print $1}' \
        "$OUT/sha256sums.asc")"
    if [[ -z "$expected" ]]; then
        echo "No SHA-256 entry found for $archive" >&2
        exit 1
    fi
    if [[ -s "$OUT/$archive" ]] &&
        printf '%s  %s\n' "$expected" "$OUT/$archive" |
            sha256sum --check --status -; then
        echo "$archive: already verified"
        continue
    fi

    size="$(curl --fail --location --silent --show-error --head \
        "$BASE_URL/$archive" |
        awk 'BEGIN {IGNORECASE=1} /^content-length:/ {gsub("\r", ""); n=$2} END {print n}')"
    if [[ ! "$size" =~ ^[0-9]+$ ]]; then
        echo "Could not determine archive size: $archive" >&2
        exit 1
    fi
    part_dir="$(mktemp -d "$OUT/.${version}.parts.XXXXXX")"
    trap '[[ -z "$part_dir" ]] || rm -rf "$part_dir"' EXIT
    part_file="$OUT/.${archive}.part"
    last_part=$(((size + CHUNK_BYTES - 1) / CHUNK_BYTES - 1))
    export BASE_URL archive part_dir size CHUNK_BYTES
    seq 0 "$last_part" |
        xargs -P "$DOWNLOAD_JOBS" -I '{}' bash -c '
            index="$1"
            start=$((index * CHUNK_BYTES))
            end=$((start + CHUNK_BYTES - 1))
            if (( end >= size )); then end=$((size - 1)); fi
            curl --fail --location --retry 5 --retry-all-errors \
                --silent --show-error \
                --range "$start-$end" \
                "$BASE_URL/$archive" -o "$part_dir/part-$index"
            expected_size=$((end - start + 1))
            actual_size=$(stat -c %s "$part_dir/part-$index")
            [[ "$actual_size" -eq "$expected_size" ]]
        ' _ '{}'

    : > "$part_file"
    for ((index = 0; index <= last_part; ++index)); do
        cat "$part_dir/part-$index" >> "$part_file"
    done
    rm -rf "$part_dir"
    part_dir=""
    if ! printf '%s  %s\n' "$expected" "$part_file" |
        sha256sum --check -; then
        rm -f "$part_file"
        echo "SHA-256 verification failed: $archive" >&2
        exit 1
    fi
    mv "$part_file" "$OUT/$archive"
done
