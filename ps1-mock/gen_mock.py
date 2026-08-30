#!/usr/bin/env python3
"""Zenmai PS1 版の画面モック生成。

640×480(480i 相当)に、冒頭の文章とコマンド欄を KH ドットフォント
(日比谷24 + 神楽坂12 = docs/ps1-port-plan.md で選定)で描く。
見た目の検証用 —— 本文の読みやすさ・コマンド欄との対比・ルビの可読性。

フォント: https://jikasei.me/font/kh-dotfont/ (SIL OFL 1.1)
  zip: https://ftp.iij.ad.jp/pub/osdn.jp/users/8/8546/khdotfont-20150527.zip
ルビ辞書: ../assets/zork1-ja.json の ruby(送り仮名アラインメント済み・387 語)

使い方: python3 gen_mock.py <フォント dir> <出力 dir>
"""
import json
import re
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

FONT_DIR = Path(sys.argv[1] if len(sys.argv) > 1 else '.')
OUT_DIR = Path(sys.argv[2] if len(sys.argv) > 2 else '.')
ASSET = Path(__file__).parent.parent / 'assets' / 'zork1-ja.json'

W, H = 640, 480
MARGIN = 32                 # 左右 1 文字ぶん(計画書)
TEXT_W = W - MARGIN * 2     # 576px = 全角 24 字
BASE = 24                   # 本文の字面
RUBY_ZONE = 14              # 空き 1 + ルビ 12 + 空き 1。★ルビの**ある行だけ**が持つ
LEAD = 8                    # ルビの無い行(英語バナー等)の行間
BLANK = 24                  # 空行(全高。ルビ域の分は前後の行が持ち込むので実際の空きはもっと広い)
TOP = 24                    # ★縦セーフマージン 5%(実測: R36H + DuckStation の切り抜きで下端が欠けた。
CMD_Y = H - 24 - 24         #   横 32px は元々 5% で無事だった —— 縦も同じ扱いが要る)
# ★最悪ケース(全行ルビ持ち = 38px)でも本文 10 行 = 240 字で、計画書の容量窓
#   203〜348 の内側。ルビ無し行は詰まるだけなので改ページの結論は崩れない

# web 版ダークテーマの紙色(web/index.html :root)
BG = (0x14, 0x12, 0x0F)
INK = (0xE8, 0xE0, 0xD2)
ACCENT = (0xC9, 0xA8, 0x6A)
DIM = (0x7D, 0x74, 0x66)

KANJI = re.compile(r'[一-龥々]')

# 実パイプライン(ZVM + glk-shim + Translator)で取得した冒頭の出力
OPENING = [
    'ZORK I: The Great Underground Empire',
    'Infocom interactive fiction - a fantasy story',
    'Copyright (c) 1981, 1982, 1983, 1984, 1985, 1986 Infocom, Inc. All rights reserved.',
    'ZORK is a registered trademark of Infocom, Inc.',
    'Release 119 / Serial number 880429',
    '',
    '家の西',
    '白い家の西、開けた野原に立っている。家の正面の扉には板が打ちつけられている。',
    'ここに小さな郵便箱がある。',
]
CMD_TEXT = 'ゆうびんばこをあける'


def segment(text, ruby, keys):
    """src/ruby.js の rubify と同じ切り方。[(文字列, 読み|None), ...] を返す。"""
    out = []
    i = 0
    while i < len(text):
        k = next((x for x in keys if text.startswith(x, i)
                  and not (KANJI.match(text[i]) and KANJI.match(text[i - 1:i] or 'x'))
                  and not (KANJI.match(x[-1]) and KANJI.match(text[i + len(x):i + len(x) + 1] or 'x'))), None)
        if not k:
            out.append((text[i], None))
            i += 1
            continue
        for seg, yomi in ruby[k]:
            out.append((seg, yomi))
        i += len(k)
    return out


def char_w(f24, ch):
    return int(f24.getlength(ch))


# ---- 禁則処理（render.c の no_head / no_tail と同一。★正典はこちら側） ----
# ★行頭に来てはいけない字 → その 1 字だけ右の余白へ**ぶら下げる**
NO_HEAD = set('、。，．・：；？！）〕］｝〉》」』】’”ー々ゝゞ゛゜'
               'ぁぃぅぇぉっゃゅょゎゕゖァィゥェォッャュョヮヵヶ')
# ★行末に来てはいけない字 → 次の行へ**道連れ**にする
NO_TAIL = set('（〔［｛〈《「『【‘“')


def wrap(units, f24):
    """ピクセル幅で貪欲に詰める。ルビ単位は原子(行またぎで割らない)。
    ★ASCII の語(空白以外の連続)は単語単位で折り返す(2026-08-19。render.c と同一規則)。
    ★禁則処理を入れた(2026-08-30。ぶら下げ + 道連れの 2 つだけ。render.c と同一規則)。"""
    lines = [[]]
    x = 0

    def line_break():
        """行を送る。★行末禁則の字が末尾に残るなら道連れにする"""
        nonlocal x
        carry = None
        if len(lines[-1]) > 1:
            seg, yomi, w = lines[-1][-1]
            if not yomi and seg in NO_TAIL:
                carry = lines[-1].pop()
                x -= w
        lines.append([])
        x = 0
        if carry:
            lines[-1].append(carry)
            x += carry[2]

    def push_ch(ch):
        nonlocal x
        w = char_w(f24, ch)
        if x + w > TEXT_W and x > 0:
            if ch in NO_HEAD and x + w <= TEXT_W + MARGIN:   # ぶら下げ
                lines[-1].append((ch, None, w))
                x += w
                return
            line_break()
            if ch == ' ':              # 折り返し直後の空白は捨てる
                return
        lines[-1].append((ch, None, w))
        x += w

    for seg, yomi in units:
        if yomi:
            w = max(char_w(f24, seg[0]) * len(seg), 12 * len(yomi))
            if x + w > TEXT_W and x > 0:
                line_break()
            lines[-1].append((seg, yomi, w))
            x += w
        else:
            i = 0
            while i < len(seg):
                ch = seg[i]
                if ord(ch) > 0x7F or ch == ' ':
                    push_ch(ch)
                    i += 1
                    continue
                j = i
                w = 0
                while j < len(seg) and ord(seg[j]) <= 0x7F and seg[j] != ' ':
                    w += char_w(f24, seg[j])
                    j += 1
                if w <= TEXT_W and x + w > TEXT_W and x > 0:
                    line_break()
                while i < j:
                    push_ch(seg[i])
                    i += 1
    return lines


def draw_line(draw, f24, f12, items, y):
    """1 行を描く。y は**本文の上端**。ルビはその上の RUBY_ZONE(空き1/ルビ12/空き1)に載る。

    実測: 神楽坂12 のグリフは 'la' 原点から行 1〜12 に、日比谷24 は 0〜23 に描かれる。
    → ルビの la 原点を y-14 に置くと実描画は y-13〜y-2(本文との空き 1 が y-1 に立つ)。
    """
    x = MARGIN
    for seg, yomi, w in items:
        if yomi:
            base_w = char_w(f24, seg[0]) * len(seg)
            ruby_w = 12 * len(yomi)
            # 箱の幅 = max(親, ルビ)。余りは中央寄せ(均等割りの最簡形)
            draw.text((x + (w - base_w) // 2, y), seg, font=f24, fill=INK, anchor='la')
            draw.text((x + (w - ruby_w) // 2, y - RUBY_ZONE), yomi, font=f12, fill=INK, anchor='la')
        else:
            draw.text((x, y), seg, font=f24, fill=INK, anchor='la')
        x += w


def main():
    asset = json.loads(ASSET.read_text())
    ruby = asset['ruby']
    keys = sorted(ruby.keys(), key=len, reverse=True)
    f24 = ImageFont.truetype(str(FONT_DIR / 'KH-Dot-Hibiya-24.ttf'), 24)
    f12 = ImageFont.truetype(str(FONT_DIR / 'KH-Dot-Kagurazaka-12.ttf'), 12)

    img = Image.new('RGB', (W, H), BG)
    draw = ImageDraw.Draw(img)

    vlines = []
    for line in OPENING:
        if not line:
            vlines.append([])
            continue
        vlines.extend(wrap(segment(line, ruby, keys), f24))
    y = TOP
    first = True
    for items in vlines:
        if not items:
            y += BLANK
            first = False
            continue
        # ★日本語行は(ルビの有無によらず)常にルビ域を持つ = 行送り 38px 固定。
        #   散文の途中で行送りが揺れないため(2026-08-19 決定)。詰めるのは英字行だけ
        is_jp = any(ord(c) > 0x7F for seg, _, _ in items for c in seg)
        y += RUBY_ZONE if is_jp else (0 if first else LEAD)
        first = False
        if y + BASE > CMD_Y - 8:
            print(f'警告: 本文がコマンド行に食い込む(y={y})', file=sys.stderr)
            break
        draw_line(draw, f24, f12, items, y)
        y += BASE

    # コマンド欄: ＞ + 入力中のかな + カーソル。右端に「いま選んでいる子音」の小さな表示
    base = CMD_Y + 21
    x = MARGIN
    draw.text((x, base), '＞', font=f24, fill=ACCENT, anchor='ls')
    x += 24
    draw.text((x, base), CMD_TEXT, font=f24, fill=INK, anchor='ls')
    x += char_w(f24, CMD_TEXT[0]) * len(CMD_TEXT)
    draw.rectangle([x + 2, CMD_Y, x + 4, CMD_Y + 23], fill=ACCENT)  # カーソル
    kx = W - MARGIN - 24
    draw.rectangle([kx - 3, CMD_Y - 2, kx + 24 + 2, CMD_Y + 24 + 1], outline=DIM)
    draw.text((kx, base), 'あ', font=f24, fill=ACCENT, anchor='ls')

    # PS1 の 15bpp(RGB555)に量子化
    img = img.point(lambda v: (v >> 3) * 255 // 31)

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    img.save(OUT_DIR / 'mock-1x.png')
    img.resize((W * 3, H * 3), Image.NEAREST).save(OUT_DIR / 'mock-3x.png')
    print('OK:', OUT_DIR / 'mock-1x.png', OUT_DIR / 'mock-3x.png')


if __name__ == '__main__':
    main()
