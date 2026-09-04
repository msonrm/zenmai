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

# ★検査どうし・検査とビルドの**並行実行を止める**。
#   このディレクトリの zenmai-zork.psexe と out-zork.elf は共有物なので、
#   走っている最中に別のビルドが入ると**足元が入れ替わり、シンボル表と中身が
#   食い違う**（2026-08-30 に踏んだ。ライセンス頁の検査が偽の赤を出した）。
#   ★今日 4 回踏んだ「古い成果物」の罠の並行実行版。
if ! mkdir .test-lock 2>/dev/null; then
    echo "★別の検査かビルドが走っています（.test-lock）。終わってからにしてください。" >&2
    echo "  （異常終了で残ったなら: rmdir native/.test-lock）" >&2
    exit 1
fi
trap 'rmdir .test-lock 2>/dev/null' EXIT INT TERM
ZM_IN_TEST=1
export ZM_IN_TEST
PY="${PY:-python3}"
ng=0

# ★**毎回焼き直す**（4 秒）。以前は `[ -f ... ] || ./build.sh` だったが、
#   これは **psexe が古いまま、シンボル表だけ新しい ELF から取る**事故を起こす ——
#   検査は「知らない番地」を覗いてゴミを読み、★**移植のせいで壊れたように見える**。
#   （2026-08-29 に実際に踏んだ。3 件が偽の赤になった）
#   ★出力の対（out-zork.elf と zenmai-zork.psexe）は**必ず同じビルドから**にする。
./build.sh >/dev/null 2>&1
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

# ★★メニューの「やめる」も**同じ行き先**であること。メニューは打つ道の近道であって、
#   別の道ではない —— 分かれていたら、片方でしか通らない値が必ず出る。
MID2=$(hist menu_quit_ja_mid.script 660)
if [ "$MID2" != "0" ]; then echo "✓ ★カナリア: メニューを開くまでに本文が積まれている（${MID2}px）"
else echo "✗ ★カナリア: メニューを開くまでに本文が積まれている"; echo "    実際: 0px"; ng=$((ng + 1)); fi

check "★メニューの「やめる」→「はい」で起動メニューへ戻る" "$(hist menu_quit_ja.script 1080)" "0"
check "★英語面のメニューでも同じ（QUIT → y）"              "$(hist menu_quit_en.script 1060)" "0"

echo
if [ "$ng" = 0 ]; then echo "--- 7 件すべて通った ---"; else echo "--- ★$ng 件 食い違った ---"; exit 1; fi
