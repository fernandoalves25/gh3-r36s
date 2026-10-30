#!/bin/bash
# Patches "Prince of Persia Classic.sh" (after popclassic-gl.patch.sh) so the game takes GLES1 from the Mali
# library that holds the EGL context. pop.armhf (gl_provider_open) dlopen()s libmali.so.1, then libGL.so.1, and
# keeps the first one that has glMatrixMode. The run that reached the menu loaded libmali.so.1 (GL_VENDOR: ARM);
# every run that aborted right after Data_960_576/Logo/logo.png ("basic_string::_S_construct NULL not valid")
# logged "GL provider: libGL.so.1": libmali.so.1 did not load, and that 32-bit libGL.so.1 never sees the Mali
# context, so glGetString returns NULL on the first texture. The launcher now logs why libmali.so.1 does not load
# (pop.armhf --gl-probe), retries it with the firmware's copies of the port's bundled runtime, then tries the
# other 32-bit Mali libraries under the libmali.so.1 name with EGL from the same file. POP_GL_PROVIDER=system in
# port.cfg turns it off; POP_GL_PROVIDER=/path forces one library.
#   bash tools/fixes/popclassic-glprovider.patch.sh "/mnt/h/ports/Prince of Persia Classic.sh"
set -eu
F=${1:?launcher path}
cp -n "$F" "$F.bak-glprovider" 2>/dev/null || true
python3 - "$F" <<'EOF'
import sys
p = sys.argv[1]; s = open(p, encoding='utf-8', newline='').read()
if 'POP_GL_PROVIDER' in s:
    print('already patched', p); sys.exit(0)
# 1. firmware mode: make libmali.so.1 loadable, or put another Mali library under that name
old_fw = '''  export SDL_VIDEO_EGL_DRIVER=libEGL.so
  echo "Graphics: firmware mode (POP_GL_MODE=firmware): EGL=libEGL.so, GLES1 resolved by the game"
fi
'''
new_fw = old_fw + '''
# The game takes GLES1 from the first of libmali.so.1, libGL.so.1 that loads and has glMatrixMode. When
# libmali.so.1 does not load it falls through to a 32-bit libGL.so.1 that never sees the Mali EGL context:
# glGetString returns NULL on the first texture and cocos2d aborts ("_S_construct NULL not valid") right after
# the logo. So: say why libmali.so.1 fails, retry it with the firmware's runtime in front of the port's bundled
# copies, else put another 32-bit Mali library under the libmali.so.1 name with EGL from the same file.
# POP_GL_PROVIDER=system leaves the game alone; POP_GL_PROVIDER=/path forces a library.
POP_GL_PROVIDER="${POP_GL_PROVIDER:-mali}"
if [ "$DEBUG_LOGGING" = "1" ]; then
  echo "Graphics: 32-bit GL libraries on this system:"
  for gl_dir in $GL_DIRS; do
    for gl_lib in "$gl_dir"/libGL.so* "$gl_dir"/libEGL.so* "$gl_dir"/libGLESv1_CM.so* "$gl_dir"/libmali* "$gl_dir"/libMali*; do
      is_armhf_elf "$gl_lib" && echo "  $gl_lib -> $(readlink -f "$gl_lib")"
    done
  done
  { /sbin/ldconfig -p 2>/dev/null || ldconfig -p 2>/dev/null; } | grep -E 'lib(GL|EGL|GLESv1_CM|mali)\\.so' | sed 's/^/  cache: /'
fi
if [ "$POP_GL_MODE" = firmware ] && [ "${SDL_VIDEO_EGL_DRIVER:-}" = libEGL.so ] && [ "$POP_GL_PROVIDER" != system ]; then
  gl_provider=""
  case "$POP_GL_PROVIDER" in /*) gl_provider="$POP_GL_PROVIDER" ;; esac
  if [ -z "$gl_provider" ] && "$GAMEDIR/$BINARY" --gl-probe libmali.so.1; then
    echo "Graphics: libmali.so.1 loads; the game takes GLES1 from it"
  elif [ -z "$gl_provider" ]; then
    GL_SHIM=$(mktemp -d /tmp/popclassic-gl.XXXXXX) || exit 1
    gl_overrides=""
    for bundled_lib in "$GAMEDIR"/libs.armhf/*.so*; do
      lib_name="${bundled_lib##*/}"
      for gl_dir in /usr/lib/arm-linux-gnueabihf /lib/arm-linux-gnueabihf; do
        is_armhf_elf "$gl_dir/$lib_name" || continue
        ln -sf "$gl_dir/$lib_name" "$GL_SHIM/$lib_name" && gl_overrides="$gl_overrides $lib_name"
        break
      done
    done
    if [ -n "$gl_overrides" ] && LD_LIBRARY_PATH="$GL_SHIM:$LD_LIBRARY_PATH" "$GAMEDIR/$BINARY" --gl-probe libmali.so.1; then
      export LD_LIBRARY_PATH="$GL_SHIM:$LD_LIBRARY_PATH"
      echo "Graphics: libmali.so.1 loads with the firmware runtime:$gl_overrides"
    else
      rm -f "$GL_SHIM"/*
      for gl_dir in $GL_DIRS; do
        for gl_lib in "$gl_dir"/libmali.so* "$gl_dir"/libmali-*.so "$gl_dir"/libMali.so*; do
          is_armhf_elf "$gl_lib" || continue
          case "$gl_lib:$(readlink -f "$gl_lib" 2>/dev/null)" in *rxp0*) continue ;; esac  # user 10.6, refused by the kernel's 11.7 driver
          "$GAMEDIR/$BINARY" --gl-probe "$gl_lib" || continue
          gl_provider="$gl_lib"
          break 2
        done
      done
      if [ -z "$gl_provider" ]; then
        rmdir "$GL_SHIM" 2>/dev/null; GL_SHIM=""
        echo "Graphics: no 32-bit Mali library loads; the game will fall back to libGL.so.1"
      fi
    fi
  fi
  if [ -n "$gl_provider" ]; then
    [ -n "$GL_SHIM" ] || GL_SHIM=$(mktemp -d /tmp/popclassic-gl.XXXXXX) || exit 1
    ln -sf "$gl_provider" "$GL_SHIM/libmali.so.1"
    ln -sf "$gl_provider" "$GL_SHIM/libGL.so.1"
    export SDL_VIDEO_EGL_DRIVER="$gl_provider"
    export LD_LIBRARY_PATH="$GL_SHIM:$LD_LIBRARY_PATH"
    echo "Graphics: EGL and GLES1 from $gl_provider ($(readlink -f "$gl_provider"))"
  fi
fi
'''
assert old_fw in s, 'firmware-mode block not found (run popclassic-gl.patch.sh first)'
s = s.replace(old_fw, new_fw)
# 2. scan mode: the shim also answers for the names the game itself opens
old_sonames = '      for soname in libEGL.so libEGL.so.1 libGLESv1_CM.so libGLESv1_CM.so.1 libGLESv2.so libGLESv2.so.2; do\n'
new_sonames = '      for soname in libEGL.so libEGL.so.1 libGLESv1_CM.so libGLESv1_CM.so.1 libGLESv2.so libGLESv2.so.2 libmali.so.1 libGL.so.1; do\n'
assert old_sonames in s, 'scan shim soname list not found'
s = s.replace(old_sonames, new_sonames)
open(p, 'w', encoding='utf-8', newline='').write(s)
print('patched', p)
EOF
bash -n "$F" && grep -n 'POP_GL_PROVIDER=\|gl-probe libmali\|libmali.so.1 libGL.so.1' "$F"
