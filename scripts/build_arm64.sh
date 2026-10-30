#!/bin/bash
# Cross-build for the R36S (RK3326, Cortex-A35, aarch64) against an old-glibc sysroot that has SDL2.
#   SYSROOT=/path/to/sysroot-arm64 bash scripts/build_arm64.sh
# The sysroot from the Don't Starve project (Ubuntu 18.04: glibc 2.27, SDL 2.0.8) is the default when present.
set -eu
cd "$(dirname "$0")/.."
CC=${CC:-aarch64-linux-gnu-gcc}
SYSROOT=${SYSROOT:-/mnt/e/Games/DontStarve_R36S/recompiled/build/sysroot-arm64}
[ -d "$SYSROOT/usr/include/SDL2" ] || { echo "no SDL2 headers in $SYSROOT (set SYSROOT)"; exit 1; }
MA=aarch64-linux-gnu
mkdir -p build
# Ubuntu's cross gcc looks in /usr/aarch64-linux-gnu whatever --sysroot says: force the sysroot's headers, crt files and libraries first.
INC="-nostdinc -isystem $($CC -print-file-name=include) -isystem $SYSROOT/usr/include/$MA -isystem $SYSROOT/usr/include -I$SYSROOT/usr/include/SDL2 -D_REENTRANT"
LIB="--sysroot=$SYSROOT -B$SYSROOT/usr/lib/$MA -L$SYSROOT/lib/$MA -L$SYSROOT/usr/lib/$MA -Wl,-rpath-link=$SYSROOT/lib/$MA:$SYSROOT/usr/lib/$MA"
FLAGS="-O2 -mcpu=cortex-a35 -std=gnu11 -D_GNU_SOURCE -Wall -Wno-unused-parameter -Wno-unused-function -Wno-misleading-indentation -ffast-math"
SRC="src/main.c src/gh3fmt.c src/audio.c src/game.c src/fb.c src/font.c src/gfx.c src/art.c src/ui.c src/render.c src/settings.c src/input.c src/career.c src/screens.c src/fx.c src/stretch.c src/practice.c src/store.c src/movie.c src/battle.c src/render_battle.c src/cheats.c src/tutorial.c src/sfx.c src/gamearts.c src/original.c"
$CC $FLAGS $INC -Ithird_party -o build/gh3.aarch64 $SRC $LIB -Wl,--allow-shlib-undefined -lSDL2 -lm -lpthread -ldl
aarch64-linux-gnu-strip build/gh3.aarch64 2>/dev/null || true
echo "built build/gh3.aarch64"
echo "newest glibc symbol needed: $(aarch64-linux-gnu-objdump -T build/gh3.aarch64 | grep -o 'GLIBC_[0-9.]*' | sort -t. -k2,2n -k3,3n -u | tail -1)"
