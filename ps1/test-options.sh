#!/bin/sh
# オプション画面(打っていないときの Start)の検証。
#
# ★オプションは**専用の画面で完結**し、閉じたら本文へ戻る。背景色も本文と変えてある。
#   だから確かめるのは「画面の状態が動いたか」と、★**本文が汚れていないこと**。
#   (一度はライセンスを本文の履歴へ流す形で作ったが、実機の指摘で作り直した。
#    その名残を検査に残しておく = 本文の履歴が伸びないことをカナリアにする)
set -e
cd "$(dirname "$0")"
PY="${PY:-python3}"
ng=0

sym() { mipsel-linux-gnu-nm out-zork.elf | grep -i " $1\$" | cut -d' ' -f1; }

# lic_top / opt_mode / opt_sel / opt_open を 1 行に
state() {
    "$PY" sim.py zenmai-zork.psexe --script "$1" --polls --stop "$2" --max 400000000 \
        --dump "$STATE,10" 2>/dev/null | sed -n 's/^DUMP: //p' | "$PY" -c "
import sys, ast
b = ast.literal_eval(sys.stdin.read().strip())
v = [int.from_bytes(b[i:i+4], 'little') for i in range(0, 16, 4)]
print('top=%d mode=%d sel=%d open=%d' % tuple(v))
"
}

check() {
    if [ "$2" = "$3" ]; then echo "✓ $1"
    else echo "✗ $1"; echo "    実際: $2"; echo "    期待: $3"; ng=$((ng + 1)); fi
}

[ -f zenmai-zork.psexe ] || ./build-zork.sh >/dev/null 2>&1
STATE=$(sym lic_top)                    # lic_top / opt_mode / opt_sel / opt_open が並ぶ
TOTAL=$(sym total_h)

check "Start でメニューが開く"        "$(state opt_open.script 700)"    "top=0 mode=0 sel=0 open=1"
check "○ でライセンス画面へ"          "$(state opt_license.script 800)" "top=0 mode=1 sel=0 open=1"
check "↓×3 で画面の中を送る"          "$(state opt_scroll.script 900)"  "top=3 mode=1 sel=0 open=1"
check "× で一段戻る(オプションは開いたまま)" "$(state opt_back.script 800)" "top=0 mode=0 sel=0 open=1"
check "Start でトグル(閉じる)"        "$(state opt_toggle.script 800)"  "top=0 mode=0 sel=0 open=0"
check "× で閉じる"                    "$(state opt_close.script 800)"   "top=0 mode=0 sel=0 open=0"

# ★カナリア: ライセンスを開いても**本文の履歴は 1 行も伸びない**
BASE=$("$PY" sim.py zenmai-zork.psexe --script opt_open.script --polls --stop 800 \
       --max 400000000 --dump "$TOTAL,4" 2>/dev/null | sed -n 's/^DUMP: //p')
SHOWN=$("$PY" sim.py zenmai-zork.psexe --script opt_license.script --polls --stop 800 \
       --max 400000000 --dump "$TOTAL,4" 2>/dev/null | sed -n 's/^DUMP: //p')
check "★カナリア: 本文が汚れない(履歴が伸びない)" "$SHOWN" "$BASE"

echo
if [ "$ng" = 0 ]; then echo "--- 7 件すべて通った ---"; else echo "--- ★$ng 件 食い違った ---"; exit 1; fi
