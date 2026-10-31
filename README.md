# Native Guitar Hero III for the R36S

English | [Português](README.pt-BR.md)

A Guitar Hero III player written in C for the R36S and other RK3326 handhelds running PortMaster (ArkOS, dArkOS,
ROCKNIX, AmberELEC). It is not emulation: it reads the PC version's own files (songs, charts, career, tuning, art,
venues and characters) and draws them with SDL2 + GLES2 in the game's own look: GH3's menus, fonts and highway, with
the game's venues, band and crowd in 3D behind it. Note judging, scoring, stars and the career's order follow the
original game.

| | |
| --- | --- |
| ![Paint It Black on the 3D stage, with the singer and star power](docs/jogo_3d.png) | ![Barracuda in the bar, with the guitarist and the crowd](docs/jogo_3d_bar.png) |
| ![Main menu](docs/menu.png) | ![Career set list](docs/setlist.png) |
| ![Select Your Hero](docs/roqueiro.png) | ![Results screen](docs/results.png) |
| ![The crowd at the Budokan](docs/palco_budokan.png) | |

The pictures show the menus in Portuguese; the game also runs in English and Spanish (Options > Video > Language).

## Features

- The career unlocks as in the original (tiers, battles, encores), per difficulty, with saved high scores and stars.
- Career cash from the game's own table and the Store: bonus songs and videos, secret rockers, guitars and finishes
  at the original prices; the encore chant.
- "Select Your Hero": the rocker, outfit and guitar that take the stage, as in GH3.
- Battles against Tom Morello, Slash and Lou with the game's attacks, the boss on the highway beside yours, Death
  Drain and the end-of-battle movies.
- Hammer-ons, sustains, star power, multiplier and rock meter with the game's timing windows.
- Practice as in GH3: speeds, sections and, below full speed, the game's drum kit instead of the band.
- Controls match Guitar Hero III on the PS2 by default (frets on L2, L1, R1, R2 and B), remappable, with five
  profiles, plus the PS2 pad's tap mode.
- Whammy on both analog sticks, bending sustains down as on the PS2, with calibration.
- The game's own venues in 3D (the `.scn`/`.tex` files in `ZONES/`): the game's cameras cutting on the song's beat,
  the band playing (with win and lose poses and the attacks in battles), a crowd that follows the rock meter and the
  song's light show. On the R36S it runs at 58.5 frames per second.
- The game's sound effects, the cheats with their fret codes, audio lag calibration, menus in Portuguese, English
  and Spanish, song previews, the game's movies and the narrated tutorials.

## Installation

This repository contains none of the game's files: you need the `DATA` folder from your own copy of Guitar Hero III
for PC. Step-by-step installation, controls and options are in [port/README.en.md](port/README.en.md).

## Building

On Linux (or WSL) with SDL2, `aarch64-linux-gnu-gcc` and an aarch64 sysroot with SDL2 (glibc 2.30 or older):

```
make            # build/gh3-3d (the game) and build/gh3 (the same without 3D, for the tests) for the PC, with headless modes: --dump, --frames, --screens, --stretchtest, --r3dtest, --langtest
make port       # build/gh3.aarch64 (with 3D) and the PortMaster zip in build/port/
```

The device build uses `-DGH3_3D` and `src/r3d/` (the `.scn` scene reader, `.tex`/DDS banks, the zone's cameras and
lights from its `.qb`, and raw GLES2 drawing inside the SDL_Renderer window, before the 2D layer). Without
`-DGH3_3D` nothing from `r3d/` is built in: that is `build/gh3`, which the headless tests use.
`build/gh3-3d --r3dtest z_dive --out DIR` loads a zone in a hidden window, saves frames and prints meshes, textures,
triangles, video memory and ms per frame at the stage's three resolutions (`R3D_NODXT=1` forces the Mali path:
textures decoded and recompressed as ETC1).

Every text the player reads goes through `T("...")`, with the Portuguese as the key. English and Spanish are in
`src/lang/`, one row per text (`{ "pt", "en", "es" }`), and `--langtest` checks them. The Spanish is a plain
translation: native speakers are welcome to improve it.

The MP3 decoder is [minimp3](https://github.com/lieff/minimp3) (CC0) and the video decoder (MPEG-1 + MP2) is
[pl_mpeg](https://github.com/phoboslab/pl_mpeg) (MIT), both in `third_party/`.
`tools/fixes/` holds fixes for other ports used on the same device.

## License

This project's code is free software under the [MIT license](LICENSE). The libraries in `third_party/` have their
own licenses (minimp3: CC0; pl_mpeg: MIT). The license covers the code only: the game's files, songs and movies
remain Activision's and are not part of this repository.

Guitar Hero is a trademark of Activision. This project is not affiliated with Activision or RedOctane.
