#!/bin/bash
# Patches "Prince of Persia Classic.sh" on the card so the 32-bit GL detection prefers the firmware's own
# Mali library (libmali.so.1, the one its kernel matches) and never picks the old libmali-bifrost-g31-rxp0
# blob (user-space 10.6, refused by this kernel's 11.7 driver: glGetString then returns NULL and cocos2d
# aborts with "basic_string::_S_construct NULL not valid").
#   bash tools/fixes/popclassic-gl.patch.sh "/mnt/h/ports/Prince of Persia Classic.sh"
# then copy tools/fixes/popclassic-port.cfg to ports/popclassic/port.cfg (POP_GL_MODE / POP_GL_BLOB options).
set -eu
F=${1:?launcher path}
cp -n "$F" "$F.bak" 2>/dev/null || true
python3 - "$F" <<'EOF'
import sys, re
p = sys.argv[1]; s = open(p, encoding='utf-8', newline='').read()
# 1. /usr/local/lib/arm-linux-gnueabihf (where the stray rxp0 blob lives) goes last in the search list
old_dirs = 'GL_DIRS="/usr/local/lib/arm-linux-gnueabihf /usr/lib/arm-linux-gnueabihf /usr/lib/arm-linux-gnueabihf/mali /lib/arm-linux-gnueabihf /usr/lib32/mali /usr/lib32 /lib32 /usr/lib /lib"'
new_dirs = 'GL_DIRS="/usr/lib/arm-linux-gnueabihf /usr/lib/arm-linux-gnueabihf/mali /lib/arm-linux-gnueabihf /usr/lib32/mali /usr/lib32 /lib32 /usr/lib /lib /usr/local/lib/arm-linux-gnueabihf"'
assert old_dirs in s, 'GL_DIRS line not found'
s = s.replace(old_dirs, new_dirs)
# 2. the blob scan: the firmware's selected library first, the old rxp0 blob never
old_loop = '''    for gl_blob in "$gl_dir"/libmali-*.so "$gl_dir"/libmali.so* "$gl_dir"/libMali.so*; do
      [ -e "$gl_blob" ] || continue
'''
new_loop = '''    for gl_blob in ${POP_GL_BLOB:-} "$gl_dir"/libmali.so.1 "$gl_dir"/libmali.so "$gl_dir"/libmali-*.so "$gl_dir"/libmali.so* "$gl_dir"/libMali.so*; do
      [ -e "$gl_blob" ] || continue
      case "$gl_blob" in *rxp0*) continue ;; esac  # old blob (user 10.6) the kernel's Mali driver (11.7) refuses
'''
assert old_loop in s, 'blob loop not found'
s = s.replace(old_loop, new_loop)
# 3. port.cfg next to the game: POP_GL_BLOB=/path forces a library
old_cd = 'cd "$GAMEDIR" || exit 1\nexec >"$GAMEDIR/log.txt" 2>&1\n'
new_cd = 'cd "$GAMEDIR" || exit 1\nexec >"$GAMEDIR/log.txt" 2>&1\n[ -f "$GAMEDIR/port.cfg" ] && . "$GAMEDIR/port.cfg"\n'
assert old_cd in s, 'cd/exec lines not found'
s = s.replace(old_cd, new_cd)
# 4. firmware mode by default: the firmware's libEGL.so with GLES1 resolved by the game itself (the run that
#    worked on the R36S); the blob scan only on POP_GL_MODE=scan / POP_GL_BLOB, and the rxp0 blob skipped
#    under any name (/usr/local/lib also reaches it as libmali.so)
old_scan = '''if [ -z "${SDL_VIDEO_EGL_DRIVER:-}" ] && [ -z "${WAYLAND_DISPLAY:-}${DISPLAY:-}" ]; then
  for gl_dir in $GL_DIRS; do
    for gl_blob in ${POP_GL_BLOB:-} "$gl_dir"/libmali.so.1 "$gl_dir"/libmali.so "$gl_dir"/libmali-*.so "$gl_dir"/libmali.so* "$gl_dir"/libMali.so*; do
      [ -e "$gl_blob" ] || continue
      case "$gl_blob" in *rxp0*) continue ;; esac  # old blob (user 10.6) the kernel's Mali driver (11.7) refuses
'''
new_scan = '''# ArkOS on the R36S: the run that worked used the firmware's own 32-bit libEGL.so and the game resolved GLES1
# from libmali.so.1 by itself (no shim, no explicit blob). The blob scan below has nothing usable to find on
# this firmware and ends up at the stray libmali-bifrost-g31-rxp0-gbm blob under /usr/local/lib (also reachable
# as libmali.so there), whose user-space 10.6 the kernel's 11.7 Mali driver refuses: glGetString returns NULL
# and cocos2d aborts with "basic_string::_S_construct NULL not valid". POP_GL_MODE=scan in port.cfg restores
# the scan; POP_GL_BLOB=/path forces one library.
POP_GL_MODE="${POP_GL_MODE:-firmware}"
if [ -n "${POP_GL_BLOB:-}" ]; then POP_GL_MODE=scan; fi
if [ "$POP_GL_MODE" = firmware ] && [ -z "${SDL_VIDEO_EGL_DRIVER:-}" ] && [ -z "${WAYLAND_DISPLAY:-}${DISPLAY:-}" ]; then
  export SDL_VIDEO_EGL_DRIVER=libEGL.so
  echo "Graphics: firmware mode (POP_GL_MODE=firmware): EGL=libEGL.so, GLES1 resolved by the game"
fi

if [ -z "${SDL_VIDEO_EGL_DRIVER:-}" ] && [ -z "${WAYLAND_DISPLAY:-}${DISPLAY:-}" ]; then
  for gl_dir in $GL_DIRS; do
    for gl_blob in ${POP_GL_BLOB:-} "$gl_dir"/libmali.so.1 "$gl_dir"/libmali.so "$gl_dir"/libmali-*.so "$gl_dir"/libmali.so* "$gl_dir"/libMali.so*; do
      [ -e "$gl_blob" ] || continue
      case "$gl_blob:$(readlink -f "$gl_blob" 2>/dev/null)" in *rxp0*) continue ;; esac  # old blob (user 10.6) the kernel's Mali driver (11.7) refuses, by any name
'''
assert old_scan in s, 'blob loop (after step 2) not found'
s = s.replace(old_scan, new_scan)
open(p, 'w', encoding='utf-8', newline='').write(s)
print('patched', p)
EOF
grep -n 'rxp0\|POP_GL_BLOB\|^GL_DIRS' "$F"
