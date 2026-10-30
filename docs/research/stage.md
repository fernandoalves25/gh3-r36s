# GH3 stage direction (cameras, lightshow, venue FX) - recipe for the C/GLES2 port

Sources: browser port `F:\games\gh3\index.html` (line numbers below are in that file), repo `src/gh3fmt.c`, and
the real data (decoded with `src/gh3fmt.c` + `tools/qbgrep.c` built in WSL, and the port's GH3 reader run under node).
Helper tools used for these notes (not kept):
`gh3lib.js` (= lines 434-1300 of index.html, `require`-able in node), `venue.js`/`venue2.js`/`songs.js` (stats), `qbg2` (qbgrep that
takes `-` as "no pab" and also reads `.nqb`), `pk.py` (pak lister), dumps `zone_<z>.txt`, `out/<song>.txt`.
"PORT" below = a choice made by the browser port (a reconstruction), "DATA" = read from game files.

## 0. The big picture
- The stage is DATA-driven, not code-driven. Three data sources:
  1. per-song cue arrays in `<song>_song.pak.xen` (cameras/lights/crowd/drums/anim/performance) = WHEN;
  2. global tables in `DATA/PAK/qb.pak.xen` (`cameracuts_notemapping`, `lightshow_notemapping`, `lightshow_coloroverrides`,
     `lightshow_coloroverrideexcludelights`) = WHAT a cue note number means;
  3. per-venue `DATA/ZONES/z_<venue>.pak.xen` (geometry, light list, light "snapshots", camera shots, particle/pyro scripts) = WHERE/HOW.
- Beat-pulsing of the lights is NOT code: it is the song's own "next preset" cue notes (57/58, ~350 per song, e.g. every 1/2 beat in intros,
  every 1-2 beats later). The only code-driven pulse is a 4% brightness lift `1 + 0.04*(1-frac(beat))^2` (L7313, `bgBeat` L6668 pulse = `pow(1-(b%1),2)`).
- Rock-meter level picks the preset family: `level = health<0.66 ? 0(poor) : health<1.33 ? 1(medium) : 2(good)` (bgBeat L6672; the same thresholds in crowd audio).
  Star power: multiply the room tint by (0.55,0.85,1.35) and the floor ambient by (0.5,0.75,1.2) (L7313-7315).
- Venue per song: the career tier's zone (`levelzones` + tier `levelKey = load_<zone>`; `songZone` L7509). The repo already reads `tierZone[]` into `gh3_shop` (gh3fmt.h:45).
  Venues present: z_artdeco z_budokan z_dive z_hell z_party z_prison z_video z_wikker (+ z_soundcheck, z_credits, z_viewer debug-only; soundcheck has only 3 cams, no show).

## 1. Song pak arrays (WHEN). What exists vs what `gh3_songpak_read` reads today
`gh3_songpak_read` (gh3fmt.c:305) reads ONLY: `_fretbars _timesig _markers _song[_rhythm|_guitarcoop|_rhythmcoop]_<diff> _<diff>_star
_starbattlemode _scripts`. Section ids are `qbkey("<song>_<suffix>")`. NOT read yet (all exist in all 158 song paks, JS reader `readSongPak` L743-776):

| section suffix | layout (all ints, ms) | JS | use |
|---|---|---|---|
| `_cameras_notes` | array of `[time, midinote, dur]` | L775 `track()` | camera cut list. 0-112 cuts/song, median 51; gap between cuts median 4.0 s (min 0.7, max 23); `dur` ~= time to next cut |
| `_lightshow_notes` | `[time, midinote, dur]` | L775 | median 579 cues/song, max 3518. 3rd field is a constant-ish tag (e.g. 59/55), ignored |
| `_crowd_notes` | `[time, midinote, dur]` | L2230 | audio only (applause 72-80); not visual |
| `_drums_notes` | `[time, note, dur(,vel)]` | L7274 | drummer hits; note 36 = kick pumps venue "drum front" (`venue_pulsedrum*`) |
| `_anim_notes` | `[time, note, dur]` | L7275 | fretting-hand poses (101-110 bass, >=118 guitar) |
| `_performance` | array of structs `{time, scr, params{name,anim,stance,node,flags}}` | L767 | band events: `band_changestance`(2015), `band_playanim`(1819), `band_playfacialanim`(674), `band_walktonode`(96), `crowd_startlighters`(53), `crowd_stoplighters`(51) over 158 songs |
| `_scripts` | `{time, scr}` | L774 | already read by C (`cues`), empty in most songs |
| `_faceoffp1/p2` | `[start,len]` | L776 | face-off sections |
Cue arrays are sorted by time already (port re-sorts). In C use `arrsec(...)` + `arr_int(e,k)` like `_star`.
Reset rule (port): when song time jumps back > 1500 ms, restart pointers (restart/practice).

## 2. Cue note meaning tables (DATA, qb.pak.xen; read by JS readLibrary L676-684)
### 2a. `cameracuts_notemapping` (39 entries `{midinote, scr, params{prefix, Type, NAME}}`)
scr 0xe4de9864 = "cut to a shot from list `<zone>_<prefix>`" (hash unnamed; 0x298e1df4 flag param unnamed). Notes 77/78 = scr 0x626438e0 `enable=true/false`
(never used in the 158 songs; PORT ignores). Notes actually used: 82-117 (counts over all songs in brackets).
```
79 cameras            80 cameras_intro        81/90 cameras_solo      82 orbit(NAME 0xf9784194)
83 cameras_zoom       84 cameras Type=closeup [1471]   85 cameras Type=mid [1030]   86 cameras Type=closeup [677]
87 cameras Type=Stage [792]   88 cameras_singer [638]   89 cameras_drummer [412]
91 orbit NAME 0xf9784194  92 orbit NAME 0x32998951  93 orbit NAME 0xfeab597e  94 orbit NAME 0x665844b0  103 orbit NAME 0x89837c69
95 cameras_guitar_closeup (NAME 0x1c28fd2a) [349]   96 cameras_starpower [34]   97 cameras_bassist [314]   98 cameras_walk
99 vert_guitarist  100 vert_drummer  101 vert_stagefront  102 vert_stagerear   104 cameras_singer_closeup [535]
105 cameras_special  106-109 cameras_special NAME=f38b1ab0/6a824b0a/1d857b9c/83e1ee3f   110 cameras Type=Stage
111 zoom_slow [119]  112 zoom_fast [37]  113 zoom_out [106]  114 pan [24]  115 boss_closeup  116 player_closeup  117 stagedive
```
Type key 0x8942a372 == qbkey("closeup") (I brute-forced); "mid", "Stage" are plain names; a 4th type 0xf3c83bbe exists on some shots, no note asks for it.
### 2b. `lightshow_notemapping` (35 entries)
```
76 intro  75 verse  74 chorus  73 solo  72 mood 0xf3b3b42a  71 Flare  70 strobe      -> scr 0x544f16f2 (set mood)  [moods]
69 GREEN(66,228,97) 68 RED(255,0,0) 67 BLUE(20,132,247) 66 YELLOW(252,227,61) 65 white 64 magenta(240,79,255) 62 ORANGE(248,142,56) 61 purple(162,80,232)
63 = override OFF (restores the preset colours)                                          -> scr lightshow_overridecolor
60 Event strobetoggle    58 Event snapshotchange(flag true)   57 Event snapshotchange(flag false)   56 scr lightshow_pyroevent
53..40 scr lightshow_settime Time = 1,.9,.8,.7,.6,.5,.4,.3,.25,.2,.15,.1,.05,0 (seconds)   39 settime Default (=0.15 s)
```
Cue frequency over all songs: 57:36537, 58:18364, 60:15645, 56:861 (pyro), moods 70-76 ~1.5-4.5k each, colors 61-69 ~1-3.8k each.
Colours are the 8 entries of `lightshow_coloroverrides` (/255). `lightshow_coloroverrideexcludelights` (80 light keys) = lights that keep their preset colour when overridden.
PORT treats 57 and 58 identically (`pi++`); the bool param 0xd6f41e9a (true/false) is unnamed - probably next/prev, unverified.

## 3. Light show state machine (PORT: `lightShow()` L6780-6827, copy this)
State: `mood`(init intro) `level`(0..2) `pi`(preset index) `blendT`(0.15) `over`(null|rgb) `strobe` `mix/dur`. Per light: `cur/from/to` = [r,g,b,intensity,farAtten]. Per beam same with 13 values.
- `listFor()`: `venue.show.moods[mood][level]` -> list of preset keys (fallback: verse, then first); current preset = `list[pi mod len]`.
- Cue loop (while `cue.t <= ms`): mood note -> `strobe = (mood==strobe)`; if mood changed: `mood=...; pi=0; apply(blendT)`.
  colour note -> `over = rgb|null; apply(0.4)`; settime -> `blendT = t` (default note -> 0.15); strobe toggle -> flip; snapshotchange -> `pi++; apply(blendT)`;
  pyro -> `pyro++` only if the cue is < 600 ms late.
- `apply(time)`: copy cur->from; to = preset values (lights missing from the preset fall back to the venue's own base values); if `over` set: to.rgb = over for every non-ambient light
  not in the keep set; beams likewise (volume colour). `dur=time; mix=0`, then each tick `mix += dt/dur; cur = from + (to-from)*mix` (LINEAR). time 0 = instant.
  A beam that was dark (density < .002) snaps to its new position/direction instead of swinging.
- Rock-meter level change -> `apply(0.5)`.
- Strobe: PORT softens it to `flick = 0.72 + 0.28*cos(t*2pi*2.5)` multiplying all non-ambient light intensity, beam power and flares. The game's own is a hard flash several times/s (not measured).
  The songs toggle note 60 every ~58 ms through a run (beforeiforget: 15155-16268 ms). The native port (r3d_lights.c) flashes on an on-toggle:
  x1.7 and 45% toward white for 34 ms, down to x0.3 over 70 ms, x0.3 until the next; flashes at least 200 ms apart (every other on-toggle, 4.3/s);
  a strobe mood with no toggles flashes on the beat.
- No-song idle (menus): verse presets, `pi++` every 8 beats, blend 0.8 s.
- Not done by PORT: FAIL / WIN moods exist in the mapping (`FAIL.Default`, `WIN.Default`) - used by the game at song end; PORT only switches camera lists there.

### Per-venue data (DATA, `z_<v>.pak.xen` .qb sections; JS `venueShow` L1203-1219)
- `z_<v>_lightshow_mapping` = struct: `<mood> -> {poor|MEDIUM|good -> {snapshots = [keys] or key of a list, 0xd63fb9af = unnamed (unused by PORT)}}`,
  `transitions{performance, mood}` (scripts), `FAIL`, `WIN`, `0xcf465431 = $z_<v>_pyro`. Moods = intro verse chorus solo 0xf3b3b42a Flare strobe (+FAIL/WIN): 9 per venue.
  poor level always uses 1 dim preset; medium/good share lists (verse: 5 presets, chorus: 5, intro 1-3, solo 2, strobe/Flare 1).
- `z_<v>_snapshots` = struct: `<presetKey> -> [ {lightchecksum, r,g,b (0-255), intensity, 0xb9832749 (=intensity copy), 0x427fbaac, farattenend}  x ~28 lights,
  {housingchecksum, pos(x,y,z), quat(x,y,z; w=sqrt(1-x2-y2-z2)), startradius, endradius, innerRadius, Range, volumedensity, volumecolorred/green/blue(0-255)} x ~23 beams ]`.
  Venues have 10-16 presets (artdeco 16, budokan 13, dive/hell/wikker/video 10, party/prison 11).
- Lights = `class=$levellight` structs in the `.nqb` (type 0x49875607) entry: `{NAME, pos, Type($ambient|$dir|$point; $dir structs also carry hotspot/Field = spot cone),
  0x786e947e quat(4), lightColor(0-1 rgb), intensity, attenstart, attenend, hotspot, Field, lightgroup($band|$alt_band|$crowd|$None), flag createdatstart}`.
  18-37 lights/venue; kinds artdeco: 7 ambient + 22 dir (7 band group, 2 crowd). Groups: band lights light the band; crowd lights tint crowd cards; ungrouped light the room.
- Beams/fixtures = `class=$gameobject` with `model="LightHousings\GO_xxx.mdl"` (+`lightvolumetype`, `volumedensity`, `volumecolor`, `Range`, start/endradius): 11-76 per venue
  (`venueHousings` L1111; the housing models live in the shared global pak: `.mdl` 0x9bcc234d + `.tex`).
- Example artdeco "verse/good" preset 0: brightest room lights (255,246,184 i1.09 far32), (255,10,10 i1.0 far22), (255,126,24 i1 far55); 14 of 23 beams lit.
  "chorus" preset 0: (254,255,197 i2.0), (255,181,122), (120,218,255); solo: reds (255,10,10 i2.0) / purples (177,10,255 i2). So colours are authored - read them, don't invent.

### How lights are applied (PORT `sceneVenue.tick` L7297-7315; `venueShade` L6853-6910)
Venue meshes are shaded with the game's model: `c = tex * material.colour * vertexColour * (mainLight*max(n.l,0)*mix(1,lightmap,1) + soft*mix(1,lightmap,1) + ambient)`.
`lightmap` = baked 2nd-UV texture (`uvLast/uv2`, `smap`); the baked pools make the venue look lit even with dim dynamic lights. Per mesh (CPU, ~300 meshes, once per frame, `shade.light` L6891):
for each room light (ungrouped, max 32): `att = clamp((far - max(0, dist - meshRadius)) / (far - near))`, `k = intensity*att*(strobe flick)`; ambient lights accumulate into `lAmb`;
others are sorted by max(rgb*k): strongest = main directional (dir to light for point lights, `-dir` for dir lights), the next 3 = "soft light" packed as
`sh0 += c*.25; sh1..3 += c*.5*dir.xyz` (hemisphere-ish SH): `soft = sh0 + sh1*n.x + sh2*n.y + sh3*n.z`. Unlit materials (list `VENUE_UNLIT` L6847: signs, glows, sky, Wibble*, Flare, Fog) = tex*vertexColour only.
Band lights drive real three.js lights on the band (intensity = cur[3]); the band shadow = flatten matrix along the named light's dir, black 35% (`shadowDark` from script `tod_manager_create_perm_lights`).
Crowd cards tint: sum of crowd-group lights (ambient all-round + key from front).
Beams: additive open cone mesh (`venueBeam` L7082): `r0->r1` radius over `Range`, alpha = (1-along)^2 * (|n.v|)^1.7 * power, power = min(1.2, density)*.9*flick, colour = volumecolor; drawn only when density>.004. Fixture models are moved/aimed to the beam's pos/dir and their `Flare` billboard coloured `c*min(1.5,density)*flick`.

## 4. Camera system
### Selection (PORT `sceneVenue` L7197-7295; DATA tables in 2a)
1. Build shots per venue: every array-of-structs section in the zone .qb entries with `cam1`/`cam2` sub-structs (or direct `pos`+`quat`) is a shot; section id = list name
   (`z_<v>_cameras`, `_cameras_solo|guitar_closeup|drummer|singer|singer_closeup|bassist|orbit|pan|dolly|zoom_slow|zoom_fast|zoom_out|intro|walk|starpower|special|stagedive|win|lose|vert_*`;
   unnamed hash lists, e.g. `0xa6e4e1f5` = same shots with `PLAYER = 1` (2P/co-op), are only used if nothing else matches). ~40 shots in `_cameras`, 2-12 in each other list; 111-148 per venue overall, ~55% two-point.
2. Per cue i with time `T_i`: `m = camNotes[note]`; candidates = `list(zone_prefix)` filtered by `Type`/`NAME` if the table gives them (fallback: whole list, then `_cameras`, then general);
   pick `k = (imul(i+1, 0x9e3779b1) >>> 8) % n`; if same as previous shot and n>1 -> `k+1`. (PORT hash; the game's real choice rule is unknown.)
   A cue that picks the shot already showing leaves it running. Notes not in the table (and 77/78) are skipped.
3. Duration: shot runs from `T_i` to the next cue time that maps to a camera (else `T_i + max(4000, dur_i)`); `u = clamp((ms - t0)/(t1 - t0))`.
4. No camera cues (no song / 0-cut songs): every 16 beats (`VENUE_SHOT_BEATS`) a shot from the "general" lists (cameras, orbit, pan, dolly, zoom_slow, zoom_out, singer, drummer, bassist, guitar_closeup) in a fixed shuffle.
5. Song end (win/lose): lists `z_<v>_cameras_win|lose` (1 orbit shot each, orbitangle -10/-20, lockto guitarist), new shot every 4 s. `cameras_starpower` only via note 96.
   (qb.pak, checked: the band's win / lose event arrays (0xf598ae8e / 0xc7fea388) run play_win_anims / play_lose_anims and script 0x6bd5485e
   = `cameracuts_setarrayprefix prefix = "cameras_win"|"cameras_lose" changenow`. Every venue but z_soundcheck has both lists, one shot of kind
   0xe2ecb8e2 (orbit-lerp: cam1 -> cam2 over the shot's time, turning orbitangle about the guitarist). A shot's time (script 0x1590418e): the
   next camera cue's, else Random(3, 8) s. r3d.c cuts to the list as the band takes its stance, 3-8 s a shot by the cue hash.)
6. Song end, highway: GuitarEvent_SongWon_Spawned / SongFailed_Spawned start the movie (Player1_wins...) and the words first, then spawn
   `waitandkillhighway` = wait 0.5 s, Crowd_Fast_Surge_Cheer, hide the highway and HUD at once (a failed normal song hides it at once).
6. Filters: drop shots whose `lockto` member is not on stage; drop FOV<45 close-ups when there is no band; drop shots with geometry within 1.2 m in front of the lens (ray test, `open()` L7226). Cheap to precompute at load.

### Shot data (DATA)
Shot struct: `{NAME, Type, DOF=<key e.g. DOF_CloseUp01/Medium0x/Off>, 0xd2bf3d04 = kind hash, params{ cam1{lockto,pos,quat(3 floats),fov,lookAt,0x51caf9f7=(look offset)}, cam2{...}, orbitangle, ...}}`
Kinds (0xd2bf3d04): 0x7eccc7e6 = single static; 0x607ca4eb = lerp cam1->cam2; 0x732ef761 = orbit around member; 0xfcc154ce = pan (cam1/cam2 look at different members); 0xe2ecb8e2 = orbit-lerp (win/lose/special); 0x7d5ad5f2 = 3-point special.
- `lockto = $World` -> world coords; `$guitarist|$bassist|$drummer|$vocalist` -> pos/quat are relative to that member's holder (add holder pos, rotate by holder yaw).
  Solo-list shots also carry bone attach (`0x1c724d65 = $bone_guitar_neck`); PORT ignores the bone (places relative to the holder) - small gap.
- `quat` stored as (x,y,z); `w = sqrt(max(0,1-x2-y2-z2))`; then multiply by quaternion (0,1,0,0) (180 deg about Y: engine looks +Z, GL looks -Z) = `venueQuat` L6766.
- `lookAt = $guitarist` + offset: ignored by PORT (stored quat is already aimed); matters only when the band walks.
- FOV: stored value is the horizontal FOV of a 4:3 picture: `vfov = 2*atan(tan(fov/2)*0.75)` (L7224). Range seen 10-104 deg; default 72 when missing/out of 5..150. Close-ups 13-36, zoom lists go 73->31 (cam1 -> cam2).
- Motion (PORT `place` L7218-7227): two-point: `e = smoothstep(u)`; `pos = lerp(p0,p1,e)`, `quat = slerp(q0,q1,e)`, `fov = lerp`. Single: fixed + tiny sway `rotateY(sin(t*.23)*.012)`.
  Orbit: rotate position (and orientation) about Y by `orbitangle_deg * u` BEFORE applying the member's holder transform. Orbit angles seen 10-120 deg.
  Camera near/far 0.05/3000, aspect = screen.
- DOF per shot (screen_fx `{on, backdist, strength, blurradius}` from qb `DOF_*` sections): PORT draws the scene again at 1/4 res + 2-pass blur + depth mix (L6943). Skip.

## 5. Venue effects
- Particles (DATA): `class=$particleobject` structs in .nqb (hell 4, party 8, video 8, wikker 8; others 0 placed) - fire/smoke/sparks; fields (`particleParams` L1134): pos, emitrangedims, docircularemit,
  emit_target (aim), anglespread/elevspread, force (gravity-ish accel), liferange, emit_rate (cards/s), lifetime, sizerange [(start),(end)], speedrange, rotvel, `particlecolor` 3 colour pairs (start/mid/end, per-particle mix, 0xRRGGBBAA)
  + vertexweight, `quickmaterial{diffuseTexture, texturelayout (1x1..8x8 sheet), quadanimationfps, blendMode Add|Blend|..., alphaCutoff}`, lod_distances, alignwithpath. Textures come from the shared global pak (`particleTextures` L7571), not the zone `.pfx`.
  Typical: hell flames 12-48/s, life 0.8-2 s, size 2-4, 2x2 sheet @12 fps, additive; party/video smoke 4-80/s blended.
  PORT renderer `venueFx` L6975: one instanced quad batch per effect, <=700 cards, billboards, CPU sim. Crowd-mood script `z_<v>_performance_changed` switches emit rates / safecreate / safekill / spawnscriptlater per level (L1168).
- Pyro (DATA): script `z_<v>_pyro` = `switch performance: case medium/good: createparticlesystem_fast NAME=<particle def> pos=(x,y,z) ...` repeated. Fired by light cue note 56 only
  (861 cues in 158 songs ~5/song), only at level>=1 (poor does nothing; `pyro[0]` empty). artdeco: 6 fountains along the stage front (rate 200/s for lifeTime 0.3 s -> ~60 cards each, life 1-1.3 s, speed 6-8, spread 15 deg, force (0,-7,0), size .1->.05, additive orange 0xff731aff); budokan 5/9 jets (150/s, 1 s, life 3-4 s); hell 4/7 (15/s, 2x2 flame sheet);
  prison 4, video 6; dive/wikker: no pyro script. => burst of a few hundred additive quads per cue; trivial.
- Venue "moving things" (L7036 `venueMotion`): per-object qb scripts (`obj_rotx/y/z speed angle`, `obj_movetorelpos`, `wait`, begin/repeat) for lanterns/imps/etc, plus PULSE events
  (`venue_pulsegreen|red|yellow|blue|orange|any|drumleft|drumright|drumboth`): object scale x1.12 (x1.1 for drums) for 0.1 s when a note of that colour is hit / the kick lands. Loop count ~ tens per venue. Optional.
- Crowd lighters: `crowd_startlighters` / `crowd_stoplighters` performance events show/hide `<zone>_lighter_geo01..24` (while the crowd's mood is not BAD); each group
  sways by its own TriggerScript, obj_movetorelpos (+-0.2/0.3, 0, 0) at speed 0.2 and back (band.md section 7; the browser port's `.1*sin` was a stand-in).
- Glow/bloom (L6912): materials with `glow & 1` rendered alone (everything else black) at 1/4 res, 3-tap-pair gaussian 2 passes, added with k = 1.1. Flares + signs. Skip or fake with additive billboards.
- Stage floor / geometry: `.scn` (0x2c3b5adc) + `.tex` (0x8bfa5e8e, "facecaa7" container) pick the entry whose `nameKey == qbkey("z_<v>")`. JS `readScene` L976, `decodeDDS` L935. The repo has NO scn/tex reader yet.

## 6. Zone pak entry types (z_artdeco; same shape in every z_*.pak.xen; no .pab; paklist output)
| type | what | evidence |
|---|---|---|
| 0x2c3b5adc | `.scn` scene geometry (5.3 MB venue + tiny extras; "faaabaca" fill) | = ~crc32(".scn"); parsed by `readScene` |
| 0x8bfa5e8e | `.tex` texture bank (13 entries, 22 MB in artdeco; magic 0xfacecaa7) | = `.tex` |
| 0xa7dea591 | `.pfx` (3.9 KB + 1.2 KB): magic 0xed0ede12, then `(key, offset, count)` triples. Particle-fx material/texture index; NOT used by the browser port (it resolves particle textures from the shared global files) | = `.pfx`; contents not decoded |
| 0xa7f505c4 | `.qb` (4 entries): camera lists, snapshots + lightshow_mapping, scripts | qbkey(".qb") |
| 0x49875607 | `.nqb` node arrays: levellight, gameobject (light housings), particleobject, levelobject, waypoint (band start points `z_<v>_trg_waypoint_<role>_start`) | = `.nqb` (port's K_NQB) |
| 0x8a20e0f8 | **`.hkc`** Havok 4.6.0-r1 collision packfile (`57e0e057 10c0c010 'hknx'` header, "Havok-4.6.0-r1"). Not needed | brute-forced crc32(".hkc") |
| 0x43904241 | **`.table`**, 4 bytes (`e5f50254`), one per pak; a per-zone checksum/id. Not needed | brute-forced crc32(".table") |
| 0x64112e85 / 0x0a6808d4 | `.skin` models (crowd/prop) / `.wav` (qbkey(".wav")=0x0a6808d4: ambient sfx?) | `.skin` in the port; wav by crc |
All keys are `~crc32(lowercase ext)`; entry names are not stored (flags 0x20 absent for the PC game; `nameKey` = qbkey of the entry's base name, e.g. `z_artdeco`).

## 7. Cost sheet for a Mali-G31 / GLES2 / 640x480
Measured (node + port reader): venue geometry 35k-71k tris, 22k-98k verts, 194-379 meshes, 56-97 materials per venue (soundcheck 18.5k tris).
- CHEAP (do): song cue arrays; light state machine (CPU, 20-37 lights x 5 floats, 23-76 beams x 13); camera shot list + `place()` math; per-mesh light packing on CPU (300 meshes) -> 8 vec3 uniforms/mesh;
  static venue draw with lightmap * vertex colour * texture (1-2 texture fetches, no normal map); additive pyro/spark quads (<=700/effect, CPU sim); additive cone beams (23-76 open cones, 20 sides; draw only lit ones, ~10 per preset); crowd tint multiplier.
- MEDIUM: band skinned meshes (4 musicians), crowd cards (animated billboards redrawn every 2nd frame in PORT), animated/wibble materials (UV scroll in the vertex shader), moving props.
- EXPENSIVE (skip first): glow render-to-texture + blur, per-shot depth of field (extra scene pass + depth texture), band planar shadows with stencil, per-fragment multi-light (do per-mesh uniforms as PORT does), hard strobe flashing (photosensitivity; keep the 2.5 Hz soft pulse).
- Texture memory: zone .tex 14-22 MB per venue as DXT; needs on-device DXT1/5 decode or S3TC extension check (Mali-G31 has no S3TC in GLES2 usually) -> decode to RGB565/RGBA4444 at load.

## 8. Recommended minimal first step (no venue geometry needed)
Goal: "authored lights + camera cut list that follow the song", in <300 lines of C and ~0 GPU cost.
1. `gh3_songpak_read`: add `int (*cam)[3], ncam; int (*lite)[3], nlite;` from `<song>_cameras_notes` and `<song>_lightshow_notes` (`arrsec` + `arr_int`, like `_star`).
2. Table loader: from qb.pak read `lightshow_notemapping`, `cameracuts_notemapping`, `lightshow_coloroverrides` into `note -> {kind, mood|rgb|time|prefix,type}` (or hard-code the 39+35 rows in section 2: they are the same for every song).
3. Port `lightShow()` verbatim as a C struct with NO lights list yet: read the venue's `z_<v>_snapshots` + `lightshow_mapping` (qb arrays/structs; 10-16 presets) and keep only, per preset, the 3 strongest non-band lights `(r,g,b,intensity)` -> 3 "stage lights" that
   cross-fade with the `blendT` rule above, cycle on notes 57/58, change family on mood notes 70-76, tint on 61-69, soft-strobe on 60, level = poor/medium/good from the rock meter.
   Draw them as 3 additive radial gradients / cones behind the highway or on the game background (one 64x64 alpha texture, 3 quads, blended `GL_ONE,GL_ONE`). This reproduces the authored colour changes in sync with every song; that is the whole "lightshow follows the song".
4. Camera cut list: parse and log it (time, note, list/Type) so the 3D step can consume it; the camera logic is only `cutTo()`/`place()` (50 lines, section 4) and needs the venue geometry (`.scn` + `.tex` reader, the first real 3D work: `readScene` L976-1051, `decodeDDS` L935).
5. Next steps in order: venue geometry static + shots + per-mesh light packing (section 3) -> pyro bursts on note 56 (section 5) -> beams -> band/crowd. Leave glow, DOF, shadows out.

## 9. Gaps / unverified
- Game's real random choice among candidate shots, real ease curves, real strobe rate, meaning of snapshotchange bool 0xd6f41e9a, of mapping key 0xd63fb9af, of mood key 0xf3b3b42a, Type 0xf3c83bbe, `.pfx` internals: not decoded (PORT guesses where noted).
- PORT does not use: cut-enable notes 77/78, FAIL/WIN light moods, lookAt, bone-attached solo cams, `cameras_walk`, `cameras_stagedive`, 2P `PLAYER=1` lists.
- Type=`$dir` lights carry hotspot/Field (spot-like cone) but PORT treats them as directional with distance falloff only.
