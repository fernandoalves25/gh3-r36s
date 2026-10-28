# Host build (Linux / WSL): make            -> build/gh3
# R36S build:               make arm64      -> build/gh3.aarch64 (needs SYSROOT, see scripts/build_arm64.sh)
CC ?= gcc
CFLAGS ?= -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-misleading-indentation -std=gnu11 -D_GNU_SOURCE
SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS := $(shell sdl2-config --libs 2>/dev/null)
SRC := src/main.c src/gh3fmt.c src/audio.c src/game.c src/fb.c src/gfx.c src/render.c
HDR := src/gh3fmt.h src/audio.h src/game.h src/fb.h src/gfx.h src/render.h third_party/minimp3.h

build/gh3: $(SRC) $(HDR)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -Ithird_party -o $@ $(SRC) $(SDL_LIBS) -lm

arm64:
	bash scripts/build_arm64.sh

clean:
	rm -rf build

.PHONY: arm64 clean
