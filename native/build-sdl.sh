#!/bin/sh
# Zenmai 本体（英語 / 日本語、起動時選択）→ SDL2 の実行ファイル。
# PortMaster（aarch64 Linux）と開発機のデスクトップで同じものが動く。
#
# ★PS1 版（build.sh）との差はこれだけ:
#     - plat_ps1.c → plat_sdl.c   （機械に触る 9 本。境界は plat.h）
#     - card.c     → save_file.c  （セーブ先がメモリーカードからファイルへ）
#     - lib.c / link.ld / pack_exe.py は **要らない**（glibc がある）
#   ★描画の芯・組版・グリフ・履歴・窓・入力の状態機械・訳は 1 行も変えずに共有する。
#
# 前提: gen_data.py / gen_tables.py 実行済み（glyphs.h / content.h / tables.h）。
#       libsdl2-dev（Debian/Ubuntu: apt install libsdl2-dev）。
#
#   sh build-sdl.sh                 → ./zenmai-zork（開発機）
#   ZENMAI_WINDOW=1 ./zenmai-zork   → 全画面ではなく窓で出す
set -e
cd "$(dirname "$0")"
OUT="${1:-zenmai-zork}"
CC="${CC:-cc}"
PY="${PY:-python3}"

# ★字の実装を選ぶ（glyph.h の境界）:
#   GLYPH=glyph_baked.c … 焼いたビットマップ。**PS1 と画素一致する**ので検査に使う（既定）
#   GLYPH=glyph_ft.c    … FreeType。字が増え、プロポーショナルになり、AA が乗る。配布用
GLYPH=${GLYPH:-glyph_baked.c}
CFLAGS="${CFLAGS:--O2 -Wall} -DZM_SDL -I. $(pkg-config --cflags sdl2)"
LDFLAGS="$(pkg-config --libs sdl2)"
if [ "$GLYPH" = "glyph_ft.c" ]; then
    CFLAGS="$CFLAGS $(pkg-config --cflags freetype2)"
    LDFLAGS="$LDFLAGS $(pkg-config --libs freetype2)"
fi

# ★story はリンカで焼き込む（PS1 と同じ手）。ファイルを外に置くと
#   「同梱してある」という主張が配布の形に依存してしまう。
cp ../vendor/zork1/zork1.z3 story.bin
ARCH=$(uname -m)
case "$ARCH" in
    aarch64) OFMT="elf64-littleaarch64"; OBIN="aarch64" ;;
    x86_64)  OFMT="elf64-x86-64";        OBIN="i386:x86-64" ;;
    *) echo "未知のアーキテクチャ: $ARCH（objcopy の書式を足すこと）" >&2; exit 1 ;;
esac
objcopy -I binary -O "$OFMT" -B "$OBIN" story.bin story-sdl.o

SRC="main.c render.c plat_sdl.c $GLYPH content_data.c input.c translate.c translate_data.c \
     ruby_data.c jp_text.c cmd.c cmd_data.c save_file.c"

# shellcheck disable=SC2086
$CC $CFLAGS $SRC story-sdl.o -o "$OUT" $LDFLAGS

echo "OK: $OUT ($(du -h "$OUT" | cut -f1))"
