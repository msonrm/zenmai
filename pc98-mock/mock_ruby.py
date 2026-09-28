#!/usr/bin/env python3
"""ふりがなの見え方の見本 (PC-98 を通さず Python で描く。2026-09-27)。

gen_screen.py + screen.asm の前段で、ふりがなの色 (白か灰か) と帯の中の位置を決めるのに使った。
決まったのは **灰・1px 下げ** (ruby_gray_dy1_*)。本文の余白は 32px の頃のもの。
出力: out/ruby_gray_dy1_{1x,2x,zoom4x,words3x}.png
"""
import os, sys
from PIL import Image
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from layout import *

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'out')
os.makedirs(OUT, exist_ok=True)

# ---- 画面 ----
PAL = {'bg': (0, 0, 0), 'ink': (255, 255, 255), 'dim': (170, 170, 170), 'acc': (85, 255, 255)}

class Screen:
    def __init__(self, ruby_col):
        self.im = Image.new('RGB', (W, H), PAL['bg'])
        self.px = self.im.load()
        self.ruby_col = ruby_col
    def blit(self, x, y, m, col, bg=None):
        for r, row in enumerate(m):
            for c, v in enumerate(row):
                if v: self.px[x + c, y + r] = col
                elif bg: self.px[x + c, y + r] = bg
    def text(self, row, x, s, col=PAL['ink'], bg=None):
        y = row * ROW + BAND
        for ch in s:
            self.blit(x, y, ank_glyph(ch) if cw(ch) == 8 else kanji_glyph(ch), col, bg)
            x += cw(ch)
        return x
    def fill_row(self, row, col):
        for y in range(row * ROW + BAND, row * ROW + ROW):
            for x in range(W): self.px[x, y] = col

    def para(self, row, s, col=PAL['ink']):
        """本文を折り返して置く。ふりがなの付いた語は割らない。返り値 = 次の行"""
        segs = segments(s)
        # 折り返し: 語の単位 (ふりがな付きは 1 単位、それ以外は 1 字) で詰める
        units = []
        for seg, y in segs:
            if y: units.append((seg, y))
            else: units += [(ch, None) for ch in seg]
        lines, cur, wsum = [], [], 0
        for u in units:
            w = sum(cw(c) for c in u[0])
            if wsum + w > W - 2 * MARGIN and cur and u[0] not in '、。」）！？…':
                lines.append(cur); cur, wsum = [], 0
            cur.append(u); wsum += w
        if cur: lines.append(cur)
        for ln in lines:
            x, last_end = MARGIN, 0
            for seg, y in ln:
                x1 = self.text(row, x, seg, col)
                if y:
                    rw = 8 * len(y)
                    rx = x + ((x1 - x) - rw) // 2
                    rx = max(rx, last_end, MARGIN - 8)
                    for i, ch in enumerate(y):
                        self.blit(rx + 8 * i, row * ROW + RUBY_DY, ruby_glyph(ch), self.ruby_col)
                    last_end = rx + rw
                x = x1
            row += 1
        return row

def screen(ruby_col, name):
    sc = Screen(ruby_col)
    # 0 行目 = 状態行 (反転)
    sc.fill_row(0, PAL['ink'])
    sc.text(0, 16, '木の上', PAL['bg'])
    sc.text(0, 640 - 16 - 8 * 22, 'Score: 0   Moves: 12', PAL['bg'])
    r = 1
    r = sc.para(r, '薄暗い森を抜けて曲がりくねる小道だ。道はここでは南北に向かっている。道の端に、低い枝を張ったとりわけ大きな木が一本立っている。')
    r += 0
    sc.text(r, MARGIN, '> きにのぼる', PAL['acc']); r += 1
    sc.text(r, MARGIN, '木の上', PAL['acc']); r += 1
    r = sc.para(r, '鳥の巣の中に、宝石をちりばめた大きな卵がある。子のない小鳥が拾ってきたものらしい。卵は繊細な金の象嵌に覆われ、瑠璃と真珠母で飾られている。ふつうの卵とは違って蝶番が付いており、いかにも壊れやすそうな留め金で閉じられている。ひどく脆く見える。')
    # 最終行 = 入力欄
    sc.text(15, MARGIN, '> たまごをとる', PAL['ink'])
    x = MARGIN + 8 * 2 + 16 * 6
    for yy in range(15 * ROW + BAND + 14, 15 * ROW + BAND + 16):
        for xx in range(x, x + 16): sc.px[xx, yy] = PAL['ink']
    sc.im.save(os.path.join(OUT, f'{name}_1x.png'))
    sc.im.resize((W * 2, H * 2), Image.NEAREST).save(os.path.join(OUT, f'{name}_2x.png'))
    # 拡大 (4 倍): 卵の段落の頭
    y0 = 6 * ROW
    sc.im.crop((0, y0, 320, y0 + ROW * 4)).resize((320 * 4, ROW * 4 * 4), Image.NEAREST) \
        .save(os.path.join(OUT, f'{name}_zoom4x.png'))
    return r

def samples(ruby_col, name):
    """読みの長い語・半濁音・拗音の見本 (1 行 1 語)"""
    words = ['取扱説明書', '吸血蝙蝠', '台所の窓', '岩壁', '一片', '立方', '上流', '中央', '円匙', '扉', '卵']
    sc = Screen(ruby_col)
    r = 0
    for i in range(0, len(words), 3):
        sc.para(r, '　'.join(words[i:i + 3]) + '　がある。')
        r += 1
    img = sc.im.crop((0, 0, 400, r * ROW))
    img.resize((img.width * 3, img.height * 3), Image.NEAREST).save(os.path.join(OUT, f'{name}_words3x.png'))

used = screen(PAL['dim'], 'ruby_gray_dy1')
samples(PAL['dim'], 'ruby_gray_dy1')
print('rows used', used)
