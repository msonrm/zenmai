#!/usr/bin/env python3
"""mock-1x.png を表示するだけの PS-EXE を作る。

構成: PS-EXE ヘッダ(2048B) + 手組みの MIPS コード(表示設定 + VRAM 転送) + RGB555 画像。
GPU を 640×480 インタレース NTSC 15bpp に設定し、画像を VRAM (0,0) へ送って表示する。
ツールチェイン不要 —— 機械語を直接並べる(命令 35 個)。

使い方: python3 make_psexe.py [出力.psexe]
検証:   python3 verify_psexe.py <出力.psexe>  (ミニ MIPS シミュレータで実行し画像一致を確認)
"""
import struct
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).parent
OUT = Path(sys.argv[1] if len(sys.argv) > 1 else HERE / 'zenmai-ps1-mock.psexe')

LOAD = 0x80010000
CODE_PAD = 0x100            # コードは 256B に納め、データはその直後
GP0, GP1 = 0x1810, 0x1814   # 0x1F800000 からのオフセット

# レジスタ番号
ZERO, T0, T2, T3, T4 = 0, 8, 10, 11, 12


def lui(rt, imm): return 0x3C000000 | rt << 16 | (imm & 0xFFFF)
def ori(rt, rs, imm): return 0x34000000 | rs << 21 | rt << 16 | (imm & 0xFFFF)
def addiu(rt, rs, imm): return 0x24000000 | rs << 21 | rt << 16 | (imm & 0xFFFF)
def lw(rt, off, rs): return 0x8C000000 | rs << 21 | rt << 16 | (off & 0xFFFF)
def sw(rt, off, rs): return 0xAC000000 | rs << 21 | rt << 16 | (off & 0xFFFF)
def bne(rs, rt, rel): return 0x14000000 | rs << 21 | rt << 16 | (rel & 0xFFFF)
def j(addr): return 0x08000000 | (addr >> 2) & 0x3FFFFFF
NOP = 0


def load_imm(rt, val):
    """val を rt へ(0 なら命令なし = ZERO を使う側で判断)。"""
    hi, lo = val >> 16, val & 0xFFFF
    if hi and lo:
        return [lui(rt, hi), ori(rt, rt, lo)]
    if hi:
        return [lui(rt, hi)]
    return [ori(rt, ZERO, lo)]


def build_code(data_addr, nwords):
    code = [lui(T0, 0x1F80)]
    # GP1: リセット → 表示モード(640×480i NTSC 15bpp) → 水平/垂直表示範囲 → 表示開始 (0,0)
    for val in [0x00000000, 0x08000027, 0x06C60260, 0x07042010, 0x05000000]:
        if val == 0:
            code.append(sw(ZERO, GP1, T0))
        else:
            code += load_imm(T4, val) + [sw(T4, GP1, T0)]
    # GP0: A0h = CPU→VRAM 転送。転送先 (0,0)・大きさ 640×480
    for val in [0xA0000000, 0x00000000, (480 << 16) | 640]:
        if val == 0:
            code.append(sw(ZERO, GP0, T0))
        else:
            code += load_imm(T4, val) + [sw(T4, GP0, T0)]
    code += load_imm(T2, data_addr)     # 画像データの先頭
    code += load_imm(T3, nwords)        # 転送語数
    loop = LOAD + len(code) * 4
    code += [
        lw(T4, 0, T2),
        NOP,                            # R3000 のロード遅延スロット
        sw(T4, GP0, T0),
        addiu(T2, T2, 4),
        addiu(T3, T3, -1),
        bne(T3, ZERO, (loop - (LOAD + (len(code) + 6) * 4)) // 4),
        NOP,
    ]
    code += load_imm(T4, 0x03000000) + [sw(T4, GP1, T0)]   # 表示 ON
    hang = LOAD + len(code) * 4
    code += [j(hang), NOP]
    assert len(code) * 4 <= CODE_PAD, f'コードが {CODE_PAD}B を超えた'
    return code


def image_words():
    """mock-1x.png → PS1 の 15bpp(bit0-4=R, 5-9=G, 10-14=B)。2 画素 = 1 語。"""
    img = Image.open(HERE / 'mock-1x.png').convert('RGB')
    assert img.size == (640, 480)
    half = []
    for r, g, b in img.getdata():
        half.append((r >> 3) | (g >> 3) << 5 | (b >> 3) << 10)
    return [half[i] | half[i + 1] << 16 for i in range(0, len(half), 2)]


def main():
    words = image_words()
    code = build_code(LOAD + CODE_PAD, len(words))
    body = b''.join(struct.pack('<I', w) for w in code)
    body += b'\0' * (CODE_PAD - len(body))
    body += b''.join(struct.pack('<I', w) for w in words)
    body += b'\0' * (-len(body) % 2048)

    header = bytearray(2048)
    header[0:8] = b'PS-X EXE'
    struct.pack_into('<I', header, 0x10, LOAD)            # 初期 PC
    struct.pack_into('<I', header, 0x18, LOAD)            # ロード先
    struct.pack_into('<I', header, 0x1C, len(body))       # 本体サイズ(2048 の倍数)
    struct.pack_into('<I', header, 0x30, 0x801FFF00)      # 初期 SP
    region = b'Sony Computer Entertainment Inc. for Japan area'
    header[0x4C:0x4C + len(region)] = region

    OUT.write_bytes(bytes(header) + body)
    print(f'OK: {OUT} ({(2048 + len(body)) / 1024:.0f} KB, 命令 {len(code)} 個)')


if __name__ == '__main__':
    main()
