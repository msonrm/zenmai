#!/bin/sh
# quit(「やめる」/ quit)の行き先の検証。
#
# ★以前は対話ループを抜けた先が `for (;;) { }` で、**「やめる」→「はい」が
#   そのままフリーズに見えた**（実機の指摘）。いまは _start ごと回り直して
#   **起動メニューへ戻る**。.bss を潰し直すので、本文の履歴も入力欄も
#   lib.c の LIFO アリーナも初期状態に戻る。
# ★見張り方: **本文の履歴の高さ**（total_h）。ゲーム中は積み上がり、
#   起動メニューでは 0。フリーズしていれば quit 直前の値のまま止まる。
set -e
cd "$(dirname "$0")"
PY="${PY:-python3}"
ng=0

[ -f zenmai-zork.psexe ] || ./build.sh >/dev/null 2>&1
TOTAL=$(mipsel-linux-gnu-nm out-zork.elf | grep -i " total_h\$" | cut -d' ' -f1)

# 本文の履歴の高さ(px)を読む
hist() {
    "$PY" sim.py zenmai-zork.psexe --script "$1" --polls --stop "$2" --max 900000000 \
        --dump "$TOTAL,4" 2>/dev/null | sed -n 's/^DUMP: //p' | "$PY" -c "
import sys, ast
print(int.from_bytes(ast.literal_eval(sys.stdin.read().strip()), 'little'))
"
}

check() {
    if [ "$2" = "$3" ]; then echo "✓ $1"
    else echo "✗ $1"; echo "    実際: $2"; echo "    期待: $3"; ng=$((ng + 1)); fi
}

# ★カナリア: そもそも本文が積まれていること（積まれていなければ下の 0 は無意味）
MID=$(hist quit_ja_mid.script 720)
if [ "$MID" != "0" ]; then echo "✓ ★カナリア: quit 前は本文が積まれている（${MID}px）"
else echo "✗ ★カナリア: quit 前は本文が積まれている"; echo "    実際: 0px"; ng=$((ng + 1)); fi

check "★「やめる」→「はい」で起動メニューへ戻る" "$(hist quit_ja.script 1050)" "0"
# ★決めた Start がそのまま言語の決定に化けないこと（化けると本文が始まって 0 でなくなる）
check "★決めた Start で言語が選ばれない（待っても始まらない）" "$(hist quit_ja.script 1300)" "0"
check "★英語面でも同じ（quit → y）"                 "$(hist quit_en.script 1250)" "0"

echo
if [ "$ng" = 0 ]; then echo "--- 4 件すべて通った ---"; else echo "--- ★$ng 件 食い違った ---"; exit 1; fi
