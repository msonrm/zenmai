#!/usr/bin/env python3
"""生バイナリ(0x80010000 ロード・先頭がエントリ)を PS-EXE に包む。"""
import struct
import sys
from pathlib import Path

LOAD = 0x80010000
body = Path(sys.argv[1]).read_bytes()
body += b'\0' * (-len(body) % 2048)

header = bytearray(2048)
header[0:8] = b'PS-X EXE'
struct.pack_into('<I', header, 0x10, LOAD)            # 初期 PC
struct.pack_into('<I', header, 0x18, LOAD)            # ロード先
struct.pack_into('<I', header, 0x1C, len(body))       # 本体サイズ(2048 の倍数)
struct.pack_into('<I', header, 0x30, 0x801FFF00)      # 初期 SP
region = b'Sony Computer Entertainment Inc. for Japan area'
header[0x4C:0x4C + len(region)] = region

Path(sys.argv[2]).write_bytes(bytes(header) + body)
print(f'OK: {sys.argv[2]} ({(2048 + len(body)) / 1024:.0f} KB)')
