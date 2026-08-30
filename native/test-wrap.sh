#!/bin/sh
# 割り付け（render.c）の検査 —— 禁則処理。詳しくは test_wrap.c の頭書き。
#
# ★字の実装（glyph.h）は検査が自前で持つ（等幅 12/24）ので、フォントも実機も要らない。
#   焼いた版と同じ送り幅なので、見ているのは PS1 版の実挙動そのもの。
set -e
cd "$(dirname "$0")"
CC="${CC:-cc}"
TMP="${TMPDIR:-/tmp}"

PY="${PY:-python3}"

$CC -O1 -Wall -I. test_wrap.c render.c content_data.c -o "$TMP/zm-test-wrap"
"$TMP/zm-test-wrap"

# ★禁則の字の並びは C と Python の参照実装（gen_mock.py）で同じでなければならない ——
#   片方だけ足すと、golden.py の作るゴールデンと C の実挙動が静かにずれる。
echo
"$PY" check_kinsoku.py
