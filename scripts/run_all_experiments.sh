#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MANIFEST="${1:-$ROOT/data/tar/linux-5.10/processed/files.txt}"
DATASET="${2:-linux-tar-subset}"
CSV="${3:-$ROOT/results/processed/metrics.csv}"
START_RUN="${START_RUN:-1}"
END_RUN="${4:-5}"
export TMPDIR="${TMPDIR:-/tmp}"
mkdir -p "$TMPDIR"

if (( START_RUN < 1 || END_RUN < START_RUN )); then
    echo "Require 1 <= START_RUN <= END_RUN" >&2
    exit 1
fi
if [[ ! -s "$MANIFEST" ]]; then
    echo "Missing or empty input manifest: $MANIFEST" >&2
    exit 1
fi
manifest_dir="$(cd "$(dirname "$MANIFEST")" && pwd)"
while IFS= read -r input || [[ -n "$input" ]]; do
    [[ -z "$input" || "$input" == \#* ]] && continue
    if [[ "$input" != /* ]]; then
        input="$manifest_dir/$input"
    fi
    if [[ ! -r "$input" ]]; then
        echo "Missing or unreadable input in manifest: $input" >&2
        exit 1
    fi
done < "$MANIFEST"
mkdir -p "$(dirname "$CSV")" "$ROOT/results/logs/finesse" \
    "$ROOT/results/logs/ntransform"
for run in $(seq "$START_RUN" "$END_RUN"); do
    for method in finesse ntransform; do
        log="$ROOT/results/logs/$method/${DATASET}-run-${run}.log"
        "$ROOT/build/src/run_$method" \
            --manifest "$MANIFEST" \
            --dataset "$DATASET" \
            --run-id "$run" \
            --csv "$CSV" \
            --log "$log"
    done
done
