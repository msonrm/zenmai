#!/bin/sh
# オプション(打っていないときの Start)の検証。
#
# ★かたちは「**本文に重ねる小さなメニュー** → 行き先は**全画面の読み物**」。
#   メニューは道しるべ、頁が目的地。だから確かめるのは
#     ・板とメニューの状態が動くか
#     ・頁から Start で**一足で**本文へ戻るか(× は一段だけ)
#     ・★**どのフェイスボタンでも決まる**か(案内を画面に書かないので、作法に乗る)
#     ・★**「もじの うちかた」だけは面ボタンで戻らない**か(面ボタンは字を出す側)
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

# clen と comp を読んで、打たれた字をそのまま出す
typed() {
    "$PY" sim.py zenmai-zork.psexe --script "$1" --polls --stop "$2" --max 400000000 \
        --dump "$TYPED,28" 2>/dev/null | sed -n 's/^DUMP: //p' | "$PY" -c "
import sys, ast
b = ast.literal_eval(sys.stdin.read().strip())
n = int.from_bytes(b[0:4], 'little')
print(''.join(chr(int.from_bytes(b[4 + 2*i:6 + 2*i], 'little')) for i in range(min(n, 12))))
"
}

# clen だけを読む
typed_n() {
    "$PY" sim.py zenmai-zork.psexe --script "$1" --polls --stop "$2" --max 900000000 \
        --dump "$TYPED,4" 2>/dev/null | sed -n 's/^DUMP: //p' | "$PY" -c "
import sys, ast
print(int.from_bytes(ast.literal_eval(sys.stdin.read().strip()), 'little'))
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
check "★△でも決まる(面ボタンはどれでも)" "$(state opt_face_tri.script 800)" "top=0 page=0 mode=1 sel=0 open=1"
check "↓↓ ○ でライセンスの頁へ"         "$(state opt_page2.script 900)"    "top=0 page=2 mode=1 sel=2 open=1"
check "★頁の中は ↓ 1 回で 1 頁送る"      "$(state opt_scroll.script 950)"   "top=14 page=2 mode=1 sel=2 open=1"
check "面ボタンで一段もどる(メニューは開いたまま)" "$(state opt_back.script 950)" "top=0 page=2 mode=0 sel=2 open=1"
check "★うちかたの頁は面ボタンで戻らない" "$(state help_face.script 800)"  "top=0 page=0 mode=1 sel=0 open=1"
check "★うちかたの頁は Start で本文へ"   "$(state help_start.script 800)" "top=0 page=0 mode=0 sel=0 open=0"
check "★頁の Start は一足で本文へ"      "$(state opt_startout.script 900)" "top=0 page=0 mode=0 sel=0 open=0"
check "★Start で開いたら Start で閉じる" "$(state opt_close.script 800)"   "top=0 page=0 mode=0 sel=0 open=0"
check "★ENGLISH でも作法は同じ(× で頁へ)" "$(state opt_en.script 800)"     "top=0 page=0 mode=1 sel=0 open=1"

# ★カナリア: 頁を開いても**本文の履歴は 1 行も伸びない**
BASE=$("$PY" sim.py zenmai-zork.psexe --script opt_open.script --polls --stop 800 \
       --max 400000000 --dump "$TOTAL,4" 2>/dev/null | sed -n 's/^DUMP: //p')
SHOWN=$("$PY" sim.py zenmai-zork.psexe --script opt_page2.script --polls --stop 900 \
       --max 400000000 --dump "$TOTAL,4" 2>/dev/null | sed -n 's/^DUMP: //p')
check "★カナリア: 本文が汚れない(履歴が伸びない)" "$SHOWN" "$BASE"

# ★試し打ち: 本文と**同じ道**で打っているか（clen / comp をそのまま読む）。
#   ★頭が「え」なら、頁を開けた ○ がそのまま字になっている（一度そうなった）
TYPED=$(sym clen)
check "★試し打ちで字が入る(開けたボタンは字にならない)" "$(typed help_type.script 900)" "かじ"
check "★試し打ちの字を本文へ持ち出さない"       "$(typed help_leave.script 950)"  ""
check "★わ行はフリックと同じ並び(を / ん / ー)"   "$(typed help_wa.script 950)"     "をんー"
check "★欄は幅で止まる(かな 21 字)"              "$(typed_n help_full.script 1950)" "21"

echo
if [ "$ng" = 0 ]; then echo "--- 17 件すべて通った ---"; else echo "--- ★$ng 件 食い違った ---"; exit 1; fi
