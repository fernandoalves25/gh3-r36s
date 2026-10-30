#!/bin/bash
# Turns the game's movies (already converted to MP4 in DATA/MOVIES/WEB) into what the R36S player decodes:
# MPEG-1 video + MP2 audio in an MPEG program stream (pl_mpeg), 640 wide, at most 30 fps, in DATA/MOVIES/R36S.
#   tools/convert_movies.sh [DATA dir] [name ...]      (default: DATA, every movie the player uses)
# Needs ffmpeg/ffprobe. A file already converted is skipped unless FORCE=1.
set -u
DATA=${1:-DATA}; shift || true
SRC="$DATA/MOVIES/WEB"; OUT="$DATA/MOVIES/R36S"
[ -d "$SRC" ] || { echo "no $SRC"; exit 1; }
mkdir -p "$OUT"
# boot logos and the intro, the career's cutscenes, the battle and multiplayer overlays, the bonus videos, the co-op career
NAMES="atvi ro_logo ns_logo intro singleplayer_01 singleplayer_02 singleplayer_03 singleplayer_04 singleplayer_05 singleplayer_06
 singleplayer_07 singleplayer_08 singleplayer_end Satan-Battle_WIN Satan-Battle_LOSS Golden_Guitar Fret_Flames Player1_wins Player2_wins
 brettmichaels sexpistols slash tommorello coop_01 coop_02 coop_03 coop_04 coop_05 coop_06"
[ $# -gt 0 ] && NAMES="$*"
# the MPEG-1 frame rate closest to the source's, never above 30 (a 59.94 source drops every other frame)
pick_rate() { awk -v r="$1" 'BEGIN { split(r, p, "/"); f = p[2] ? p[1] / p[2] : p[1]; if (f > 30.5) f /= 2;
  n = split("24000/1001 24 25 30000/1001 30", c, " "); best = c[n]; bd = 1e9;
  for (i = 1; i <= n; i++) { split(c[i], q, "/"); v = q[2] ? q[1] / q[2] : q[1]; d = f - v; if (d < 0) d = -d; if (d < bd) { bd = d; best = c[i] } } print best }'; }
for n in $NAMES; do
  in="$SRC/$n.mp4"; out="$OUT/$n.mpg"
  [ -f "$in" ] || { echo "skip $n (no $in)"; continue; }
  [ -f "$out" ] && [ "${FORCE:-0}" != 1 ] && { echo "have $n"; continue; }
  rate=$(pick_rate "$(ffprobe -v error -select_streams v:0 -show_entries stream=r_frame_rate -of default=nw=1:nk=1 "$in")")
  if ffprobe -v error -select_streams a -show_entries stream=index -of csv=p=0 "$in" | grep -q .; then aud=(-c:a mp2 -b:a 160k -ar 44100 -ac 2); else aud=(-an); fi
  # 640x360 at ~2 Mbit/s: clean on the 640x480 screen and cheap to decode; B-frames off (each costs two predictions),
  # a keyframe every second
  ffmpeg -nostdin -v error -y -i "$in" -map 0:v:0 -map '0:a:0?' -vf "scale=640:-2:flags=lanczos,fps=$rate,format=yuv420p" -r "$rate" \
    -c:v mpeg1video -b:v 2000k -maxrate 3200k -bufsize 1835k -qmin 2 -qmax 16 -bf 0 -g 30 -mbd rd -trellis 1 "${aud[@]}" -f mpeg "$out.tmp" \
    && mv "$out.tmp" "$out" && echo "$n: $rate fps, $(du -k "$out" | cut -f1) KB" || { rm -f "$out.tmp"; echo "FAILED $n"; }
done
du -ch "$OUT"/*.mpg 2>/dev/null | tail -1
