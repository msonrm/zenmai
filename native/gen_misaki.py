#!/usr/bin/env python3
"""ふりがなの字形（美咲ゴシック 7×7・8×8 の枠）→ misaki_data.{c,h}（手で編集しない）。

★BDF は追跡していない（取ってくる素材。出どころと許諾 = vendor/misaki/README.md）。
  生成した表の方を追跡するので、BDF が無くても本体は建つ（build-pc98.sh は BDF があるときだけ焼き直す）。
★入れる字 = ひらがな・カタカナの全部 + 訳の辞書のふりがなに現れる字（ー など）。
  ★ふりがなに現れる字が 1 つでも BDF に無ければ止まる。

字形の置き方は試作（pc98-mock/layout.py の ruby_glyph）と同じ: 8×8 の枠で、ベースラインは上から 6。
1 字 = 8 バイト（上の行から、bit7 が左端）。

    python3 gen_misaki.py            # BDF = ../pc98-mock/misaki_gothic.bdf（MISAKI_BDF で変えられる）
"""
import json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
BDF = os.environ.get('MISAKI_BDF', os.path.join(HERE, '..', 'pc98-mock', 'misaki_gothic.bdf'))


def load_bdf(path):
    glyphs, cur = {}, None
    with open(path, encoding='latin1') as f:
        for line in f:
            t = line.split()
            if not t:
                continue
            if t[0] == 'ENCODING':
                cur = {'enc': int(t[1])}
            elif t[0] == 'BBX':
                cur['bbx'] = tuple(map(int, t[1:5]))
            elif t[0] == 'BITMAP':
                cur['rows'] = []
            elif t[0] == 'ENDCHAR':
                glyphs[cur['enc']] = cur
                cur = None
            elif cur is not None and 'rows' in cur:
                cur['rows'].append(int(t[0], 16))
    return glyphs


def glyph8(g):
    w, h, xo, yo = g['bbx']
    top = 6 - (h + yo)
    rows = [0] * 8
    for r, bits in enumerate(g['rows']):
        for x in range(w):
            if (bits >> (7 - x)) & 1:
                rows[top + r] |= 0x80 >> (xo + x)
    return rows


def main():
    if not os.path.exists(BDF):
        sys.exit(f'美咲ゴシックの BDF が無い: {BDF}（vendor/misaki/README.md）')
    glyphs = load_bdf(BDF)
    ruby = json.load(open(os.path.join(HERE, '..', 'assets', 'zork1-ja.json'), encoding='utf-8'))['ruby']
    need = {ord(c) for v in ruby.values() for _, y in v if y for c in y}
    want = need | set(range(0x3041, 0x3097)) | set(range(0x30A1, 0x30F7)) | {0x30FC}
    miss = sorted(u for u in need if u not in glyphs)
    if miss:
        sys.exit('ふりがなの字が BDF に無い: ' + ' '.join(chr(u) for u in miss))
    codes = sorted(u for u in want if u in glyphs)
    with open(os.path.join(HERE, 'misaki_data.c'), 'w', encoding='utf-8') as f:
        f.write('/* gen_misaki.py が生成。手で編集しない。\n'
                ' * ★字形は美咲ゴシック（Copyright(C) 2002-2021 Num Kadoma）。出どころと許諾は vendor/misaki/。 */\n'
                '#include "misaki_data.h"\n\nconst unsigned short misaki_code[MISAKI_N] = {\n')
        for i in range(0, len(codes), 12):
            f.write('    ' + ', '.join(f'0x{u:04X}' for u in codes[i:i + 12]) + ',\n')
        f.write('};\n\nconst unsigned char misaki_glyph[MISAKI_N][8] = {\n')
        for u in codes:
            f.write('    { ' + ', '.join(f'0x{b:02X}' for b in glyph8(glyphs[u])) + ' },   /* ' + chr(u) + ' */\n')
        f.write('};\n')
    with open(os.path.join(HERE, 'misaki_data.h'), 'w', encoding='utf-8') as f:
        f.write('/* gen_misaki.py が生成。手で編集しない */\n#ifndef MISAKI_DATA_H\n#define MISAKI_DATA_H\n'
                f'enum {{ MISAKI_N = {len(codes)} }};\n'
                'extern const unsigned short misaki_code[MISAKI_N];     /* Unicode 順 */\n'
                'extern const unsigned char misaki_glyph[MISAKI_N][8];  /* 上の行から・bit7 が左端 */\n#endif\n')
    print(f'misaki_data.c: {len(codes)} 字（ふりがなに現れる {len(need)} 字を含む）')


if __name__ == '__main__':
    main()
