#!/usr/bin/env python3
"""PS-EXE をミニ MIPS シミュレータで実行して検証する。

確かめること:
  1. GP1 への書き込み列 = リセット / 表示モード 0x27(640×480i NTSC 15bpp) /
     表示範囲 / 表示開始 / 表示 ON
  2. GP0 の A0h 転送が (0,0) 640×480 で、画素データが mock-1x.png と完全一致

使い方: python3 verify_psexe.py zenmai-ps1-mock.psexe
"""
import struct
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).parent
exe = Path(sys.argv[1]).read_bytes()

assert exe[:8] == b'PS-X EXE'
pc0, load, size = (struct.unpack_from('<I', exe, o)[0] for o in (0x10, 0x18, 0x1C))
mem = bytearray(2 * 1024 * 1024)
phys = load & 0x1FFFFF
mem[phys:phys + size] = exe[2048:2048 + size]

reg = [0] * 32
pc = pc0
gp0, gp1 = [], []
steps = 0
while steps < 3_000_000:
    op = struct.unpack_from('<I', mem, pc & 0x1FFFFF)[0]
    steps += 1
    npc = pc + 4
    o, rs, rt = op >> 26, (op >> 21) & 31, (op >> 16) & 31
    imm = op & 0xFFFF
    simm = imm - 0x10000 if imm & 0x8000 else imm
    if op == 0:
        pass
    elif o == 0x0F:                                    # lui
        reg[rt] = (imm << 16) & 0xFFFFFFFF
    elif o == 0x0D:                                    # ori
        reg[rt] = reg[rs] | imm
    elif o == 0x09:                                    # addiu
        reg[rt] = (reg[rs] + simm) & 0xFFFFFFFF
    elif o == 0x23:                                    # lw
        reg[rt] = struct.unpack_from('<I', mem, (reg[rs] + simm) & 0x1FFFFF)[0]
    elif o == 0x2B:                                    # sw
        a = (reg[rs] + simm) & 0x1FFFFFFF
        if a == 0x1F801810:
            gp0.append(reg[rt])
        elif a == 0x1F801814:
            gp1.append(reg[rt])
        else:
            struct.pack_into('<I', mem, a & 0x1FFFFF, reg[rt])
    elif o == 0x05:                                    # bne(遅延スロットは次周で実行)
        if reg[rs] != reg[rt]:
            delay = struct.unpack_from('<I', mem, npc & 0x1FFFFF)[0]
            assert delay == 0, '遅延スロットが nop でない'
            npc = npc + 4 * simm
            steps += 1
    elif o == 0x02:                                    # j
        target = (pc & 0xF0000000) | (op & 0x3FFFFFF) << 2
        if target == pc:
            break                                      # 自己ループ = 終了
        npc = target
    else:
        raise SystemExit(f'未対応命令 0x{op:08X} @ 0x{pc:08X}')
    reg[0] = 0
    pc = npc
else:
    raise SystemExit('終了せず(命令数上限)')

print(f'実行 {steps} 命令で自己ループに到達')
print('GP1 列:', [f'0x{v:08X}' for v in gp1])
assert gp1 == [0x00000000, 0x08000027, 0x06C60260, 0x07042010, 0x05000000, 0x03000000], 'GP1 列が想定と違う'

assert gp0[0] == 0xA0000000 and gp0[1] == 0 and gp0[2] == (480 << 16) | 640, 'A0h 転送ヘッダが違う'
data = gp0[3:]
assert len(data) == 640 * 480 // 2, f'転送語数 {len(data)}'

img = Image.open(HERE / 'mock-1x.png').convert('RGB')
expect = [(r >> 3) | (g >> 3) << 5 | (b >> 3) << 10 for r, g, b in img.getdata()]
got = []
for w in data:
    got += [w & 0xFFFF, w >> 16]
assert got == expect, '画素が PNG と一致しない'
print('OK: GP1 設定列・転送ヘッダ・画素 307,200 点すべて一致')
