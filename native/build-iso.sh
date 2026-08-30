#!/bin/sh
# .psexe → PS1 のブータブル CD イメージ(zenmai-zork.bin / .cue)。
#   前提: mkpsxiso(https://github.com/Lameguy64/mkpsxiso)を PATH か $MKPSXISO に
#   前提: build.sh で zenmai-zork.psexe を焼いてあること
#
# ★ライセンスデータは同梱しない(iso.xml のコメント参照)。未改造の実機では起動しない。
#   PS2(FreeMcBoot)で遊ぶには、この .cue を cue2pops で VCD に変換する —— docs/ps1-ps2-pops.md
set -e
cd "$(dirname "$0")"
: "${MKPSXISO:=mkpsxiso}"
[ -f zenmai-zork.psexe ] || ./build.sh

# ★$LICENSE に PS1 のライセンスデータを渡すと焼き込む(未改造の実機・POPS 向け)。
#   Sony の著作物なので配布物には入れない。自分の PS1 ソフトから吸うこと:
#     dumpsxiso -x out/ -s out/game.xml /path/to/your-ps1-game.cue
#   → out/ に licence.dat(または license.dat)が出る。それを渡す:
#     LICENSE=out/licence.dat ./build-iso.sh
: "${LICENSE:=}"
if [ -n "$LICENSE" ]; then
    echo "ライセンスデータ: $LICENSE"
    "$MKPSXISO" -y -L "$LICENSE" iso.xml
else
    echo "ライセンスデータ: 無し(未改造の実機では起動しない。POPS でも要るかもしれない)"
    "$MKPSXISO" -y iso.xml
fi
ls -l zenmai-zork.bin zenmai-zork.cue
