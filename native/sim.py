#!/usr/bin/env python3
"""R3000(PS1 CPU)整数命令シミュレータ — エミュレータ無しの開発リグ。

PS-EXE を実行し、GP0/GP1 への書き込みを収集する。A0h 転送を仮想 VRAM に適用し、
--png で VRAM の 640×480 を PNG に書き出す。--expect で PNG と画素一致を検証する。

対応: MIPS I 整数命令(mult/div 系含む)。分岐遅延スロットは実機どおり実行する。
ロード遅延は模さない(gas が nop を詰める前提。実機で踏む前に gas を信じる)。

使い方: python3 sim.py <exe.psexe> [--png out.png] [--expect ref.png] [--max N]
"""
import argparse
import struct
import sys
from pathlib import Path

M32 = 0xFFFFFFFF


def s32(v):
    return v - 0x100000000 if v & 0x80000000 else v


CARD_BYTES = 128 * 1024                        # 16 ブロック × 64 フレーム × 128B


def card_reply(joy, card):
    """メモリーカード(装置 81h)の応答。psx-spx の Read/Write シーケンスをそのまま写す。

    ★sim が模すのは**論理**だけ。実機の /ACK の間合いや書き込み待ちは模していないので、
    ここが緑でも「実機で速すぎて落ちる」型は捕まらない(GPU の FIFO と同じ穴)。
    """
    i, tx = joy['idx'], joy['tx']
    cmd = tx[1] if len(tx) > 1 else 0
    if i <= 0:
        return 0xFF
    if i == 1:
        return 0x08                            # FLAG(bit3 = まだ書かれていない)
    if i == 2:
        return 0x5A
    if i == 3:
        return 0x5D
    sect = (((tx[4] << 8) | tx[5]) & 0x3FF) if len(tx) > 5 else 0
    if cmd == 0x52:                            # ---- Read Frame ----
        if i in (4, 5):
            return 0x00
        if i == 6:
            return 0x5C
        if i == 7:
            return 0x5D
        if i == 8:
            return (sect >> 8) & 0xFF
        if i == 9:
            return sect & 0xFF
        if 10 <= i < 138:
            return card[sect * 128 + (i - 10)]
        if i == 138:
            chk = (sect >> 8) ^ (sect & 0xFF)
            for b in card[sect * 128:(sect + 1) * 128]:
                chk ^= b
            return chk & 0xFF
        if i == 139:
            return 0x47                        # "G" = Good
        return 0xFF
    if cmd == 0x57:                            # ---- Write Frame ----
        if i <= 134:
            return 0x00                        # アドレス・データ・CHK を送っている間
        if i == 135:
            data = bytes(tx[6:134])
            chk = (sect >> 8) ^ (sect & 0xFF)
            for b in data:
                chk ^= b
            joy['bad'] = (chk & 0xFF) != tx[134]
            if not joy['bad']:
                card[sect * 128:(sect + 1) * 128] = data
                joy['dirty'] = True
            return 0x5C
        if i == 136:
            return 0x5D
        if i == 137:
            return 0x4E if joy.get('bad') else 0x47
        return 0xFF
    return 0xFF


def run(exe_path, max_steps, pad=None, card_path=None):
    """pad = (toggle, stop): デジタルパッドを接続扱いにし、× を toggle フィールド周期で
    押し離しする。stop フィールドに達したら停止(最終画面の対話ループは自発終了しないため)。
    card_path: メモリーカードの像(128KB)。あれば読み込み、書き換わっていれば書き戻す。"""
    exe = Path(exe_path).read_bytes()
    assert exe[:8] == b'PS-X EXE'
    pc0, load, size = (struct.unpack_from('<I', exe, o)[0] for o in (0x10, 0x18, 0x1C))
    sp0 = struct.unpack_from('<I', exe, 0x30)[0]
    mem = bytearray(2 * 1024 * 1024)
    mem[load & 0x1FFFFF:(load & 0x1FFFFF) + size] = exe[2048:2048 + size]

    r = [0] * 32
    r[29] = sp0
    hi = lo = 0
    pc = pc0
    gp0, gp1 = [], []
    branch = None          # (発火 pc, 飛び先) — 遅延スロット実行後に反映

    def rd_w(a): return struct.unpack_from('<I', mem, a & 0x1FFFFC)[0]
    def rd_h(a): return struct.unpack_from('<H', mem, a & 0x1FFFFE)[0]
    def rd_b(a): return mem[a & 0x1FFFFF]

    joy = {'idx': 0, 'rx': 0xFF, 'polls': 0, 'dev': 0, 'tx': [], 'dirty': False}
    card = bytearray(CARD_BYTES)
    if card_path and Path(card_path).exists():
        raw = Path(card_path).read_bytes()[:CARD_BYTES]
        card[:len(raw)] = raw

    def write(a, val, sz):
        pa = a & 0x1FFFFFFF
        if pa == 0x1F801810:
            gp0.append(val & M32)
            return
        if pa == 0x1F801814:
            gp1.append(val & M32)
            return
        if pa == 0x1F80104A:                          # JOY_CTRL → 交換をリセット
            joy['idx'] = 0
            joy['tx'] = []
            joy['dev'] = 0
            return
        if pa == 0x1F801040:                          # JOY_DATA 送信 → 応答を用意
            joy['tx'].append(val & 0xFF)
            if joy['idx'] == 0:
                joy['dev'] = val & 0xFF
            if joy['dev'] == 0x81:                    # メモリーカード
                if joy['idx'] == 0:
                    joy['cardio'] = joy.get('cardio', 0) + 1
                joy['rx'] = card_reply(joy, card)
                joy['idx'] += 1
                return
            if pad is None:
                joy['rx'] = 0xFF
            else:
                if val == 0x42:
                    joy['polls'] += 1                 # ポーリング回数(閉ループ台本の時間軸)
                field = joy['polls'] if (pad[0] == 'script' and len(pad) > 3 and pad[3]) \
                    else steps // 2000
                if pad[0] == 'script':
                    mask = 0
                    for f0, f1, m in pad[1]:
                        if f0 <= field < f1:
                            mask |= m
                    btn = 0xFFFF & ~(mask & 0xFFFF)   # 負論理
                    lx = 0x00 if mask >> 16 & 1 else 0xFF if mask >> 17 & 1 else 0x80
                    ly = 0x00 if mask >> 18 & 1 else 0xFF if mask >> 19 & 1 else 0x80
                    rx = 0x00 if mask >> 20 & 1 else 0xFF if mask >> 21 & 1 else 0x80
                    ry = 0x00 if mask >> 22 & 1 else 0xFF if mask >> 23 & 1 else 0x80
                    # DualShock アナログ応答(id 0x73 + RX,RY,LX,LY)
                    seq = (0xFF, 0x73, 0x5A, btn & 0xFF, btn >> 8, rx, ry, lx, ly)
                else:
                    field = steps // 2000
                    mask = 0x4000 if (field // pad[0]) % 2 == 1 else 0   # × = bit14
                    btn = 0xFFFF & ~mask
                    seq = (0xFF, 0x41, 0x5A, btn & 0xFF, btn >> 8)
                joy['rx'] = seq[min(joy['idx'], len(seq) - 1)]
                joy['idx'] += 1
            return
        if pa >= 0x1F000000:
            return                                    # その他の I/O は読み捨て
        if sz == 4:
            struct.pack_into('<I', mem, pa & 0x1FFFFC, val & M32)
        elif sz == 2:
            struct.pack_into('<H', mem, pa & 0x1FFFFE, val & 0xFFFF)
        else:
            mem[pa & 0x1FFFFF] = val & 0xFF

    steps = 0
    while steps < max_steps:
        op = rd_w(pc)
        steps += 1
        npc = pc + 4
        if branch is not None:
            npc = branch
            branch = None
        o = op >> 26
        rs, rt, rd = (op >> 21) & 31, (op >> 16) & 31, (op >> 11) & 31
        sh, fn = (op >> 6) & 31, op & 63
        imm = op & 0xFFFF
        simm = imm - 0x10000 if imm & 0x8000 else imm

        if o == 0:                                    # SPECIAL
            if fn == 0x00: r[rd] = (r[rt] << sh) & M32           # sll
            elif fn == 0x02: r[rd] = r[rt] >> sh                 # srl
            elif fn == 0x03: r[rd] = (s32(r[rt]) >> sh) & M32    # sra
            elif fn == 0x04: r[rd] = (r[rt] << (r[rs] & 31)) & M32
            elif fn == 0x06: r[rd] = r[rt] >> (r[rs] & 31)
            elif fn == 0x07: r[rd] = (s32(r[rt]) >> (r[rs] & 31)) & M32
            elif fn == 0x08: branch = r[rs]                      # jr
            elif fn == 0x09: r[rd or 31], branch = (pc + 8) & M32, r[rs]   # jalr
            elif fn == 0x10: r[rd] = hi
            elif fn == 0x12: r[rd] = lo
            elif fn == 0x11: hi = r[rs]
            elif fn == 0x13: lo = r[rs]
            elif fn == 0x18:                                     # mult
                v = s32(r[rs]) * s32(r[rt]); hi, lo = (v >> 32) & M32, v & M32
            elif fn == 0x19:                                     # multu
                v = r[rs] * r[rt]; hi, lo = (v >> 32) & M32, v & M32
            elif fn == 0x1A:                                     # div
                a, b = s32(r[rs]), s32(r[rt])
                if b: lo, hi = (int(a / b)) & M32, (a - int(a / b) * b) & M32
            elif fn == 0x1B:                                     # divu
                a, b = r[rs], r[rt]
                if b: lo, hi = (a // b) & M32, (a % b) & M32
            elif fn in (0x20, 0x21): r[rd] = (r[rs] + r[rt]) & M32   # add/addu
            elif fn in (0x22, 0x23): r[rd] = (r[rs] - r[rt]) & M32   # sub/subu
            elif fn == 0x24: r[rd] = r[rs] & r[rt]
            elif fn == 0x25: r[rd] = r[rs] | r[rt]
            elif fn == 0x26: r[rd] = r[rs] ^ r[rt]
            elif fn == 0x27: r[rd] = ~(r[rs] | r[rt]) & M32
            elif fn == 0x2A: r[rd] = 1 if s32(r[rs]) < s32(r[rt]) else 0
            elif fn == 0x2B: r[rd] = 1 if r[rs] < r[rt] else 0
            else: raise SystemExit(f'未対応 SPECIAL fn=0x{fn:02X} @ 0x{pc:08X}')
        elif o == 0x01:                               # REGIMM: bltz/bgez(+al)
            cond = s32(r[rs]) < 0 if (rt & 1) == 0 else s32(r[rs]) >= 0
            if rt & 0x10: r[31] = (pc + 8) & M32
            if cond: branch = (pc + 4 + 4 * simm) & M32
        elif o == 0x02: branch = (pc & 0xF0000000) | (op & 0x3FFFFFF) << 2   # j
        elif o == 0x03:                               # jal
            r[31] = (pc + 8) & M32
            branch = (pc & 0xF0000000) | (op & 0x3FFFFFF) << 2
        elif o == 0x04:
            if r[rs] == r[rt]: branch = (pc + 4 + 4 * simm) & M32   # beq
        elif o == 0x05:
            if r[rs] != r[rt]: branch = (pc + 4 + 4 * simm) & M32   # bne
        elif o == 0x06:
            if s32(r[rs]) <= 0: branch = (pc + 4 + 4 * simm) & M32  # blez
        elif o == 0x07:
            if s32(r[rs]) > 0: branch = (pc + 4 + 4 * simm) & M32   # bgtz
        elif o in (0x08, 0x09): r[rt] = (r[rs] + simm) & M32        # addi/addiu
        elif o == 0x0A: r[rt] = 1 if s32(r[rs]) < simm else 0       # slti
        elif o == 0x0B: r[rt] = 1 if r[rs] < (simm & M32) else 0    # sltiu
        elif o == 0x0C: r[rt] = r[rs] & imm
        elif o == 0x0D: r[rt] = r[rs] | imm
        elif o == 0x0E: r[rt] = r[rs] ^ imm
        elif o == 0x0F: r[rt] = (imm << 16) & M32                   # lui
        elif o == 0x20: v = rd_b(r[rs] + simm); r[rt] = (v - 0x100 if v & 0x80 else v) & M32
        elif o == 0x21: v = rd_h(r[rs] + simm); r[rt] = (v - 0x10000 if v & 0x8000 else v) & M32
        elif o == 0x24 and (r[rs] + simm) & 0x1FFFFFFF >= 0x1F000000:
            r[rt] = joy['rx'] if (r[rs] + simm) & 0x1FFFFFFF == 0x1F801040 else 0
        elif o == 0x23:
            a = (r[rs] + simm) & 0x1FFFFFFF
            if a == 0x1F801814:                       # GPUSTAT: bit31 が 2000 命令ごとに反転
                r[rt] = 0x1C000000 | ((steps // 2000 & 1) << 31)
            elif a == 0x1F801044:                     # JOY_STAT: 常に TX ready / RX あり
                r[rt] = 0x7
            elif a >= 0x1F000000:
                r[rt] = 0
            else:
                r[rt] = rd_w(a)
        elif o == 0x24: r[rt] = rd_b(r[rs] + simm)
        elif o == 0x25: r[rt] = rd_h(r[rs] + simm)
        elif o == 0x28: write(r[rs] + simm, r[rt], 1)
        elif o == 0x29: write(r[rs] + simm, r[rt], 2)
        elif o == 0x2B: write(r[rs] + simm, r[rt], 4)
        else:
            raise SystemExit(f'未対応 op=0x{o:02X} @ 0x{pc:08X}')
        r[0] = 0

        if branch == pc:                              # 自己分岐(j / beq $0,$0 等)= 正常終了
            break
        if pad is not None:
            # ★--polls のときは停止も**ポーリング回数**で測る。フィールド数(steps//2000)で
            #   止めると、起動が重い版では台本の 1 行目に着く前に切れる —— しかも
            #   「exit 0 で完走」に見えるので、**打っていないのに通ったと読み違える**。
            #   台本の時間軸をポーリングにした理由(処理の重さに追従する)は停止条件にも要る。
            cur = joy['polls'] if (pad[0] == 'script' and len(pad) > 3 and pad[3]) \
                else steps // 2000
            if cur >= (pad[2] if pad[0] == 'script' else pad[1]):
                break                                 # 台本の停止点に到達
        pc = npc
    else:
        raise SystemExit('終了せず(命令数上限)')
    print('ポーリング %d 回 / カード交換 %d 回%s'
          % (joy['polls'], joy.get('cardio', 0), ' ★書き換えあり' if joy['dirty'] else ''),
          file=sys.stderr)
    if card_path and joy['dirty']:
        Path(card_path).write_bytes(bytes(card))
        print('メモリーカード →', card_path)
    return steps, gp0, gp1, mem


def vram_from_gp0(gp0):
    """GP0 列から A0h 転送を解釈して仮想 VRAM(1024×512)を作る。"""
    vram = [[0] * 1024 for _ in range(512)]
    i = 0
    while i < len(gp0):
        if gp0[i] >> 24 == 0x80:                      # VRAM 面内コピー
            sx, sy = gp0[i + 1] & 0xFFFF, gp0[i + 1] >> 16
            dx, dy = gp0[i + 2] & 0xFFFF, gp0[i + 2] >> 16
            w, h = gp0[i + 3] & 0xFFFF, gp0[i + 3] >> 16
            block = [vram[sy + r][sx:sx + w] for r in range(h)]
            for r in range(h):
                vram[dy + r][dx:dx + w] = block[r]
            i += 4
        elif gp0[i] >> 24 == 0x02:                    # 矩形フィル(色は 24bit → 555 へ切り詰め)
            col = gp0[i]
            p = (col & 0xFF) >> 3 | (col >> 8 & 0xFF) >> 3 << 5 | (col >> 16 & 0xFF) >> 3 << 10
            x, y = gp0[i + 1] & 0xFFFF, gp0[i + 1] >> 16
            w, h = gp0[i + 2] & 0xFFFF, gp0[i + 2] >> 16
            for r_ in range(y, y + h):
                for c_ in range(x, x + w):
                    vram[r_][c_] = p
            i += 3
        elif gp0[i] >> 24 == 0xA0:
            x, y = gp0[i + 1] & 0xFFFF, gp0[i + 1] >> 16
            w, h = gp0[i + 2] & 0xFFFF, gp0[i + 2] >> 16
            n = (w * h + 1) // 2
            px = []
            for word in gp0[i + 3:i + 3 + n]:
                px += [word & 0xFFFF, word >> 16]
            for k in range(min(w * h, len(px))):      # 停止で途切れた転送は書けた分だけ
                vram[y + k // w][x + k % w] = px[k]
            i += 3 + n
        else:
            i += 1
    return vram


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('--png')
    ap.add_argument('--expect')
    ap.add_argument('--max', type=int, default=20_000_000)
    ap.add_argument('--pad', help='"toggle,stop": パッド接続 + × を toggle フィールド周期で押す')
    ap.add_argument('--script', help='ボタン台本ファイル: 行 = "f0-f1:UP+TRI" 形式。#コメント可')
    ap.add_argument('--dump', help='"hexaddr,hexlen": 終了時に RAM を印字(デバッグ用)')
    ap.add_argument('--stop', type=int, default=40000, help='--script 時の停止フィールド')
    ap.add_argument('--polls', action='store_true', help='台本の時間軸をパッドのポーリング回数にする(閉ループ)')
    ap.add_argument('--card', help='メモリーカードの像(128KB)。読み込み、書き換わったら書き戻す')
    args = ap.parse_args()

    pad = tuple(int(x) for x in args.pad.split(',')) if args.pad else None
    if args.script:
        BITS = {'SEL': 0, 'L3': 1, 'R3': 2, 'START': 3, 'UP': 4, 'RIGHT': 5, 'DOWN': 6, 'LEFT': 7,
                'L2': 8, 'R2': 9, 'L1': 10, 'R1': 11, 'TRI': 12, 'CIR': 13, 'X': 14, 'SQ': 15,
                'LSL': 16, 'LSR': 17, 'LSU': 18, 'LSD': 19,   # 左スティック(疑似ビット)
                'RSL': 20, 'RSR': 21, 'RSU': 22, 'RSD': 23}   # 右スティック(疑似ビット)
        windows = []
        for line in Path(args.script).read_text().splitlines():
            line = line.split('#')[0].strip()
            if not line:
                continue
            rng, names = line.split(':')
            f0, f1 = (int(x) for x in rng.split('-'))
            mask = 0
            for nm in names.split('+'):
                mask |= 1 << BITS[nm.strip()]
            windows.append((f0, f1, mask))
        pad = ('script', windows, args.stop, args.polls)
    steps, gp0, gp1, mem = run(args.exe, args.max, pad, args.card)
    print(f'実行 {steps} 命令 / GP1 {len(gp1)} 件: ' + ' '.join(f'0x{v:08X}' for v in gp1))
    if args.dump:
        parts = args.dump.split(',')
        addr, dlen = int(parts[0], 16), int(parts[1], 16)
        data = bytes(mem[addr & 0x1FFFFF:(addr & 0x1FFFFF) + dlen])
        if len(parts) > 2:
            Path(parts[2]).write_bytes(data)
            print(f'DUMP → {parts[2]} ({dlen} bytes)')
        else:
            print('DUMP:', repr(data))

    if not (args.png or args.expect):
        return
    vram = vram_from_gp0(gp0)
    if args.png and not args.expect:
        # ★PIL が無い環境でも画面を見たいので、PNG は自前で書ける道を用意する
        #   (Pillow は入っていないことがある。ここで詰まると寸法が目で確かめられない)
        write_png(args.png, vram)
        print('VRAM →', args.png)
        return
    from PIL import Image
    img = Image.new('RGB', (640, 480))
    img.putdata([((p & 31) << 3, (p >> 5 & 31) << 3, (p >> 10 & 31) << 3)
                 for row in vram[:480] for p in row[:640]])
    if args.png:
        img.save(args.png)
        print('VRAM →', args.png)
    if args.expect:
        ref = Image.open(args.expect).convert('RGB')
        a = [(r >> 3, g >> 3, b >> 3) for r, g, b in img.getdata()]
        b_ = [(r >> 3, g >> 3, b >> 3) for r, g, b in ref.getdata()]
        assert a == b_, '画素が一致しない'
        print('OK: 画素 307,200 点一致')


def write_png(path, vram):
    """RGB555 の VRAM を 640x480 の PNG に落とす(zlib だけで書く)"""
    import struct, zlib
    raw = bytearray()
    for y in range(480):
        raw.append(0)                       # フィルタ = None
        row = vram[y]
        for x in range(640):
            p = row[x]
            raw += bytes(((p & 31) << 3, (p >> 5 & 31) << 3, (p >> 10 & 31) << 3))

    def chunk(tag, data):
        return (struct.pack('>I', len(data)) + tag + data
                + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF))

    Path(path).write_bytes(
        b'\x89PNG\r\n\x1a\n'
        + chunk(b'IHDR', struct.pack('>IIBBBBB', 640, 480, 8, 2, 0, 0, 0))
        + chunk(b'IDAT', zlib.compress(bytes(raw), 6))
        + chunk(b'IEND', b''))


if __name__ == '__main__':
    main()
