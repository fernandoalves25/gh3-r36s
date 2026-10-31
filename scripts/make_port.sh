#!/bin/bash
# Assembles the PortMaster package build/port/gh3-r36s.zip:
#   GH3.sh                   the launcher
#   gh3/gh3.aarch64          the player, with the game's venues in 3D (scripts/build_arm64.sh, -DGH3_3D)
#   gh3/port.json, README.md (Português), README.en.md (English), port.cfg
#   gh3/DATA/PUT_THE_GAME_DATA_HERE.txt
# No game material goes in: the player copies their own DATA folder to ports/gh3/DATA.
set -eu
cd "$(dirname "$0")/.."
BIN=build/gh3.aarch64
[ -f "$BIN" ] || { echo "no $BIN: run scripts/build_arm64.sh first"; exit 1; }
OUT=build/port; rm -rf "$OUT"; mkdir -p "$OUT/gh3/DATA"
cp port/GH3.sh "$OUT/"; chmod +x "$OUT/GH3.sh"
cp "$BIN" port/port.json port/README.md port/README.en.md port/port.cfg "$OUT/gh3/"
printf 'Copy the game'"'"'s DATA folder contents here: PAK/, SONGS/, MUSIC/, ZONES/ (the venues, the band and the crowd; MOVIES and STREAMS are optional).\n' > "$OUT/gh3/DATA/PUT_THE_GAME_DATA_HERE.txt"
python3 - "$OUT" <<'EOF'
import os, sys, zipfile
out = sys.argv[1]; z = zipfile.ZipFile(os.path.join(out, 'gh3-r36s.zip'), 'w', zipfile.ZIP_DEFLATED)
for root, _, files in os.walk(out):
    for f in files:
        if f.endswith('.zip'): continue
        p = os.path.join(root, f); arc = os.path.relpath(p, out).replace(os.sep, '/')
        zi = zipfile.ZipInfo(arc); zi.compress_type = zipfile.ZIP_DEFLATED
        zi.external_attr = (0o755 if arc.endswith(('.sh', '.aarch64')) else 0o644) << 16
        z.writestr(zi, open(p, 'rb').read())
z.close(); print('wrote', os.path.join(out, 'gh3-r36s.zip'))
EOF
ls -la "$OUT/gh3-r36s.zip"
