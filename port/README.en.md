# Guitar Hero III (native) for the R36S

English | [Português](README.md)

A native port of Guitar Hero III for the R36S and other RK3326 handhelds running PortMaster (ArkOS, dArkOS, ROCKNIX, AmberELEC).
It is not emulation or box64: it is a player written in C that reads the game's original files (the PC version) and draws
through the device's GPU (SDL2 + GLES2), with the game's own menus, art and fonts, and the game's stages, band and crowd
in 3D behind the highway. The note-hit rules (windows,
hammer-ons, sustains, star power, multiplier, rock meter), the scoring and the stars follow the game's own,
as worked out in the browser port.

The game starts in Portuguese. To switch to English, go to **Options > Video > Language**.

## What you need

- An R36S with PortMaster installed.
- The `DATA` folder from your PC copy of Guitar Hero III. These subfolders are read:
  - `PAK/` (song list, career and tuning: `qb.pak.xen` + `qb.pab.xen`) - 240 MB
  - `SONGS/` (the charts: `*_song.pak.xen`) - 380 MB
  - `MUSIC/` (the audio: `*.fsb.xen` + `*.dat.xen`) - 2.1 GB
  - `STREAMS/` (the menu music and the guitar riffs: `streamall.fsb.xen`) - 85 MB, optional
  - `MOVIES/R36S/` (the game's videos in MPEG-1, `*.mpg`) - 200 MB, optional. They are made on a PC from the MP4s in
    `MOVIES/WEB/` with `tools/convert_movies.sh DATA` (needs ffmpeg). Only this subfolder is read; without it the videos are skipped.
    The four bonus videos (`brettmichaels`, `sexpistols`, `slash`, `tommorello`) are 115 MB of that total.
  - `ZONES/global_sfx.pak.xen` (the game's sound effects: menus, wrong notes, star power, attacks, crowd) - 6 MB, optional.
    Without it the effects are synthesized.
  - `ZONES/global.pak.xen` + `global.pab.xen` (the game's 2D art: notes, frets, HUD, menu backgrounds and fonts) - 83 MB, optional.
    Without them the game runs with a plain fallback look.
  - `ZONES/` (the 3D venues, `z_*.pak.xen`) - 225 MB. Without them the game shows a 2D stage instead. The rest of
    `MOVIES/` is not used and can be left out to save card space.

## Installation

1. Copy `GH3.sh` to `roms/ports/` (or `roms2/ports/`) on the card and the `gh3/` folder to `roms/ports/gh3/`.
2. Copy the game's `DATA` folder to `roms/ports/gh3/DATA/` (with at least `PAK`, `SONGS`, `MUSIC` and `ZONES` inside).
3. Refresh the game list (or reboot) and open **Guitar Hero III** in the Ports menu.

If you had the previous version, you can delete `GH3 3D.sh` from `roms/ports/` and `gh3-3d.aarch64` and `log-3d.txt` from `gh3/`:
`GH3.sh` now opens the game in 3D already. Your saves stay the same.

## The 3D stage

Behind the highway the game draws the game's own venue in 3D, read from `DATA/ZONES/z_*.pak.xen`: the meshes and textures
of the set's stage (the bar, the inferno, the Budokan...), with the game's cameras cutting according to each song's
cut list (`<song>_cameras_notes`) and the win and lose cameras at the end. In the menus, the last stage loaded wanders through
the game's establishing shots. On the stage:

- **The game's band** (**Options > Video > 3D Band**), with the rocker, outfit and guitar picked in "Select Your
  Hero", the win and lose poses at the end of the song, and the attacks and reactions in the battles. The singer follows the
  song (no singer in instrumentals); any models missing from your copy of the game are replaced by the default.
- **The game's crowd**, which changes with the rock meter, raises its arms during star power and lights lighters on the
  songs that call for it.
- **The light show** (beams, color, strobe, fire), which follows the music and lights up the stage, the band and the crowd.

**Options > Video > 3D Stage** turns the venue off (back to the 2D stage), **Stage Effects** turns off the lights, the
strobe and the crowd, and `Dim the Background` still applies over the venue. **Options > Video > 3D Stage Resolution**
chooses the size the venue is drawn at before being scaled up behind the highway: High (640x480), **Medium**
(480x360, the default) or Low (320x240, the lightest); the highway and the scoreboard always stay sharp. On the R36S, at Medium, it
runs at 58.5 frames per second (the panel's refresh rate). The first time a stage opens, its textures are compressed (ETC1) and
stored in `cache-3d/` in the port's folder; on later runs it opens faster. Deleting `cache-3d/` only makes the
next load rebuild it. While a song plays, `log.txt` gets an `r3d perf:` line every 10 s with
frames per second, average and maximum frame time, venue time, draw calls, triangles and texture memory. Without `ZONES/`, or when the device renders in software (`SOFT_RENDER=1`), the game uses the 2D stage on its own.

## Main menu

Just like the game's: a title screen (press Start, a guitar riff plays) and the menu with **Career**, Co-op Career,
**Quickplay**, Multiplayer, **Practice** and **Options**. Co-op and multiplayer show up, but they need two
players and do not open on the R36S. The 11 menu songs from the game itself play in the menus, read from `STREAMS/` (without that
folder the menus are silent).

The menus and the game's texts exist in Portuguese, English and Español. The game starts in Portuguese; to switch, go to
**Options > Video > Language**. The choice applies immediately and is saved in `gh3.cfg`.

- **Quickplay**: any song already unlocked in the career, on any difficulty. The high score counts, the career does not change.
- **Practice > Practice**: pick the sections ("Start at" and "End at") and the speed, as in the game: Normal,
  Slow (80%), Slower (67%) and Slowest (50%). Below normal speed the song plays slower at the same
  pitch, the highway scrolls more slowly and the hit windows stay the same size in song time (wider
  on the clock). You can't fail, there is no Star Power and nothing is saved; at the end, "Change Practice" goes back to choosing the section.
- **Practice > Tutorials**: the game's narrated lessons (basics, Star Power, battle, hammer-ons and pull-offs), rebuilt from
  the game's own scripts: texts, the God of Rock's and Lou's lines in the right order, the highway that freezes
  while they explain, the practice chart and each lesson's goal. The narration is in English (or French, German,
  Italian, Spanish, your pick in the list); the captions, in the language chosen in Options > Video > Language, summarize what each line teaches. The opponent's
  attacks in the battle lesson are not simulated. Completed lessons are marked in `gh3-tutoriais.cfg`.

## Career

The song list is the game's career, unlocked as in the original:

- The 8 sets open in order; each set has 4 songs, a battle in some (Tom Morello, Slash, Lou) and an encore.
- Complete **2 songs on Easy, 3 on Medium and all 4 on Hard and Expert** to unlock the battle and then the encore.
- The encore (or the last battle) closes the set and opens the next one. Progress is separate for each difficulty (← → in the list).
- The list has the game's three tabs, each with its own notebook: **Set List** (the career's sets, then the co-op
  encores and the rest), **Bonus** and **Downloads**. L1 and R1 move from one tab to the next.
- The battles are like the game's (in Career and Quickplay, on lead guitar): the boss plays rhythm guitar on the right
  highway. Completing a phrase of battle notes gives you an attack (up to 3); the star power button fires the top one.
  Attacks: amp overload (the highway shakes and the notes blink), broken string (press the broken fret until
  it is fixed), difficulty up, double notes, lefty flip, whammy (move the whammy bar until the frets are freed) and
  power drain. The meters only move during an attack; whoever makes the other fail wins. Against Lou, near the
  end comes the Death Drain (you have to knock him down before it), and after he is beaten he still has to be "finished off" in the rest
  of the song. Before the battle the boss's line plays (from `STREAMS/`); A skips it.
- Before the encore the crowd calls for more (the game's own "encore" chant, from each stage); A starts the song right away.
- **Money**: the career pays by stars, using the game's table (3/4/5 stars: Easy $75/$175/$300, Medium
  $150/$350/$600, Hard $300/$700/$1200, Expert $450/$1050/$1800). Replaying a song only pays the difference when you
  earn more stars than you had earned on it; Quickplay, Practice and No Fail don't pay.
- **Store** (Y in the career list): the 24 bonus songs ($500 each; they stay locked in every mode until bought) and the
  4 bonus videos ($1500). Through the Fire and Flames can't be bought: it opens when you finish the career. Guitars,
  finishes and rockers were left out (they need the 3D models). If you had already played a bonus song, you keep it.
- Downloaded songs are always open. In Options > Cheats you can unlock everything at once.
- The career videos play as in the game: the intro the first time the career is opened and each set's video
  when you complete it (the last one is the ending). There are also the logos and the game's intro at startup (Options > Video > Intro
  Movies). A or B skips a video, Start skips all the startup ones. They need `MOVIES/R36S/` (see above).

High scores (points, percentage, longest streak and stars) are kept in `gh3-recordes.cfg`, per song, part and difficulty.
Money and purchases are kept in `gh3-carreira.cfg` (the first time, the career already played pays what it would have paid).
A song only counts as completed if you don't fail; in No Fail mode nothing is saved.

## Controls (PS2-style defaults, all configurable in Options > Controller > Buttons)

The default profile follows the PS2 Guitar Hero III with the regular controller (DualShock 2): the frets sit on the shoulder buttons and
orange on X, which on the R36S is the bottom button (B). The "R36S" profile puts the frets
in the order L1 L2 R2 R1, which felt more natural on the device's shoulders; the "PS2" profile is the manual to the letter, button
by button:

| Action | PS2 (DualShock 2) | R36S (default) | PS2 profile |
| --- | --- | --- | --- |
| Green fret | L2 | L1 | L2 |
| Red fret | L1 | L2 | L1 |
| Yellow fret | R1 | R2 | R1 |
| Blue fret | R2 | R1 | R2 |
| Orange fret | X | B (bottom button) | B |
| Strum | D-pad up/down | D-pad up/down | same |
| Star power | Select | Select or X (top button) | same |
| Whammy | left analog stick | either analog stick | same |
| Pause | Start | Start | same |
| Quit the game | | Select + Start together, on any screen | same |

Each action accepts up to three buttons. There are six ready-made profiles (R36S, PS2, the first Guitar Hero's with the face buttons, In a row, Reversed and Triggers) and the controls screen
has a live test. To cancel changing a button, just wait 5 seconds without pressing anything.

The whammy works as on the PS2: while holding a long note, it lowers the guitar's pitch (up to three quarters of a
tone at the bottom), the note's tail wobbles, and during a star power phrase moving the whammy fills the meter. Star power can
be used with the meter at half (the mark in the middle of the bar); the button shows on screen when it is ready.

## Sound lag (important)

The R36S's audio reaches your ears a little later than the game calculates. Before playing, go to
**Options > Calibrate Audio Lag**: a click plays and you strum in time; the game measures the lag and adjusts the notes and the
judging. Without it, the notes look right on screen and still come out late.

## Options (saved in `gh3.cfg`)

Organized like the game's:

- **Audio**: band, guitar and effects volume, menu music, song preview in the list, audio lag.
- **Calibrate Audio Lag**: the measurement described above.
- **Controller**: Lefty Flip, Strumming: Normal (the D-pad, as on the guitar) or Tap only (like GH3 on the regular PS2 controller: pressing the fret button is already the strum, with the same rules and the same wrong-note sound), **Calibrate Whammy** (release the analog stick and press A, push it all the way and press A: the game stores where it rests, the dead zone, and the end of its travel), the whammy **Sensitivity** (50% to 200%) and the buttons.
- The options poster has the game's twelve strips; the ones that are of no use on the R36S (Calibrate Video Lag, Manage Band,
  Save / Load, Videos, the Store) are grayed out and the cursor skips over them. The PC's auto-login strip
  became **Restore Defaults**.
- **Video**: 3D Stage, 3D Stage Resolution and 3D Band, Stage Effects, Dim the Background, Show FPS, Intro Movies and **Language** (Português, English or Español: it switches immediately and is
  saved as `idioma=` in `gh3.cfg`).

The look is the game's own, read from `ZONES/global.pak.xen` + `global.pab.xen`: the notes, the frets, the flames, the highway with
its rails, the rock meter with the star power bulbs, the scoreboard box with the multiplier bulb and the game's
digits, and in the menus the backgrounds (the main menu's hall, the set list's wooden table, the poster wall), the
pause menu's frame and pattern, the button pills and the titles font. The small texts remain in the pixel font.
Without those two files the game draws everything in a plain fallback look.
- **Top Rockers**: your best results on each song, by difficulty.
- **Cheats**: the game's, with the same codes (in "Enter a Code", hold the frets of each chord and strum):
  Hyperspeed, No Fail, Easy Expert (hit windows doubled; not valid in the career or for high scores), Precision Mode
  (windows halved), Unlock All Songs and Unlock Everything. Each one can also be turned on and off directly. Air Guitar
  (the band without instruments) and Performance Mode (no highway or scoreboard, the song plays on its own) change the 3D band; Bret
  Michaels needs `PAK/Singer_Brett.pak.xen`, which not every copy of the game has.

`port.cfg` only has the launcher's options (vsync, clocks, software rendering).

## What is still missing

- Co-op and multiplayer.
- The sound effects are the game's own (`ZONES/global_sfx.pak.xen`, through the game's sound event table: menus,
  wrong notes, lost multiplier, streaks, star power, battle attacks, store, results), with the crowd of each set's
  stage (the murmur that follows the rock meter, boos and applause, the claps during star power, the cheer at the
  end). Without that file the effects are synthesized. The guitar goes quiet when you miss, as in the game.

## Building from source

You need a Linux (or WSL) with `aarch64-linux-gnu-gcc` and an aarch64 sysroot with SDL2 (glibc up to 2.30, ArkOS's):

```
make            # build/gh3-3d (the game) and build/gh3 (the same without the 3D, for tests) for the PC, with windowless modes: --dump, --frames, --screens, --r3dtest
make port       # build/gh3.aarch64 (with the 3D) and the PortMaster zip in build/port/
```

The game is compiled with `-DGH3_3D` plus `src/r3d/` (without the define none of the 3D is compiled). It needs an
SDL2 with `SDL_RenderFlush` (2.0.10+, resolved at run time) and SDL's GLES2/OpenGL renderer; in software
it falls back to the 2D stage.
