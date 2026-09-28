#!/usr/bin/env python3
"""PC-98 版の画面の構造 (2026-09-27〜28 に決めたもの・色は仮) を、screen.asm が読むデータにする。

  上の帯      0〜31    場所 (左・黄) と状態 (右) を 0 行目に。字 8〜23 の上下に 8 ずつ
  本文の上    32〜44   装飾と同じ色 (本文 1 行目のふりがなの上端 49 の 4 ラスタ上まで)
  左右の装飾  幅 48    縁の線は描かない
  本文        2〜13 行目・横 64〜575 (1 行 全角 32 字)
  入力欄の枠  352〜399 15 行目の字 368〜383 の上下に 16 ずつ。上に 2px の縁

出力 (out/): text.bin / attr.bin (80x17 語) / planes.rle (B・R・G・I の 4 プレーンを順に RLE) / pal.bin (16 色 x G,R,B)
RLE = (個数 1〜255, 値) の組の列。
"""
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
import layout as L

L.MARGIN = 64                 # 本文の左端 (装飾 48 + 16)。1 行 = 全角 32 字
SIDE = 48                     # 左右の装飾の幅
TOP_H = 32                    # 上の帯 0〜31 (0 行目の字 8〜23 の上下に 8 ずつ)
IN_Y = 352                    # 入力欄の枠の上端。15 行目の字 368〜383 の上下に 16 ずつ
BODY_LAST = 13                # 本文の最終行 (字 320〜335)
COLS, ROWS = 80, 17
RUBY_DY = 1

# パレット (番号: G,R,B 各 0〜15)。★仮の色
PAL = {0: (0, 0, 0),          # 背景
       1: (3, 2, 7),          # 上の帯 (紺)
       2: (4, 7, 3),          # 左右の装飾 (焦げ茶)
       3: (5, 2, 5),          # 入力欄の枠 (深緑がかった青)
       4: (9, 5, 9),          # 入力欄の枠の縁 (明るめ)
       5: (6, 10, 5),         # 装飾の縁 (明るめの茶)
       8: (10, 10, 10)}       # ふりがな (灰)
A_WHITE, A_YELLOW, A_CYAN = 0xE1, 0xC1, 0xA1

text = [[0x20] * COLS for _ in range(ROWS)]
attr = [[A_WHITE] * COLS for _ in range(ROWS)]
idx = bytearray(640 * 400)    # 画素ごとのパレット番号

def rect(x0, y0, x1, y1, c):
    for y in range(y0, y1):
        idx[y * 640 + x0:y * 640 + x1] = bytes([c]) * (x1 - x0)

def pc98_code(ch):
    if ch == '—': ch = '―'
    e = ch.encode('euc_jp')
    j = ((e[0] & 0x7f) << 8) | (e[1] & 0x7f)
    return L.swap(L.swap(j, L.J90), L.J78)

def put(row, col, s, a=A_WHITE):
    for ch in s:
        if ord(ch) < 0x80:
            text[row][col] = ord(ch); attr[row][col] = a; col += 1
        else:
            p = pc98_code(ch)
            left = ((p & 0xff) << 8) | ((p >> 8) - 0x20)
            text[row][col] = left; text[row][col + 1] = left | 0x80
            attr[row][col] = attr[row][col + 1] = a
            col += 2
    return col

def ruby_at(x, y, yomi):
    for i, ch in enumerate(yomi):
        for r, bits in enumerate(L.ruby_glyph(ch)):
            for c, v in enumerate(bits):
                if v: idx[(y + r) * 640 + x + 8 * i + c] = 8

def para(row, s, a=A_WHITE):
    units = []
    for seg, y in L.segments(s):
        if y: units.append((seg, y))
        else: units += [(ch, None) for ch in seg]
    lines, cur, wsum = [], [], 0
    for u in units:
        w = sum(L.cw(c) for c in u[0])
        if wsum + w > L.W - 2 * L.MARGIN and cur and u[0] not in '、。」）！？…':
            lines.append(cur); cur, wsum = [], 0
        cur.append(u); wsum += w
    if cur: lines.append(cur)
    for ln in lines:
        if row > BODY_LAST: break
        x, last_end = L.MARGIN, 0
        for seg, y in ln:
            x1 = x + sum(L.cw(c) for c in seg)
            put(row, x // 8, seg, a)
            if y:
                rw = 8 * len(y)
                rx = max(x + ((x1 - x) - rw) // 2, last_end, L.MARGIN - 8)
                ruby_at(rx, row * L.ROW + RUBY_DY, y)
                last_end = rx + rw
            x = x1
        row += 1
    return row

# ---- グラフィック (背景の構造) ----
rect(0, 0, 640, TOP_H, 1)                         # 上の帯
rect(0, TOP_H, SIDE, IN_Y, 2)                     # 左の装飾
rect(640 - SIDE, TOP_H, 640, IN_Y, 2)             # 右の装飾
TOP_DECO = 2 * L.ROW + RUBY_DY - 4                 # 45 = 本文 1 行目のふりがなの上端 (49) の 4 ラスタ上
rect(0, TOP_H, 640, TOP_DECO, 2)                  # 本文の上 (32〜44) を装飾と同じ色で
rect(0, IN_Y, 640, 400, 3)                        # 入力欄の枠
rect(0, IN_Y, 640, IN_Y + 2, 4)                   # 枠の上の縁

# ---- テキスト ----
put(0, L.MARGIN // 8, 'ダムのロビー', A_YELLOW)
put(0, 80 - 8 - 20, 'Score: 0   Moves: 12')
r = 2
put(r, L.MARGIN // 8, '> あんないしょをよむ', A_CYAN); r += 1
GUIDE = '「 治水ダム第三号| | 治水ダム第三号は、堂々たる冷たい川を治めるため、大地下帝国七八三年に建造された。この事業は、全能なる地元の暴君、過剰王ディムウィット・フラットヘッド卿からの三千七百万ゾークミッドの下賜によって支えられた。この壮大な構造物は三十七万立方フィートのコンクリートから成り、中央の高さ二百五十六フィート、上端の幅百九十三フィート。'
for part in GUIDE.split('|'):
    part = part.strip()
    if not part: r += 1; continue
    r = para(r, part)
put(15, L.MARGIN // 8, '> _')

# ---- 書き出し ----
def rle(b):
    out, i = bytearray(), 0
    while i < len(b):
        v, n = b[i], 1
        while i + n < len(b) and b[i + n] == v and n < 255: n += 1
        out += bytes([n, v]); i += n
    return bytes(out)

planes = []
for bit in range(4):                               # B, R, G, I
    pl = bytearray(80 * 400)
    for p in range(640 * 400):
        if idx[p] >> bit & 1: pl[p >> 3] |= 0x80 >> (p & 7)
    planes.append(rle(pl))
out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'out')
os.makedirs(out, exist_ok=True)
w16 = lambda rows: b''.join(v.to_bytes(2, 'little') for rw in rows for v in rw)
open(os.path.join(out, 'text.bin'), 'wb').write(w16(text))
open(os.path.join(out, 'attr.bin'), 'wb').write(w16(attr))
open(os.path.join(out, 'planes.rle'), 'wb').write(b''.join(planes) + b'\0')
open(os.path.join(out, 'pal.bin'), 'wb').write(b''.join(bytes(PAL.get(i, (0, 0, 0))) for i in range(16)))
print('rows used', r, 'rle bytes', [len(p) for p in planes])
