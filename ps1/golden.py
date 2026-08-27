#!/usr/bin/env python3
"""demo_main.c と同じ規則で最終画面(全ターン流し終えた状態)を PIL で作る = ゴールデン。

C 実装(描画層)の独立参照実装。sim.py --expect golden.png で突き合わせる。
グリフの字形は同じフォント・同じラスタライズ経路なので、割り付けが一致すれば
画素も一致する(ずれ = 割り付けの移植ミス)。

使い方: python3 golden.py <フォント dir> golden.png [ターン数(冒頭含む。既定=全部)]
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent / 'ps1-mock'))
import gen_mock
from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).parent
FONT_DIR = Path(sys.argv[1])
OUT = sys.argv[2]

W, H, MARGIN, TEXT_W = 640, 480, 32, 576
TOP, BODY_H, CMD_Y, CMD_H = 24, 400, 432, 24
RUBY_ZONE, BASE, LEAD, BLANK = 14, 24, 8, 24
BG, INK, ACCENT, DIM = (0x14, 0x12, 0x0F), (0xE8, 0xE0, 0xD2), (0xC9, 0xA8, 0x6A), (0x7D, 0x74, 0x66)

asset = json.loads((HERE.parent / 'assets' / 'zork1-ja.json').read_text())
ruby = asset['ruby']
keys = sorted(ruby.keys(), key=len, reverse=True)
turns = json.loads((HERE / 'turns.json').read_text())
if len(sys.argv) > 3:
    turns = turns[:int(sys.argv[3])]
f24 = ImageFont.truetype(str(FONT_DIR / 'KH-Dot-Hibiya-24.ttf'), 24)
f12 = ImageFont.truetype(str(FONT_DIR / 'KH-Dot-Kagurazaka-12.ttf'), 12)

TALL = 4096
canvas = Image.new('RGB', (W, TALL), BG)
cd = ImageDraw.Draw(canvas)
state = {'cursor': 0, 'first': True, 'frags': [], 'fw': 0}


def gw(ch):
    return int(f24.getlength(ch))


def flush(color):
    if not state['frags']:
        return
    is_jp = any(ord(c) > 0x7F for seg, _, _ in state['frags'] for c in seg)
    state['cursor'] += RUBY_ZONE if is_jp else (0 if state['first'] else LEAD)
    state['first'] = False
    y = state['cursor']
    x = MARGIN
    for seg, yomi, w in state['frags']:
        if yomi:
            bw, rw = 24 * len(seg), 12 * len(yomi)
            cd.text((x + (w - bw) // 2, y), seg, font=f24, fill=color, anchor='la')
            cd.text((x + (w - rw) // 2, y - RUBY_ZONE), yomi, font=f12, fill=color, anchor='la')
        else:
            cd.text((x, y), seg, font=f24, fill=color, anchor='la')
        x += w
    state['cursor'] += BASE
    state['frags'] = []
    state['fw'] = 0


def push_char(ch, color):
    w = gw(ch)
    if state['fw'] + w > TEXT_W and state['fw'] > 0:
        flush(color)
        if ch == ' ':
            return
    state['frags'].append((ch, None, w))
    state['fw'] += w


def push_text(s, color):
    """ASCII の語は単語単位で折り返す(render.c push_text と同一規則)。"""
    i = 0
    while i < len(s):
        ch = s[i]
        if ord(ch) > 0x7F or ch == ' ':
            push_char(ch, color)
            i += 1
            continue
        j = i
        w = 0
        while j < len(s) and ord(s[j]) <= 0x7F and s[j] != ' ':
            w += gw(s[j])
            j += 1
        if w <= TEXT_W and state['fw'] + w > TEXT_W and state['fw'] > 0:
            flush(color)
        for k in range(i, j):
            push_char(s[k], color)
        i = j


def draw_logical(line, color):
    if not line.strip():
        state['cursor'] += BLANK
        state['first'] = False
        return
    for seg, yomi in gen_mock.segment(line, ruby, keys):
        if yomi:
            w = max(24 * len(seg), 12 * len(yomi))
            if state['fw'] + w > TEXT_W and state['fw'] > 0:
                flush(color)
            state['frags'].append((seg, yomi, w))
            state['fw'] += w
        else:
            push_text(seg, color)
    flush(color)


for line in turns[0]['lines']:
    draw_logical(line, INK)
for t in turns[1:]:
    state['cursor'] += BLANK + RUBY_ZONE
    cd.text((MARGIN, state['cursor']), '＞' + t['ja'], font=f24, fill=ACCENT, anchor='la')
    state['cursor'] += BASE
    for line in t['lines']:
        draw_logical(line, INK)

view = state['cursor'] - BODY_H
screen = Image.new('RGB', (W, H), BG)
screen.paste(canvas.crop((0, view, W, view + BODY_H)), (0, TOP))
sd = ImageDraw.Draw(screen)
sd.rectangle([0, TOP + BODY_H, W - 1, CMD_Y - 1], fill=BG)   # 念のため(窓外は BG)
sd.text((MARGIN, CMD_Y), '＞', font=f24, fill=ACCENT, anchor='la')
x = MARGIN + 24
sd.rectangle([x + 2, CMD_Y, x + 4, CMD_Y + 23], fill=ACCENT)
kx = W - MARGIN - 24
for c in range(kx - 3, kx + 27):
    screen.putpixel((c, CMD_Y), DIM)
    screen.putpixel((c, CMD_Y + CMD_H - 1), DIM)
for r in range(CMD_Y, CMD_Y + CMD_H):
    screen.putpixel((kx - 3, r), DIM)
    screen.putpixel((kx + 26, r), DIM)
sd.text((kx, CMD_Y), 'あ', font=f24, fill=ACCENT, anchor='la')

screen.point(lambda v: (v >> 3) * 255 // 31).save(OUT)
print('golden:', OUT, f'(最終 cursor={state["cursor"]}px, view={view})')
