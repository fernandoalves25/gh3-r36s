#!/bin/bash
# PORTMASTER: popclassic.zip, Prince of Persia Classic.sh

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

source "$controlfolder/control.txt"
export PORT_32BIT="Y"
[ -f "$controlfolder/mod_${CFW_NAME}.txt" ] && source "$controlfolder/mod_${CFW_NAME}.txt"
get_controls

GAMEDIR="/$directory/ports/popclassic"
BINARY="pop.armhf"
# Set to 0 to disable detailed traces; enabled by default for now.
DEBUG_LOGGING=1
cd "$GAMEDIR" || exit 1
exec >"$GAMEDIR/log.txt" 2>&1
[ -f "$GAMEDIR/port.cfg" ] && . "$GAMEDIR/port.cfg"

if [ "$DEBUG_LOGGING" = "1" ]; then
  export LOADER_TRACE=1
  export POPCLASSIC_DEBUG=1
  EAPX_OPTIONS=()
  echo "Debug logging is enabled."
else
  unset LOADER_TRACE
  unset POPCLASSIC_DEBUG
  EAPX_OPTIONS=(--quiet)
  echo "Debug logging is disabled."
fi

if [ ! -f donor/lib/armeabi/libgame_logic.so ]; then
  APK=""
  for candidate in "$GAMEDIR"/gamedata/*.[aA][pP][kK]; do
    [ -f "$candidate" ] || continue
    APK="$candidate"
    break
  done

  OBB="$GAMEDIR/gamedata/main.1.org.ubisoft.premium.POPClassic.obb"
  if [ -z "$APK" ] || [ ! -f "$OBB" ]; then
    pm_message "The matching Prince of Persia Classic 1.0 APK and OBB were not found in ports/popclassic/gamedata. The APK may keep its original filename; see README.md."
    exit 1
  fi
  command -v python3 >/dev/null 2>&1 || {
    pm_message "Prince of Persia Classic requires Python 3 for first-launch data import."
    pm_finish
    exit 1
  }
  pm_message "Importing Prince of Persia Classic data. Keep the device powered on."
  python3 "$GAMEDIR/eapx.py" install --recipe "$GAMEDIR/popclassic.eapx.json" \
    --game-dir "$GAMEDIR" --input "$APK" --input "$OBB" --no-adopt \
    "${EAPX_OPTIONS[@]}" || {
      pm_message "Prince of Persia Classic data import failed. See popclassic/log.txt."
      pm_finish
      exit 1
    }
fi

export POPCLASSIC_INPUT=joystick
echo "Input: native gamepad mode (on-screen touch controls hidden by the game)."
export LD_LIBRARY_PATH="$GAMEDIR/libs.armhf${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
for audio_libdir in /usr/local/lib/arm-linux-gnueabihf /usr/lib/arm-linux-gnueabihf /usr/lib32; do
  [ -d "$audio_libdir/pipewire-0.3" ] && export PIPEWIRE_MODULE_DIR="$audio_libdir/pipewire-0.3"
  [ -d "$audio_libdir/spa-0.2" ] && export SPA_PLUGIN_DIR="$audio_libdir/spa-0.2"
  [ -d "$audio_libdir/alsa-lib" ] && export ALSA_PLUGIN_DIR="$audio_libdir/alsa-lib"
done
for audio_runtime in "${XDG_RUNTIME_DIR:-}" "/run/user/$(id -u)" /run/user/0; do
  [ -n "$audio_runtime" ] && [ -d "$audio_runtime" ] || continue
  export XDG_RUNTIME_DIR="$audio_runtime"
  break
done
if [ -S "${XDG_RUNTIME_DIR:-}/pulse/native" ]; then
  export PULSE_SERVER="unix:$XDG_RUNTIME_DIR/pulse/native"
fi
if [ -n "${PIPEWIRE_MODULE_DIR:-}" ] || [ -n "${PULSE_SERVER:-}" ]; then
  unset AUDIODEV ALSA_CONFIG_PATH ALSA_CARD
  export SDL_AUDIO_ALSA_SET_BUFFER_SIZE=1
else
  export AUDIODEV="${AUDIODEV:-plug:dmix}"
fi
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"
echo "Audio: SDL=$SDL_AUDIODRIVER PCM=${AUDIODEV:-default} PipeWire=${PIPEWIRE_MODULE_DIR:-unset} SPA=${SPA_PLUGIN_DIR:-unset} ALSA=${ALSA_PLUGIN_DIR:-unset}"
[ ! -r /proc/asound/cards ] || cat /proc/asound/cards

$ESUDO chmod +x "$GAMEDIR/$BINARY" 2>/dev/null

GL_SHIM=""
GL_DIRS="/usr/lib/arm-linux-gnueabihf /usr/lib/arm-linux-gnueabihf/mali /lib/arm-linux-gnueabihf /usr/lib32/mali /usr/lib32 /lib32 /usr/lib /lib /usr/local/lib/arm-linux-gnueabihf"

is_armhf_elf() {
  [ -f "$1" ] && [ "$(od -An -tu1 -j4 -N1 "$1" 2>/dev/null | tr -d ' ')" = 1 ]
}

WAYLAND_SOCKET=""
for wayland_runtime in "${XDG_RUNTIME_DIR:-}" "/run/user/$(id -u)" /run/user/0; do
  [ -n "$wayland_runtime" ] && [ -d "$wayland_runtime" ] || continue
  if [ -n "${WAYLAND_DISPLAY:-}" ]; then
    case "$WAYLAND_DISPLAY" in
      /*) [ -S "$WAYLAND_DISPLAY" ] && WAYLAND_SOCKET="$WAYLAND_DISPLAY" ;;
      *) [ -S "$wayland_runtime/$WAYLAND_DISPLAY" ] && WAYLAND_SOCKET="$wayland_runtime/$WAYLAND_DISPLAY" ;;
    esac
  fi
  [ -n "$WAYLAND_SOCKET" ] && { export XDG_RUNTIME_DIR="$wayland_runtime"; break; }
  for wayland_candidate in "$wayland_runtime"/wayland-*; do
    [ -S "$wayland_candidate" ] || continue
    WAYLAND_SOCKET="$wayland_candidate"
    WAYLAND_DISPLAY="${wayland_candidate##*/}"
    export XDG_RUNTIME_DIR WAYLAND_DISPLAY
    break 2
  done
done

if [ -n "$WAYLAND_SOCKET" ] && [ -z "${SDL_VIDEO_EGL_DRIVER:-}" ] && [ -z "${SDL_VIDEO_GL_DRIVER:-}" ]; then
  export SDL_VIDEODRIVER=wayland
  GL_MESA_DIR=""
  for gl_dir in $GL_DIRS; do
    case "$gl_dir" in */mali) continue ;; esac
    is_armhf_elf "$gl_dir/libEGL.so.1" || continue
    is_armhf_elf "$gl_dir/libGLESv1_CM.so.1" || continue
    [ -f "$gl_dir/libEGL_mesa.so.0" ] || continue
    GL_MESA_DIR="$gl_dir"
    break
  done

  if [ -z "$GL_MESA_DIR" ]; then
    echo "Graphics: Wayland session at $WAYLAND_SOCKET, but no 32-bit Mesa EGL/GLES1 stack was found."
    pm_message "Prince of Persia Classic could not find the 32-bit Mesa graphics libraries needed by this Rocknix Wayland session. Update Rocknix and PortMaster, then retry."
    pm_finish
    exit 1
  fi

  GL_SHIM=$(mktemp -d /tmp/popclassic-gl.XXXXXX) || exit 1
  ln -sf "$GL_MESA_DIR/libEGL.so.1" "$GL_SHIM/libEGL.so" || exit 1
  ln -sf "$GL_MESA_DIR/libEGL.so.1" "$GL_SHIM/libEGL.so.1" || exit 1
  ln -sf "$GL_MESA_DIR/libGLESv1_CM.so.1" "$GL_SHIM/libGLESv1_CM.so" || exit 1
  ln -sf "$GL_MESA_DIR/libGLESv1_CM.so.1" "$GL_SHIM/libGLESv1_CM.so.1" || exit 1
  for gl_soname in libGLESv2.so libGLESv2.so.2; do
    [ -e "$GL_MESA_DIR/$gl_soname" ] && ln -sf "$GL_MESA_DIR/$gl_soname" "$GL_SHIM/$gl_soname"
  done

  # ROCKNIX's Panfrost driver can need newer C++ runtime symbols than the
  # backward-compatible copy bundled with the port. Keep the real firmware
  # libmali name intact for ROCKNIX's own GBM hook; do not alias it to Mesa.
  for gl_dir in $GL_DIRS; do
    if is_armhf_elf "$gl_dir/libmali.so.1"; then
      ln -sf "$gl_dir/libmali.so.1" "$GL_SHIM/libmali.so.1"
      break
    fi
  done
  export SDL_VIDEO_EGL_DRIVER="$GL_MESA_DIR/libEGL.so.1"
  export SDL_VIDEO_GL_DRIVER="$GL_MESA_DIR/libGLESv1_CM.so.1"
  export LD_LIBRARY_PATH="$GL_SHIM:$LD_LIBRARY_PATH"
  echo "Graphics: Wayland Mesa/Panfrost EGL=$SDL_VIDEO_EGL_DRIVER GLES1=$SDL_VIDEO_GL_DRIVER"
  echo "Graphics: LIBGL_DRIVERS_PATH=${LIBGL_DRIVERS_PATH:-firmware default}"

  if ! "$GAMEDIR/$BINARY" --gl-probe "$GL_MESA_DIR/libEGL_mesa.so.0"; then
    echo "Graphics: Mesa driver did not load with the port's bundled runtime; trying matching Rocknix libraries."
    GL_MESA_UNSHADOWED=""
    for bundled_lib in "$GAMEDIR"/libs.armhf/*.so*; do
      [ -f "$bundled_lib" ] || continue
      lib_name="${bundled_lib##*/}"
      [ -f "$GL_MESA_DIR/$lib_name" ] || continue
      if ln -sf "$GL_MESA_DIR/$lib_name" "$GL_SHIM/$lib_name"; then
        GL_MESA_UNSHADOWED="$GL_MESA_UNSHADOWED $lib_name"
      fi
    done
    echo "Graphics: using firmware runtime overrides:${GL_MESA_UNSHADOWED:- none}"
    if ! "$GAMEDIR/$BINARY" --gl-probe "$GL_MESA_DIR/libEGL_mesa.so.0"; then
      echo "Graphics: the Mesa driver still cannot load with the firmware runtime."
      rm -f "$GL_SHIM"/*; rmdir "$GL_SHIM" 2>/dev/null || true; GL_SHIM=""
      pm_message "Prince of Persia Classic found Rocknix Mesa, but its 32-bit driver dependencies could not be loaded. See ports/popclassic/log.txt."
      pm_finish
      exit 1
    fi
  fi
  if ! "$GAMEDIR/$BINARY" --gl-probe-init; then
    echo "Graphics: Mesa loaded, but SDL could not create a GLES1 or compatibility context."
    rm -f "$GL_SHIM"/*; rmdir "$GL_SHIM" 2>/dev/null || true; GL_SHIM=""
    pm_message "Prince of Persia Classic could not start a graphics context under Rocknix Wayland. See ports/popclassic/log.txt."
    pm_finish
    exit 1
  fi
fi

if [ -z "${SDL_VIDEO_EGL_DRIVER:-}" ] && [ -z "${WAYLAND_DISPLAY:-}${DISPLAY:-}" ]; then
  for gl_dir in $GL_DIRS; do
    for gl_blob in ${POP_GL_BLOB:-} "$gl_dir"/libmali.so.1 "$gl_dir"/libmali.so "$gl_dir"/libmali-*.so "$gl_dir"/libmali.so* "$gl_dir"/libMali.so*; do
      [ -e "$gl_blob" ] || continue
      case "$gl_blob" in *rxp0*) continue ;; esac  # old blob (user 10.6) the kernel's Mali driver (11.7) refuses
      [ "$(od -An -tu1 -j4 -N1 "$gl_blob" 2>/dev/null | tr -d ' ')" = 1 ] || continue
      GL_SHIM=$(mktemp -d /tmp/popclassic-gl.XXXXXX) || break 2
      for soname in libEGL.so libEGL.so.1 libGLESv1_CM.so libGLESv1_CM.so.1 libGLESv2.so libGLESv2.so.2; do
        ln -s "$gl_blob" "$GL_SHIM/$soname"
      done
      export SDL_VIDEO_EGL_DRIVER="$gl_blob" SDL_VIDEO_GL_DRIVER="$gl_blob"
      export LD_LIBRARY_PATH="$GL_SHIM:$gl_dir:$LD_LIBRARY_PATH"
      break 2
    done
  done
fi
if [ -z "${SDL_VIDEO_EGL_DRIVER:-}" ]; then
  for gl_dir in $GL_DIRS; do
    for egl in "$gl_dir/libEGL.so.1" "$gl_dir/libEGL.so"; do
      [ -e "$egl" ] || continue
      for gles in "$gl_dir/libGLESv1_CM.so.1" "$gl_dir/libGLESv1_CM.so"; do
        [ -e "$gles" ] || continue
        [ "$(od -An -tu1 -j4 -N1 "$egl" 2>/dev/null | tr -d ' ')" = 1 ] || continue
        export SDL_VIDEO_EGL_DRIVER="$egl" SDL_VIDEO_GL_DRIVER="$gles"
        export LD_LIBRARY_PATH="$gl_dir:$LD_LIBRARY_PATH"
        break 3
      done
    done
  done
fi
echo "Graphics: video=${SDL_VIDEODRIVER:-auto} EGL=${SDL_VIDEO_EGL_DRIVER:-auto} GLES1=${SDL_VIDEO_GL_DRIVER:-auto}"

export POPCLASSIC_SAVEDIR="$GAMEDIR/saves"
export POPCLASSIC_RESOLUTION=auto
if [ -f "$GAMEDIR/resolution.txt" ]; then
  export POPCLASSIC_RESOLUTION="$(tr -d '\r\n' < "$GAMEDIR/resolution.txt")"
fi
mkdir -p "$POPCLASSIC_SAVEDIR"

$GPTOKEYB2 "$BINARY" -c "$GAMEDIR/popclassic.ini" &
pm_platform_helper "$GAMEDIR/$BINARY"
"$GAMEDIR/$BINARY" "$GAMEDIR/donor"
[ -z "$GL_SHIM" ] || { rm -f "$GL_SHIM"/*; rmdir "$GL_SHIM"; }
pm_finish
