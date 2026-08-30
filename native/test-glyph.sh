#!/bin/sh
# 字の境界（glyph.h）の検査 —— 器から出ないこと・器の中で切れないこと。
# ★2026-08-30 に実機で踏んだバグ（24 行の器に descender を書いて .bss を壊す）の
#   再発防止。詳しくは test_glyph.c の頭書き。
set -e
cd "$(dirname "$0")"
CC="${CC:-cc}"
TMP="${TMPDIR:-/tmp}"

[ -f zenmai.otf ] || { echo "SKIP: zenmai.otf が無い（sh make_font.sh <KH の dir> で作る）"; exit 0; }

$CC -O1 -Wall -DZM_SDL -I. test_glyph.c glyph_ft.c \
    $(pkg-config --cflags --libs freetype2) -o "$TMP/zm-test-glyph"
ZENMAI_FONT="$PWD/zenmai.otf" "$TMP/zm-test-glyph"
