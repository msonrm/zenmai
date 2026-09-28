#!/bin/sh
# PC-98 版の画面の試作を作って QuuBee で撮る: gen_screen.py → nasm → QuuBee (headless) → out/screen-{1x,2x}.png
# 要るもの: python3 (Pillow)・nasm・node・QuuBee のリポジトリ (QB_DIR)・美咲ゴシックの BDF (README)
set -e
cd "$(dirname "$0")"
python3 gen_screen.py
nasm -I out/ -f bin -o out/SCREEN.COM screen.asm
mkdir -p out/game
cp out/SCREEN.COM out/game/
printf 'SCREEN\r\n' > out/game/RUN.BAT
node shot.js
python3 -c "
from PIL import Image
Image.open('out/screen-1x.png').resize((1280, 800), Image.NEAREST).save('out/screen-2x.png')"
echo "→ out/screen-2x.png (見本 = screen-2x.png と見比べる)"
