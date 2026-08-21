#!/usr/bin/env python3
"""sim の --dump で吸った canvas(RGB555 raw)を PNG にする。
使い方: python3 canvas2png.py canvas.bin out.png [幅=640 高さ=896]
"""
import struct
import sys

from PIL import Image

data = open(sys.argv[1], 'rb').read()
w = int(sys.argv[3]) if len(sys.argv) > 3 else 640
h = int(sys.argv[4]) if len(sys.argv) > 4 else 896
px = struct.unpack(f'<{w * h}H', data[:w * h * 2])
img = Image.new('RGB', (w, h))
img.putdata([((p & 31) << 3, (p >> 5 & 31) << 3, (p >> 10 & 31) << 3) for p in px])
img.save(sys.argv[2])
print(sys.argv[2])
