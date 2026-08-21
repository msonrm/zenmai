#!/bin/sh
# 英語版 Zork(MojoZork 内蔵)→ PS-EXE。前提: gen_data.py / gen_tables.py 実行済み。
set -e
cd "$(dirname "$0")"
OUT="${1:-zenmai-en.psexe}"
PY="${PY:-python3}"

# -msoft-float は付けない(glibc ヘッダが soft ABI を拒む。FP を使うコードは無いので
#  hard ABI フラグでも FP 命令は出ない)
CFLAGS="-O1 -march=mips1 -mabi=32 -mfp32 -mno-abicalls -fno-pic -G0 \
        -ffreestanding -fno-builtin -nostdlib -Wall"

cp ../vendor/zork1/zork1.z3 story.bin
mipsel-linux-gnu-objcopy -I binary -O elf32-tradlittlemips -B mips story.bin story.o
mipsel-linux-gnu-gcc $CFLAGS -I. -c zm_main.c -o zm_main.o
mipsel-linux-gnu-gcc $CFLAGS -c render.c -o render.o
mipsel-linux-gnu-gcc $CFLAGS -c content_data.c -o content_data.o
mipsel-linux-gnu-gcc $CFLAGS -c input.c -o input.o
mipsel-linux-gnu-gcc $CFLAGS -c lib.c -o lib.o
mipsel-linux-gnu-ld -T link.ld zm_main.o render.o content_data.o input.o lib.o story.o -o out-en.elf
mipsel-linux-gnu-objcopy -O binary out-en.elf out-en.bin
"$PY" pack_exe.py out-en.bin "$OUT"
