#!/bin/bash
# Headless check: auto-plays a song for N frames with the software renderer, saves every K-th frame as PNG.
#   bash scripts/dev_frames.sh <song> [diff] [frames] [every] [outdir]
set -eu
cd "$(dirname "$0")/.."
SONG=${1:-barracuda}; DIFF=${2:-expert}; N=${3:-1500}; K=${4:-150}; OUT=${5:-build/frames}
DATA=${GH3_DATA:-/mnt/f/games/gh3/DATA}
mkdir -p "$OUT"; rm -f "$OUT"/*.bmp "$OUT"/*.png
./build/gh3 --data "$DATA" --frames "$N" --every "$K" --song "$SONG" --diff "$DIFF" --out "$OUT"
python3 -I tools/bmp2png.py "$OUT"/*.bmp && rm -f "$OUT"/*.bmp
ls "$OUT"
