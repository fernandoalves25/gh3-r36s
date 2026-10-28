#!/bin/bash
# PortMaster launcher for Guitar Hero III (native) on the R36S and other RK3326 handhelds.
# Layout under ports/gh3/:
#   gh3.aarch64   the player (this repo: scripts/build_arm64.sh)
#   DATA/         YOUR copy of the game's data folder (PAK/, SONGS/, MUSIC/ are what is read)
#   port.cfg      options (tap mode, volume, lefty, clocks)
#   log.txt       written on every run
XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
if [ -d "/opt/system/Tools/PortMaster/" ]; then controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then controlfolder="$XDG_DATA_HOME/PortMaster"
else controlfolder="/roms/ports/PortMaster"; fi
source "$controlfolder/control.txt"
[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
get_controls

GAMEDIR="/$directory/ports/gh3"
cd "$GAMEDIR" || exit 1
> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1
[ -f "$GAMEDIR/port.cfg" ] && source "$GAMEDIR/port.cfg"

if [ -z "$(find "$GAMEDIR/DATA" -maxdepth 2 -iname 'qb.pak.xen' 2>/dev/null | head -n 1)" ]; then
    pm_message "Game files not found. Copy the game's DATA folder to ports/gh3/DATA (see README.md)."
    sleep 6; exit 1
fi

export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
export GH3_DATA="$GAMEDIR/DATA"
export GH3_SCREEN="${DISPLAY_WIDTH:-640}x${DISPLAY_HEIGHT:-480}"
export GH3_FULLSCREEN=1
export GH3_TAP="${TAP_MODE:-0}" GH3_VOLUME="${VOLUME:-100}" GH3_LEFTY="${LEFTY:-0}" GH3_VSYNC="${VSYNC:-1}"
[ "${SOFT_RENDER:-0}" = 1 ] && export GH3_RENDER=soft

# full clocks while playing: CPU, GPU and memory governors to "performance", the previous ones back on exit (PERF_GOV=0 leaves them alone)
GOVS=()
if [ "${PERF_GOV:-1}" != 0 ]; then
    for g in /sys/devices/system/cpu/cpufreq/policy*/scaling_governor /sys/class/devfreq/*/governor; do
        [ -f "$g" ] || continue
        old=$(cat "$g" 2>/dev/null) || continue
        echo performance | $ESUDO tee "$g" >/dev/null 2>&1 && GOVS+=("$g=$old")
    done
fi

chmod +x "$GAMEDIR/gh3.aarch64"
"$GAMEDIR/gh3.aarch64"
echo "exit code $?"

for kv in "${GOVS[@]}"; do echo "${kv#*=}" | $ESUDO tee "${kv%%=*}" >/dev/null 2>&1; done
pm_finish
