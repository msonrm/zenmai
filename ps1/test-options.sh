#!/bin/sh
# オプション(打っていないときの Start)の検証。
#
# ★かたちは「**本文に重ねる小さなメニュー** → 行き先は**全画面の読み物**」。
#   メニューは道しるべ、頁が目的地。だから確かめるのは
#     ・板とメニューの状態が動くか
#     ・頁から Start で**一足で**本文へ戻るか(× は一段だけ)
#     ・決定/取消が**言語で入れ替わる**か(にほんご ○ / ENGLISH ×)
#     ・★**本文が汚れていないこと**(一度はライセンスを本文へ流す形で作り、
#       実機の指摘で作り直した。その名残を履歴の伸びで見張る)
set -e
cd "$(dirname "$0")"
PY="${PY:-python3}"
ng=0

sym() { mipsel-linux-gnu-nm out-zork.elf | grep -i " $1\$" | cut -d' ' -f1; }

# page_top / opt_page / opt_mode / opt_sel / opt_open が並ぶ(nm で確認)
state() {
    "$PY" sim.py zenmai-zork.psexe --script "$1" --polls --stop "$2" --max 400000000 \
        --dump "$STATE,20" 2>/dev/null | sed -n 's/^DUMP: //p' | "$PY" -c "
import sys, ast
b = ast.literal_eval(sys.stdin.read().strip())
v = [int.from_bytes(b[i:i+4], 'little') for i in range(0, 20, 4)]
print('top=%d page=%d mode=%d sel=%d open=%d' % tuple(v))
"
}

check() {
    if [ "$2" = "$3" ]; then echo "✓ $1"
    else echo "✗ $1"; echo "    実際: $2"; echo "    期待: $3"; ng=$((ng + 1)); fi
}

[ -f zenmai-zork.psexe ] || ./build-zork.sh >/dev/null 2>&1
STATE=$(sym page_top)
TOTAL=$(sym total_h)

check "Start でメニューが開く"          "$(state opt_open.script 700)"     "top=0 page=0 mode=0 sel=0 open=1"
check "↓ で選択が動く"                  "$(state opt_move.script 800)"     "top=0 page=0 mode=0 sel=1 open=1"
check "○ で 1 つめの頁へ"               "$(state opt_page0.script 800)"    "top=0 page=0 mode=1 sel=0 open=1"
check "↓↓ ○ でライセンスの頁へ"         "$(state opt_page2.script 900)"    "top=0 page=2 mode=1 sel=2 open=1"
check "頁の中を ↓×3 で送る"             "$(state opt_scroll.script 1000)"  "top=3 page=2 mode=1 sel=2 open=1"
check "× で一段もどる(メニューは開いたまま)" "$(state opt_back.script 900)" "top=0 page=0 mode=0 sel=0 open=1"
check "★頁の Start は一足で本文へ"      "$(state opt_startout.script 900)" "top=0 page=0 mode=0 sel=0 open=0"
check "メニューの Start でとじる"       "$(state opt_toggle.script 800)"   "top=0 page=0 mode=0 sel=0 open=0"
check "メニューの × でとじる"           "$(state opt_close.script 800)"    "top=0 page=0 mode=0 sel=0 open=0"
check "★ENGLISH では × がきめる"        "$(state opt_en.script 800)"       "top=0 page=0 mode=1 sel=0 open=1"
check "★ENGLISH では ○ がとじる"        "$(state opt_en_cancel.script 800)" "top=0 page=0 mode=0 sel=0 open=0"

# ★カナリア: 頁を開いても**本文の履歴は 1 行も伸びない**
BASE=$("$PY" sim.py zenmai-zork.psexe --script opt_open.script --polls --stop 800 \
       --max 400000000 --dump "$TOTAL,4" 2>/dev/null | sed -n 's/^DUMP: //p')
SHOWN=$("$PY" sim.py zenmai-zork.psexe --script opt_page2.script --polls --stop 900 \
       --max 400000000 --dump "$TOTAL,4" 2>/dev/null | sed -n 's/^DUMP: //p')
check "★カナリア: 本文が汚れない(履歴が伸びない)" "$SHOWN" "$BASE"

echo
if [ "$ng" = 0 ]; then echo "--- 12 件すべて通った ---"; else echo "--- ★$ng 件 食い違った ---"; exit 1; fi
