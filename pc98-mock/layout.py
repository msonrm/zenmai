#!/usr/bin/env python3
"""PC-98 版の画面の試作で共有する部品: 字形 (本文・ふりがな)・ふりがなの割り当て・寸法。

本文 = QuuBee の font.bmp (16px 漢字・8x16 ANK = 漢字 ROM の代わり)
ふりがな = 美咲ゴシック 7x7 (8x8 の枠)
行 = 24 ラスタ (上 8 = ふりがなの帯 / 下 16 = 本文)

素材の場所は環境変数で変えられる:
  QB_DIR      QuuBee のリポジトリ (既定 ~/development/qb)。web/assets/font.bmp を読む
  MISAKI_BDF  美咲ゴシックの BDF (既定 pc98-mock/misaki_gothic.bdf = 追跡しない。README 参照)
"""
import json, re, os

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BMP = os.path.join(os.environ.get('QB_DIR', os.path.expanduser('~/development/qb')), 'web/assets/font.bmp')
MISAKI = os.environ.get('MISAKI_BDF', os.path.join(HERE, 'misaki_gothic.bdf'))
JA_JSON = os.path.join(HERE, '..', 'assets', 'zork1-ja.json')
W, H = 640, 400
ROW = 24            # 1 行のラスタ数
BAND = 8            # ふりがなの帯
RUBY_DY = 1         # ふりがなを帯の中で下げる量 (本文に接してよい。上の行から離す)
MARGIN = 32         # 本文の左右の余白 (Zenmai の MARGIN と同じ。gen_screen.py は 64 に上書きする)

# ---- font.bmp (2048x2048 1bpp・下から上・0 = 字) ----
raw = open(FONT_BMP, 'rb').read()
bw, bh = int.from_bytes(raw[18:22], 'little'), int.from_bytes(raw[22:26], 'little')
off = int.from_bytes(raw[10:14], 'little')
stride = bw // 8
def bmp_bit(x, y):
    b = raw[off + (bh - 1 - y) * stride + x // 8]
    return not (b >> (7 - x % 8)) & 1

# 字 → 漢字 ROM の位置は native/pc98_jis.py が正典 (本体の表もそこから作る)
import sys
sys.path.insert(0, os.path.join(HERE, '..', 'native'))
import pc98_jis

def kanji_glyph(ch):
    """全角 1 字 → 16x16 の真偽表。U+2014 は ― (JIS 213D) に寄せる"""
    p = pc98_jis.rom_of(ch)
    x0, y0 = ((p >> 8) - 0x20) * 16, (p & 0xff) * 16
    return [[bmp_bit(x0 + x, y0 + y) for x in range(16)] for y in range(16)]

def ank_glyph(ch):
    c = ord(ch)
    return [[bmp_bit(c * 8 + x, y) for x in range(8)] for y in range(16)]

# ---- 美咲 BDF (Unicode・8x8 の枠・ascent 6) ----
misaki = {}
with open(MISAKI, encoding='latin1') as f:
    cur = None
    for line in f:
        t = line.split()
        if not t: continue
        if t[0] == 'ENCODING': cur = {'enc': int(t[1])}
        elif t[0] == 'BBX': cur['bbx'] = tuple(map(int, t[1:5]))
        elif t[0] == 'BITMAP': cur['rows'] = []
        elif t[0] == 'ENDCHAR':
            misaki[cur['enc']] = cur; cur = None
        elif cur is not None and 'rows' in cur: cur['rows'].append(int(t[0], 16))
def ruby_glyph(ch):
    g = misaki[ord(ch)]
    w, h, xo, yo = g['bbx']
    top = 6 - (h + yo)
    m = [[False] * 8 for _ in range(8)]
    for r, bits in enumerate(g['rows']):
        for x in range(w):
            if (bits >> (7 - x)) & 1:
                m[top + r][xo + x] = True
    return m

# ---- ふりがなの割り当て (src/ruby.js と同じ規則) ----
d = json.load(open(JA_JSON, encoding='utf-8'))
ruby = d['ruby']
keys = sorted(ruby, key=len, reverse=True)
KANJI = re.compile('[一-龥々]')
def segments(s):
    out, i = [], 0
    while i < len(s):
        k = next((x for x in keys if s.startswith(x, i)
                  and not (KANJI.match(s[i]) and i > 0 and KANJI.match(s[i - 1]))
                  and not (KANJI.match(x[-1]) and i + len(x) < len(s) and KANJI.match(s[i + len(x)]))), None)
        if not k:
            out.append((s[i], None)); i += 1; continue
        out += [(seg, y) for seg, y in ruby[k]]
        i += len(k)
    return out

def cw(ch): return 8 if ord(ch) < 0x80 else 16
