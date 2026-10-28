#!/bin/bash

set -e

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
[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
get_controls

GAMEDIR="/$directory/ports/halo-ce"
CONFDIR="$GAMEDIR/conf"
mkdir -p "$CONFDIR"
cd "$GAMEDIR"

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1
[ -f "$GAMEDIR/port.cfg" ] && source "$GAMEDIR/port.cfg"

export XDG_DATA_HOME="$CONFDIR"
export HALO_DATA_ROOT="$GAMEDIR"
export HALO_SAVE_ROOT="$CONFDIR/save"
export SDL_VIDEODRIVER="${SDL_VIDEODRIVER:-kmsdrm}"
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"
export SDL_RENDER_DRIVER="${SDL_RENDER_DRIVER:-opengles}"
export SDL_GAMECONTROLLERCONFIG="${sdl_controllerconfig:-}"

# Graphics: the firmware's own libEGL / libGLESv2 are used, which point at the Mali blob that
# matches its kernel. The original launcher forced a hard-coded blob (libmali-bifrost-g31-rxp0)
# in front of them; on this R36S the kernel's Mali driver is newer than that blob
# ("/dev/mali0 is not of a compatible version (user 10.6, kernel 11.7)"), so no window could be
# created and the game ran blind. MALI_SHIM=1 in halo-ce/port.cfg brings the shim back, for a
# firmware that has no usable libEGL.so.1 of its own; it then prefers the blob the firmware
# selected (libmali.so.1) over any particular file.
MALI_ROOT=""
have_egl=0
for d in /usr/lib/aarch64-linux-gnu /usr/lib /lib/aarch64-linux-gnu /lib; do
    [ -e "$d/libEGL.so.1" ] && have_egl=1 && break
done
if [ "${MALI_SHIM:-0}" = 1 ] || [ "$have_egl" = 0 ]; then
    MALI_LIB=""
    for candidate in \
        /usr/lib/aarch64-linux-gnu/libmali.so.1 \
        /usr/lib/aarch64-linux-gnu/libmali.so \
        /usr/lib/libmali.so.1 \
        /usr/lib/aarch64-linux-gnu/libmali-bifrost-g31*gbm*.so* \
        /usr/lib/aarch64-linux-gnu/libmali-bifrost-g52*.so* \
        /usr/lib/aarch64-linux-gnu/libmali*.so* \
        /usr/lib/aarch64-linux-gnu/libMali.so* \
        /usr/lib/libmali*.so* \
        /usr/lib/libMali.so; do
        if [ -f "$candidate" ]; then
            MALI_LIB="$candidate"
            break
        fi
    done
    if [ -n "$MALI_LIB" ]; then
        MALI_ROOT="/tmp/halo-ce-mali"
        mkdir -p "$MALI_ROOT"
        ln -sfn "$MALI_LIB" "$MALI_ROOT/libEGL.so.1"
        ln -sfn "$MALI_LIB" "$MALI_ROOT/libGLESv2.so.2"
        ln -sfn "$MALI_LIB" "$MALI_ROOT/libgbm.so.1"
        echo "halo-ce: Mali shim from $MALI_LIB"
    fi
else
    rm -rf /tmp/halo-ce-mali 2>/dev/null || true
    echo "halo-ce: using the firmware's EGL/GLES libraries"
fi

export LD_LIBRARY_PATH="$GAMEDIR/libs.${DEVICE_ARCH}${MALI_ROOT:+:$MALI_ROOT}:${GAMEDIR}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

if [ ! -f "$GAMEDIR/MAPS/ui.map" ] || [ ! -f "$GAMEDIR/MAPS/a10.map" ]; then
    pm_message "Halo CE data is missing. Copy the original MAPS folder into $GAMEDIR."
    sleep 5
    exit 1
fi

# full clocks while playing (PERF_GOV=0 in port.cfg leaves them alone)
GOVS=()
if [ "${PERF_GOV:-1}" != 0 ]; then
    for g in /sys/devices/system/cpu/cpufreq/policy*/scaling_governor /sys/class/devfreq/*/governor; do
        [ -f "$g" ] || continue
        old=$(cat "$g" 2>/dev/null) || continue
        echo performance | $ESUDO tee "$g" >/dev/null 2>&1 && GOVS+=("$g=$old")
    done
fi

$GPTOKEYB "halo-ce.${DEVICE_ARCH}" &
pm_platform_helper "$GAMEDIR/halo-ce.${DEVICE_ARCH}"
set +e
"$GAMEDIR/halo-ce.${DEVICE_ARCH}"
echo "halo-ce: exit code $?"

for kv in "${GOVS[@]}"; do echo "${kv#*=}" | $ESUDO tee "${kv%%=*}" >/dev/null 2>&1; done
pm_finish
