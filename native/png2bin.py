#!/usr/bin/env python3
"""640×480 PNG → PS1 15bpp(bit0-4=R, 5-9=G, 10-14=B)生バイナリ。"""
import struct
import sys

from PIL import Image

img = Image.open(sys.argv[1]).convert('RGB')
assert img.size == (640, 480)
with open(sys.argv[2], 'wb') as f:
    for r, g, b in img.getdata():
        f.write(struct.pack('<H', (r >> 3) | (g >> 3) << 5 | (b >> 3) << 10))
