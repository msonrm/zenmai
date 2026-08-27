#!/bin/sh
# .psexe → PS1 のブータブル CD イメージ(zenmai-zork.bin / .cue)。
#   前提: mkpsxiso(https://github.com/Lameguy64/mkpsxiso)を PATH か $MKPSXISO に
#   前提: build-zork.sh で zenmai-zork.psexe を焼いてあること
#
# ★ライセンスデータは同梱しない(iso.xml のコメント参照)。未改造の実機では起動しない。
#   PS2(FreeMcBoot)で遊ぶには、この .cue を cue2pops で VCD に変換する —— docs/ps1-ps2-pops.md
set -e
cd "$(dirname "$0")"
: "${MKPSXISO:=mkpsxiso}"
[ -f zenmai-zork.psexe ] || ./build-zork.sh
"$MKPSXISO" -y iso.xml
ls -l zenmai-zork.bin zenmai-zork.cue
