#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SOURCE="${1:-$ROOT/res/public/Quattro-1024.png}"
OUTPUT="${2:-$ROOT/res/Quattro.icns}"
ICONSET="$(mktemp -d)/Quattro.iconset"
mkdir -p "$ICONSET"
trap 'rm -rf "$(dirname "$ICONSET")"' EXIT

for spec in "16:16" "16:16@2x" "32:32" "32:32@2x" "128:128" "128:128@2x" "256:256" "256:256@2x" "512:512" "512:512@2x"; do
    pixels="${spec%%:*}"
    suffix="${spec#*:}"
    if [[ "$suffix" == *@2x ]]; then
        output_pixels=$((pixels * 2))
        name="icon_${pixels}x${pixels}@2x.png"
    else
        output_pixels="$pixels"
        name="icon_${pixels}x${pixels}.png"
    fi
    sips -z "$output_pixels" "$output_pixels" "$SOURCE" --out "$ICONSET/$name" >/dev/null
done

iconutil -c icns "$ICONSET" -o "$OUTPUT"
