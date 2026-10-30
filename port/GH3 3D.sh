#!/bin/bash
# PortMaster launcher for Guitar Hero III (native) on the R36S and other RK3326 handhelds: the build with the 3D venues.
# Same folder, data and save files as GH3.sh; only the binary differs (and the log, log-3d.txt).
# Layout under ports/gh3/:
#   gh3-3d.aarch64     the player with the game's venues in 3D (this repo: scripts/build_arm64.sh, -DGH3_3D)
#   gh3.aarch64        the player with the 2D stage (GH3.sh)
#   DATA/              YOUR copy of the game's data folder (PAK/, SONGS/, MUSIC/ are what is read)
#   gh3.cfg            options and controls, written by the game's own menus (Opções / Controles)
#   gh3-recordes.cfg   records and career progress
#   gh3-carreira.cfg   career money and what the store sold
#   port.cfg           launcher options (vsync, clocks, software rendering)
#   log-3d.txt         written on every run of this launcher
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
> "$GAMEDIR/log-3d.txt" && exec > >(tee "$GAMEDIR/log-3d.txt") 2>&1
[ -f "$GAMEDIR/port.cfg" ] && source "$GAMEDIR/port.cfg"

if [ -z "$(find "$GAMEDIR/DATA" -maxdepth 2 -iname 'qb.pak.xen' 2>/dev/null | head -n 1)" ]; then
    pm_message "Arquivos do jogo não encontrados. Copie a pasta DATA do Guitar Hero III para ports/gh3/DATA (veja README.md)."
    sleep 6; exit 1
fi

export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
export GH3_DATA="$GAMEDIR/DATA"
export GH3_HOME="$GAMEDIR"
export GH3_SCREEN="${DISPLAY_WIDTH:-640}x${DISPLAY_HEIGHT:-480}"
export GH3_FULLSCREEN=1
export GH3_VSYNC="${VSYNC:-1}"
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

chmod +x "$GAMEDIR/gh3-3d.aarch64"
"$GAMEDIR/gh3-3d.aarch64"
echo "exit code $?"

for kv in "${GOVS[@]}"; do echo "${kv#*=}" | $ESUDO tee "${kv%%=*}" >/dev/null 2>&1; done
pm_finish
