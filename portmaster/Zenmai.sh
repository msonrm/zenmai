#!/bin/bash
# Zenmai — 日本語で読み、日本語で打つ Zork。ゲームパッドだけで遊べる。
# PortMaster の起動スクリプト（雛形は portmaster.games/packaging.html）。

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

source $controlfolder/control.txt
[ -f "$controlfolder/mod_${CFW_NAME}.txt" ] && source "$controlfolder/mod_${CFW_NAME}.txt"

get_controls

GAMEDIR="/$directory/ports/zenmai"
CONFDIR="$GAMEDIR/conf"
mkdir -p "$CONFDIR"

cd "$GAMEDIR"
> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

export XDG_DATA_HOME="$CONFDIR"
export LD_LIBRARY_PATH="$GAMEDIR/libs.${DEVICE_ARCH}:$LD_LIBRARY_PATH"
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

# ★セーブはポートのフォルダに置く —— SD カードごと持ち運べる形にするため。
#   置き場所を決めるのは起動スクリプトの仕事で、ゲーム側は ZENMAI_SAVE を見るだけ。
export ZENMAI_SAVE="$GAMEDIR/zenmai.sav"

$ESUDO chmod +x "$GAMEDIR/zenmai-zork.${DEVICE_ARCH}"

# ★gptokeyb は使わない —— このポートは SDL のゲームコントローラを直接読む。
#   キー入力へ化かす層を挟むと、**同時押しで 1 字を決める**入力
#   （左手＝子音行 + 右手＝母音）が壊れる。ここは移植の芯なので譲らない。
pm_platform_helper "$GAMEDIR/zenmai-zork.${DEVICE_ARCH}"
"$GAMEDIR/zenmai-zork.${DEVICE_ARCH}"

pm_finish
