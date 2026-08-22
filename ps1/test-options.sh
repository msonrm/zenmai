#!/bin/sh
# オプション画面(打っていないときの Start)の検証。
# ★ライセンスは専用の画面を持たず**本文の履歴へ流す**ので、確かめるのは
#   「履歴の高さが増えたか」になる(長文のスクロールは既にあるものがそのまま効く)。
set -e
cd "$(dirname "$0")"
PY="${PY:-python3}"
ng=0

sym() { mipsel-linux-gnu-nm out-zork.elf | grep -i " $1\$" | cut -d' ' -f1; }
run() { "$PY" sim.py zenmai-zork.psexe --script "$1" --polls --stop "$2" \
        --max 400000000 --dump "$3,4" 2>/dev/null | sed -n 's/^DUMP: //p'; }

check() {
    if [ "$2" = "$3" ]; then echo "✓ $1"
    else echo "✗ $1"; echo "    実際: $2"; echo "    期待: $3"; ng=$((ng + 1)); fi
}

[ -f zenmai-zork.psexe ] || ./build-zork.sh >/dev/null 2>&1
OPEN=$(sym opt_open)
TOTAL=$(sym total_h)

check "Start で開く(打っていないときだけ)" "$(run opt_open.script 700 "$OPEN")" "b'\\x01\\x00\\x00\\x00'"
check "× で閉じる"                          "$(run opt_close.script 800 "$OPEN")" "b'\\x00\\x00\\x00\\x00'"
check "Start でトグル(開けたボタンで閉じる)"  "$(run opt_toggle.script 800 "$OPEN")" "b'\\x00\\x00\\x00\\x00'"

BASE=$(run /dev/null 800 "$TOTAL" 2>/dev/null || true)
printf '60-90:START\n' > /tmp/zm-opt-base.script
BASE=$(run /tmp/zm-opt-base.script 800 "$TOTAL")
AFTER=$(run opt_license.script 800 "$TOTAL")
check "○ でライセンスが本文へ流れる(履歴が伸びる)" "$([ "$BASE" != "$AFTER" ] && echo yes || echo no)" "yes"
check "★カナリア: 開かなければ履歴は伸びない" "$BASE" "b'h\\x01\\x00\\x00'"

echo
if [ "$ng" = 0 ]; then echo "--- 5 件すべて通った ---"; else echo "--- ★$ng 件 食い違った ---"; exit 1; fi
