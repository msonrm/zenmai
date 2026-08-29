#!/bin/sh
# card.c を sim 上で検証する PS-EXE。前提: gen_icon.py 実行済み(card_icon.h)。
set -e
cd "$(dirname "$0")"
PY="${PY:-python3}"
CFLAGS="-O1 -march=mips1 -mabi=32 -mfp32 -mno-abicalls -fno-pic -G0 \
        -ffreestanding -fno-builtin -nostdlib -Wall"
mipsel-linux-gnu-gcc $CFLAGS -c card_test.c -o card_test.o
mipsel-linux-gnu-gcc $CFLAGS -c card.c -o card_t.o
mipsel-linux-gnu-ld -T link.ld card_test.o card_t.o -o out-card.elf
mipsel-linux-gnu-objcopy -O binary out-card.elf out-card.bin
"$PY" pack_exe.py out-card.bin card-test.psexe
mipsel-linux-gnu-nm out-card.elf | grep -i ' result$'
