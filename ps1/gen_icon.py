#!/usr/bin/env python3
"""メモリーカードのアイコンとタイトルを card_icon.h に出す。

BIOS のメモリーカード管理画面に出る 16×16・4bpp の絵と、16 色 CLUT、
それに Shift-JIS のタイトル(64 バイト)を作る。

★タイトルに作品名は入れない。名乗るのはこのソフトの名前だけ、という
  リポジトリ全体の方針(README「商標の姿勢」)をセーブ画面にも通す。

使い方: python3 gen_icon.py
"""
import math
from pathlib import Path

W = H = 16
TITLE = 'ぜんまい'                       # BIOS の管理画面に出る名前


def clut(r, g, b, solid=True):
    """PS1 の 16bit CLUT: bit0-4=R bit5-9=G bit10-14=B bit15=STP。
    0x0000 は完全透明、0x8000 は不透明の黒。"""
    return (0x8000 if solid else 0) | (r & 31) | ((g & 31) << 5) | ((b & 31) << 10)


# 0 = 透明 / 1..3 = 渦の芯から外へ(新芽の色) / 4 = 縁
PAL = [
    0x0000,
    clut(14, 27, 10), clut(9, 22, 7), clut(5, 16, 5),
    clut(3, 9, 3),
] + [0x0000] * 11

px = [[0] * W for _ in range(H)]
cx, cy = 7.5, 8.0        # 描いてみると上に寄るので中心を少し下げる
# アルキメデス螺旋を 2 回転。外へ行くほど濃くする(芽の先が明るい)
STEPS = 4000
for t in range(STEPS):
    a = t / STEPS * 4.0 * math.pi
    rr = 1.1 + (a / (4.0 * math.pi)) * 6.2   # 芯を空ける(0.4 だと中心が潰れた)
    x = int(round(cx + rr * math.cos(a)))
    y = int(round(cy + rr * math.sin(a)))
    if 0 <= x < W and 0 <= y < H:
        px[y][x] = 1 + int((a / (4.0 * math.pi)) * 2.99)

# ★縁取りは付けない。16×16 に 2 回転を描くと線の間隔が 1〜2 画素しかなく、
#   8 近傍を塗った時点で**渦が塗り潰されて消える**(試して分かった)。
#   BIOS は黒背景で描くので、明るい線だけで足りる。
# 芽の先(外周)から芯へ向かって暗くする
for y in range(H):
    for x in range(W):
        if px[y][x]:
            px[y][x] = 4 - px[y][x] if px[y][x] < 4 else 1

# 4bpp: 1 バイトに 2 画素、下位ニブルが左
bitmap = []
for y in range(H):
    for x in range(0, W, 2):
        bitmap.append((px[y][x] & 15) | ((px[y][x + 1] & 15) << 4))

title = TITLE.encode('shift_jis')
assert len(title) <= 64, 'タイトルが 64 バイトを超えている'
title = title + b'\x00' * (64 - len(title))


def carr(name, data, per=12, fmt='0x%02X'):
    out = ['static const unsigned char %s[%d] = {' % (name, len(data))]
    for i in range(0, len(data), per):
        out.append('    ' + ', '.join(fmt % b for b in data[i:i + per]) + ',')
    out.append('};')
    return '\n'.join(out)


src = ['/* gen_icon.py が生成。手で編集しない */',
       '/* BIOS の管理画面に出るアイコン(16x16 4bpp)・16 色 CLUT・Shift-JIS のタイトル */',
       '',
       carr('CARD_TITLE_SJIS', title),
       '',
       carr('CARD_ICON_BITMAP', bitmap),
       '',
       'static const unsigned short CARD_ICON_CLUT[16] = {',
       '    ' + ', '.join('0x%04X' % c for c in PAL[:8]) + ',',
       '    ' + ', '.join('0x%04X' % c for c in PAL[8:16]) + ',',
       '};',
       '']
Path(__file__).with_name('card_icon.h').write_text('\n'.join(src))

art = '\n'.join(''.join('.#*+o'[px[y][x]] for x in range(W)) for y in range(H))
print(art)
print('card_icon.h: タイトル %d バイト / 絵 %d バイト' % (len(title), len(bitmap)))
