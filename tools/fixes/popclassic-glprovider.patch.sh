#!/bin/bash
# Patches "Prince of Persia Classic.sh" (after popclassic-gl.patch.sh) so the game's GLES1 calls go to the same
# Mali library that holds the EGL context. Every run that aborted with "basic_string::_S_construct NULL not
# valid" right after Data_960_576/Logo/logo.png logged "GL provider: libGL.so.1"; the one that reached the menu
# logged "GL provider: libmali.so.1" (GL_VENDOR: ARM). The game prefers a 32-bit libGL.so.1 when the system has
# one, and that library has no current context: glGetString returns NULL on the first texture load and cocos2d
# builds a std::string from it. The launcher now links libGL.so.1 to the 32-bit libmali.so.1 in a shim
# directory, and logs which 32-bit GL libraries the device has. POP_GL_PROVIDER=system in port.cfg turns it off;
# POP_GL_PROVIDER=/path forces one library.
#   bash tools/fixes/popclassic-glprovider.patch.sh "/mnt/h/ports/Prince of Persia Classic.sh"
set -eu
F=${1:?launcher path}
cp -n "$F" "$F.bak-glprovider" 2>/dev/null || true
python3 - "$F" <<'EOF'
import sys
p = sys.argv[1]; s = open(p, encoding='utf-8', newline='').read()
if 'POP_GL_PROVIDER' in s:
    print('already patched', p); sys.exit(0)
# 1. firmware mode: libGL.so.1 -> the 32-bit libmali.so.1 the loader would pick (the one the working run used)
old_fw = '''  export SDL_VIDEO_EGL_DRIVER=libEGL.so
  echo "Graphics: firmware mode (POP_GL_MODE=firmware): EGL=libEGL.so, GLES1 resolved by the game"
fi
'''
new_fw = old_fw + '''
# The game takes GLES1 from libGL.so.1 when a 32-bit one exists (GL4ES/Mesa), and that library never sees the
# Mali EGL context: glGetString returns NULL and cocos2d aborts on the first texture ("_S_construct NULL not
# valid"). The run that worked took GLES1 from libmali.so.1, so libGL.so.1 points there in a shim directory.
# POP_GL_PROVIDER=system keeps the system libGL.so.1; POP_GL_PROVIDER=/path forces a library.
if [ "$DEBUG_LOGGING" = "1" ]; then
  echo "Graphics: 32-bit GL libraries on this system:"
  for gl_dir in $GL_DIRS; do
    for gl_lib in "$gl_dir"/libGL.so* "$gl_dir"/libEGL.so* "$gl_dir"/libGLESv1_CM.so* "$gl_dir"/libmali*; do
      is_armhf_elf "$gl_lib" && echo "  $gl_lib -> $(readlink -f "$gl_lib")"
    done
  done
  { /sbin/ldconfig -p 2>/dev/null || ldconfig -p 2>/dev/null; } | grep -E 'lib(GL|EGL|GLESv1_CM|mali)\\.so' | sed 's/^/  cache: /'
fi
POP_GL_PROVIDER="${POP_GL_PROVIDER:-mali}"
if [ "$POP_GL_MODE" = firmware ] && [ "${SDL_VIDEO_EGL_DRIVER:-}" = libEGL.so ] && [ "$POP_GL_PROVIDER" != system ]; then
  gl_provider=""
  case "$POP_GL_PROVIDER" in
    /*) gl_provider="$POP_GL_PROVIDER" ;;
    *)
      for gl_lib in $({ /sbin/ldconfig -p 2>/dev/null || ldconfig -p 2>/dev/null; } | sed -n 's/^[[:space:]]*libmali\\.so\\.1 (.*) => //p') \\
                    /lib/arm-linux-gnueabihf/libmali.so.1 /usr/lib/arm-linux-gnueabihf/libmali.so.1; do
        is_armhf_elf "$gl_lib" && { gl_provider="$gl_lib"; break; }
      done
      if [ -z "$gl_provider" ]; then
        for gl_dir in $GL_DIRS; do
          is_armhf_elf "$gl_dir/libmali.so.1" && { gl_provider="$gl_dir/libmali.so.1"; break; }
        done
      fi
      ;;
  esac
  if is_armhf_elf "$gl_provider"; then
    GL_SHIM=$(mktemp -d /tmp/popclassic-gl.XXXXXX) || exit 1
    ln -s "$gl_provider" "$GL_SHIM/libGL.so.1"
    export LD_LIBRARY_PATH="$GL_SHIM:$LD_LIBRARY_PATH"
    echo "Graphics: GLES1 provider libGL.so.1 -> $gl_provider ($(readlink -f "$gl_provider"))"
  else
    echo "Graphics: no 32-bit libmali.so.1 found; the game picks its own GLES1 library"
  fi
fi
'''
assert old_fw in s, 'firmware-mode block not found (run popclassic-gl.patch.sh first)'
s = s.replace(old_fw, new_fw)
# 2. scan mode: the shim also answers for libGL.so.1, so the game cannot pick a different library there either
old_sonames = '      for soname in libEGL.so libEGL.so.1 libGLESv1_CM.so libGLESv1_CM.so.1 libGLESv2.so libGLESv2.so.2; do\n'
new_sonames = '      for soname in libEGL.so libEGL.so.1 libGLESv1_CM.so libGLESv1_CM.so.1 libGLESv2.so libGLESv2.so.2 libGL.so.1; do\n'
assert old_sonames in s, 'scan shim soname list not found'
s = s.replace(old_sonames, new_sonames)
open(p, 'w', encoding='utf-8', newline='').write(s)
print('patched', p)
EOF
bash -n "$F" && grep -n 'POP_GL_PROVIDER\|libGL.so.1' "$F"
