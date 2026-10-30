# Host build (Linux / WSL): make            -> build/gh3 (2D) and build/gh3-3d (the same plus the 3D venues, -DGH3_3D)
# R36S build:               make arm64      -> build/gh3.aarch64 and build/gh3-3d.aarch64 (needs SYSROOT, see scripts/build_arm64.sh)
CC ?= gcc
CFLAGS ?= -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-misleading-indentation -std=gnu11 -D_GNU_SOURCE
SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS := $(shell sdl2-config --libs 2>/dev/null)
SRC := src/main.c src/gh3fmt.c src/audio.c src/game.c src/fb.c src/font.c src/gfx.c src/art.c src/ui.c src/render.c src/settings.c src/input.c src/career.c src/screens.c src/fx.c src/stretch.c src/practice.c src/store.c src/movie.c src/battle.c src/render_battle.c src/cheats.c src/tutorial.c
HDR := src/gh3fmt.h src/audio.h src/game.h src/fb.h src/font.h src/gfx.h src/art.h src/ui.h src/render.h src/settings.h src/input.h src/career.h src/app.h src/fx.h src/stretch.h src/practice.h third_party/minimp3.h src/store.h src/movie.h third_party/pl_mpeg.h src/battle.h src/cheats.h src/tutorial.h
SRC3D := src/r3d/r3d.c src/r3d/r3d_zone.c
HDR3D := src/r3d/r3d.h src/r3d/r3d_gl.h src/r3d/r3d_zone.h

all: build/gh3 build/gh3-3d

build/gh3: $(SRC) $(HDR)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -Ithird_party -o $@ $(SRC) $(SDL_LIBS) -lm -lpthread

build/gh3-3d: $(SRC) $(HDR) $(SRC3D) $(HDR3D)
	@mkdir -p build
	$(CC) $(CFLAGS) -DGH3_3D $(SDL_CFLAGS) -Ithird_party -Isrc -o $@ $(SRC) $(SRC3D) $(SDL_LIBS) -lm -lpthread -ldl

arm64:
	bash scripts/build_arm64.sh

port: arm64
	bash scripts/make_port.sh

clean:
	rm -rf build

.PHONY: all arm64 port clean
