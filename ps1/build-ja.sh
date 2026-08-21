#!/bin/sh
# 英語版 Zork(MojoZork 内蔵)→ PS-EXE。前提: gen_data.py / gen_tables.py 実行済み。
set -e
cd "$(dirname "$0")"
OUT="${1:-zenmai-ja.psexe}"
PY="${PY:-python3}"

# -msoft-float は付けない(glibc ヘッダが soft ABI を拒む。FP を使うコードは無いので
#  hard ABI フラグでも FP 命令は出ない)
CFLAGS="-O1 -march=mips1 -mabi=32 -mfp32 -mno-abicalls -fno-pic -G0 \
        -ffreestanding -fno-builtin -nostdlib -Wall"

cp ../vendor/zork1/zork1.z3 story.bin
mipsel-linux-gnu-objcopy -I binary -O elf32-tradlittlemips -B mips story.bin story.o
mipsel-linux-gnu-gcc $CFLAGS -I. -c zm_ja_main.c -o zm_ja_main.o
mipsel-linux-gnu-gcc $CFLAGS -c render.c -o render.o
mipsel-linux-gnu-gcc $CFLAGS -c content_data.c -o content_data.o
mipsel-linux-gnu-gcc $CFLAGS -c input.c -o input.o
mipsel-linux-gnu-gcc $CFLAGS -c lib.c -o lib.o
mipsel-linux-gnu-gcc $CFLAGS -c translate.c -o translate.o
mipsel-linux-gnu-gcc $CFLAGS -c translate_data.c -o translate_data.o
mipsel-linux-gnu-gcc $CFLAGS -c ruby_data.c -o ruby_data.o
mipsel-linux-gnu-gcc $CFLAGS -c jp_text.c -o jp_text.o
mipsel-linux-gnu-gcc $CFLAGS -c cmd.c -o cmd.o
mipsel-linux-gnu-gcc $CFLAGS -c cmd_data.c -o cmd_data.o
mipsel-linux-gnu-ld -T link.ld zm_ja_main.o render.o content_data.o input.o lib.o translate.o translate_data.o ruby_data.o jp_text.o cmd.o cmd_data.o story.o -o out-ja.elf
mipsel-linux-gnu-objcopy -O binary out-ja.elf out-ja.bin
"$PY" pack_exe.py out-ja.bin "$OUT"
