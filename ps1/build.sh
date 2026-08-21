#!/bin/sh
# C → PS-EXE。前提: gen_data.py で glyphs.h / content.h を生成済み。
set -e
cd "$(dirname "$0")"
OUT="${1:-zenmai-scroll.psexe}"
PY="${PY:-python3}"

# PS1 は R3000(mips1・FPU なし)。-G0 = gp 相対を使わない(crt0 無しで動かすため)
CFLAGS="-O2 -march=mips1 -mabi=32 -msoft-float -mno-abicalls -fno-pic -G0 \
        -ffreestanding -fno-builtin -nostdlib -Wall -Wextra"

mipsel-linux-gnu-gcc $CFLAGS -c main.c -o main.o
mipsel-linux-gnu-gcc $CFLAGS -c render.c -o render.o
mipsel-linux-gnu-gcc $CFLAGS -c content_data.c -o content_data.o
mipsel-linux-gnu-gcc $CFLAGS -c input.c -o input.o
mipsel-linux-gnu-gcc $CFLAGS -c jp_text.c -o jp_text.o
mipsel-linux-gnu-gcc $CFLAGS -c ruby_data.c -o ruby_data.o
mipsel-linux-gnu-ld -T link.ld main.o render.o content_data.o input.o jp_text.o ruby_data.o -o out.elf
mipsel-linux-gnu-objcopy -O binary out.elf out.bin
"$PY" pack_exe.py out.bin "$OUT"
