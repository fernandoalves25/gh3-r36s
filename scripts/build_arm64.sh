#!/bin/bash
# Cross-build for the R36S (RK3326, Cortex-A35, aarch64) against an old-glibc sysroot that has SDL2.
#   SYSROOT=/path/to/sysroot-arm64 bash scripts/build_arm64.sh
# The sysroot from the Don't Starve project (Ubuntu 18.04: glibc 2.27, SDL 2.0.8) is the default when present.
# One binary, gh3.aarch64, built with -DGH3_3D: the game's venues in 3D behind the highway (src/r3d/; the 2D stage stays as the fallback
# when ZONES/ is missing or the 3D stage is off). The 2D-only build is the host's build/gh3, for the headless tests.
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
SRC="src/main.c src/gh3fmt.c src/audio.c src/game.c src/fb.c src/font.c src/gfx.c src/art.c src/ui.c src/render.c src/settings.c src/input.c src/career.c src/screens.c src/fx.c src/stretch.c src/practice.c src/store.c src/movie.c src/overlay.c src/battle.c src/render_battle.c src/cheats.c src/tutorial.c src/sfx.c src/charsel.c src/charsel_pt.c src/orig_charsel.c src/gamearts.c src/original.c src/orig_setup.c src/orig_setlist.c src/orig_loading.c src/orig_results.c src/orig_options.c src/orig_font.c"
SRC3D="src/r3d/r3d.c src/r3d/r3d_zone.c src/r3d/r3d_lights.c src/r3d/r3d_skin.c src/r3d/r3d_band.c src/r3d/r3d_crowd.c"
glibc_of() { aarch64-linux-gnu-objdump -T "$1" | grep -o 'GLIBC_[0-9.]*' | sort -t. -k2,2n -k3,3n -u | tail -1; }
$CC $FLAGS -DGH3_3D $INC -Ithird_party -Isrc -o build/gh3.aarch64 $SRC $SRC3D $LIB -Wl,--allow-shlib-undefined -lSDL2 -lm -lpthread -ldl
aarch64-linux-gnu-strip build/gh3.aarch64 2>/dev/null || true
echo "built build/gh3.aarch64 with the 3D venues (newest glibc symbol needed: $(glibc_of build/gh3.aarch64))"
