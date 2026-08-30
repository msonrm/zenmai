#!/bin/sh
# Zenmai 本体(英語 / 日本語、起動時選択)→ PS-EXE。前提: gen_data.py / gen_tables.py 実行済み。
# ★出力名 zenmai-zork.psexe は Releases の配布名。README がこの名前を指している。
set -e
cd "$(dirname "$0")"

# ★検査が走っている最中に焼き直さない（共有の psexe / elf の足元が入れ替わる）。
#   検査自身が呼ぶときは ZM_IN_TEST=1 が立っているので通す。
if [ -z "${ZM_IN_TEST:-}" ] && [ -d .test-lock ]; then
    echo "★検査が走っています（.test-lock）。焼き直すと検査の足元が入れ替わります。" >&2
    exit 1
fi
OUT="${1:-zenmai-zork.psexe}"
PY="${PY:-python3}"

# -msoft-float は付けない(glibc ヘッダが soft ABI を拒む。FP を使うコードは無いので
#  hard ABI フラグでも FP 命令は出ない)
CFLAGS="-O1 -march=mips1 -mabi=32 -mfp32 -mno-abicalls -fno-pic -G0 \
        -ffreestanding -fno-builtin -nostdlib -Wall"

cp ../vendor/zork1/zork1.z3 story.bin
mipsel-linux-gnu-objcopy -I binary -O elf32-tradlittlemips -B mips story.bin story.o
mipsel-linux-gnu-gcc $CFLAGS -I. -c main.c -o main.o
mipsel-linux-gnu-gcc $CFLAGS -c render.c -o render.o
mipsel-linux-gnu-gcc $CFLAGS -c plat_ps1.c -o plat_ps1.o
mipsel-linux-gnu-gcc $CFLAGS -c glyph_baked.c -o glyph_baked.o
mipsel-linux-gnu-gcc $CFLAGS -c content_data.c -o content_data.o
mipsel-linux-gnu-gcc $CFLAGS -c input.c -o input.o
mipsel-linux-gnu-gcc $CFLAGS -c lib.c -o lib.o
mipsel-linux-gnu-gcc $CFLAGS -c translate.c -o translate.o
mipsel-linux-gnu-gcc $CFLAGS -c translate_data.c -o translate_data.o
mipsel-linux-gnu-gcc $CFLAGS -c ruby_data.c -o ruby_data.o
mipsel-linux-gnu-gcc $CFLAGS -c jp_text.c -o jp_text.o
mipsel-linux-gnu-gcc $CFLAGS -c cmd.c -o cmd.o
mipsel-linux-gnu-gcc $CFLAGS -c cmd_data.c -o cmd_data.o
mipsel-linux-gnu-gcc $CFLAGS -c card.c -o card.o
mipsel-linux-gnu-ld -T link.ld main.o render.o plat_ps1.o glyph_baked.o content_data.o input.o lib.o translate.o translate_data.o ruby_data.o jp_text.o cmd.o cmd_data.o card.o story.o -o out-zork.elf
mipsel-linux-gnu-objcopy -O binary out-zork.elf out-zork.bin
"$PY" pack_exe.py out-zork.bin "$OUT"
