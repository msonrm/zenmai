#!/usr/bin/env python3
"""本文の左右の縁の絵柄（.MAG）を作る。出力 = <出力先>/<名前>.MAG（既定 pc98-out/）。

★絵柄は適当でよい（msonrm・2026-10-01）: 8px の格子を場面らしい数色でランダム風に塗ったタイル。
  80×320 の 1 枚 = 左の縁（40）+ 右の縁（40）。MAG は 16 色で、絵柄が使うのはパレットの 6・7・9〜15
  （0〜5・8 は UI の予約で、本体は読み込んでも上書きしない = pc98_theme.h）。
★MAG（MAKI02）の符号化は QuuBee の magimage.js（デコーダ）を仕様の参照に、書いたもの。
  8px のタイルは 1 ユニット（4 バイト）なので、同じ色のタイルは上の行からのコピーで畳める。
"""
import os, random, sys

W, H = 80, 320
# 場面ごとのパレット（#RGB・使う番号 = 6, 7, 9, 10, 11）
SCENES = {
    'FIELD':  ['241', '352', '463', '230', '574'],
    'WATER':  ['135', '246', '357', '124', '468'],
    'TEMPLE': ['324', '435', '546', '213', '657'],
    'DEEP':   ['211', '322', '433', '100', '544'],
    'HOUSE':  ['532', '643', '754', '421', '865'],
}
IDX = [6, 7, 9, 10, 11]
COPY = [None, (0, 1), (0, 2), (0, 4), (1, 0), (1, 1), (2, 0), (2, 1), (2, 2),
        (4, 0), (4, 1), (4, 2), (8, 0), (8, 1), (8, 2), (16, 0)]


def tiles(name):
    """8px 格子の色番号 (rows, cols)。隣と同じ色になりやすくして、まだら模様にする"""
    rnd = random.Random(name)
    rows, cols = H // 8, W // 8
    t = [[0] * cols for _ in range(rows)]
    for y in range(rows):
        for x in range(cols):
            c = rnd.randrange(len(IDX))
            if rnd.random() < 0.35 and x % 5:
                c = t[y][x - 1]
            elif rnd.random() < 0.35 and y:
                c = t[y - 1][x]
            t[y][x] = c
    return t


def encode(name, pal_hex):
    t = tiles(name)
    bw = W // 2                                 # 1 行のバイト数（4bpp）
    img = bytearray(bw * H)
    for y in range(H):
        for x in range(W):
            i = IDX[t[y // 8][x // 8]]
            if x % 2 == 0:
                img[y * bw + x // 2] |= i << 4
            else:
                img[y * bw + x // 2] |= i
    units = bw // 4
    flag = [0] * units
    abits, bbytes, pix = [], bytearray(), bytearray()
    out = bytearray(len(img))
    for y in range(H):
        tgt, lits = [], []
        for c in range(units):
            pos = y * bw + c * 4
            f = 0
            for half in (0, 1):
                dst = pos + half * 2
                w = img[dst:dst + 2]
                n = 0
                for k in range(1, 16):
                    dy, dx = COPY[k]
                    s = dst - (dy * bw + dx * 2)
                    if s >= 0 and img[s:s + 2] == w:
                        n = k
                        break
                f |= n << (4 if half == 0 else 0)
                if n == 0:
                    lits.append(w)
            tgt.append(f)
        for c in range(units):
            if tgt[c] != flag[c]:
                abits.append(1)
                bbytes.append(tgt[c] ^ flag[c])
                flag[c] = tgt[c]
            else:
                abits.append(0)
        for w in lits:
            pix += w
    abytes = bytearray()
    for i in range(0, len(abits), 8):
        b = 0
        for j in range(8):
            b = b << 1 | (abits[i + j] if i + j < len(abits) else 0)
        abytes.append(b)
    pal = bytearray()
    for i in range(16):
        h = pal_hex[IDX.index(i)] if i in IDX else {0: '000'}.get(i, '888')
        r, g, b = (int(c, 16) * 17 for c in h)
        pal += bytes([g, r, b])                   # G, R, B の順・各 0〜255
    offA = 32 + len(pal)
    offB = offA + len(abytes)
    offP = offB + len(bbytes)
    hd = bytearray(32)
    hd[1] = 0                                     # 機種
    hd[3] = 0                                     # 16 色・400 ライン
    for k, v in enumerate((0, 0, W - 1, H - 1)):
        hd[4 + k * 2:6 + k * 2] = v.to_bytes(2, 'little')
    for k, v in ((12, offA), (16, offB), (20, len(bbytes)), (24, offP), (28, len(pix))):
        hd[k:k + 4] = v.to_bytes(4, 'little')
    return b'MAKI02  ' + b'zenmai edge pattern ' + name.encode() + b'\x1a' + bytes(hd) + bytes(pal) + bytes(abytes) + bytes(bbytes) + bytes(pix)


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else 'pc98-out'
    os.makedirs(outdir, exist_ok=True)
    for name, pal in SCENES.items():
        data = encode(name, pal)
        with open(os.path.join(outdir, name + '.MAG'), 'wb') as f:
            f.write(data)
        print(f'{name}.MAG: {len(data)} バイト')


if __name__ == '__main__':
    main()
