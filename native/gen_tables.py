#!/usr/bin/env python3
"""labo の gamepad-kana-table.ts から入力テーブルを抽出して tables.h を生成。

手写しの転記事故を避けるため、正典(TS)を直接パースする。
使い方: python3 gen_tables.py <gamepad-kana-table.ts のパス>
"""
import re
import sys
from pathlib import Path

src = Path(sys.argv[1]).read_text()


def block(name):
    m = re.search(name + r'[^=]*=[^[(]*[\[(]', src)
    depth, i = 1, m.end()
    while depth:
        if src[i] in '[(':
            depth += 1
        elif src[i] in '])':
            depth -= 1
        i += 1
    return src[m.end():i - 1]


def table10x5(name):
    rows = re.findall(r'\[((?:\s*"[^"]*",?\s*)+)\]', block(name))
    out = []
    for r in rows:
        cells = re.findall(r'"([^"]*)"', r)
        assert len(cells) == 5, cells
        out.append(cells)
    assert len(out) == 10, name
    return out


def pairs(name):
    return re.findall(r'\["([^"]+)",\s*"([^"]+)"\]', block(name))


kana = table10x5('KANA_TABLE')
# ★PS1 適応: や行の「」と わ行の「？」は空枠にする(コマンド入力に記号は不要で、
#   や/ゆ/よ・わ/を を狙って隣に触れると意図せず入る。実機フィードバック 2026-08-19)。
#   空枠は英語表の空セルと同じ扱い(machine は ch==0 で無反応・eager も立たない)
assert kana[7][1] == '「' and kana[7][3] == '」' and kana[9][2] == '？'
kana[7][1] = kana[7][3] = ''
# ★PS1 適応: わ行は**フリック入力と同じ並び**にする(実機フィードバック 2026-08-24)。
#   中央=わ / 左=を / 上=ん / 右=ー —— 段の並びは 中央→あ段・左→い段・上→う段・右→え段
#   なので、そのまま わ・を・ん・ー と入る。ゐ / ゑ は落とす(原作に出てこない)。
#   ん と を は R2 でも出せるが、**同じ字に道が 2 本あっても害はない**。揃うほうが強い。
assert kana[9] == ['わ', 'ゐ', '？', 'ゑ', 'を']
kana[9] = ['わ', 'を', 'ん', 'ー', '']
eng = table10x5('ENGLISH_TABLE')
youon = pairs('YOUON_POSTSHIFT_MAP')
daku = pairs('DAKUTEN_MAP')
handaku = pairs('HANDAKUTEN_MAP')
# ★PS1 適応: う→ゔ を落とす。**KH ドットフォントに ゔ(U+3094) が無い**ので、
#   打てても黒い四角(.notdef)が出るだけだった(2026-08-25 に実測して発覚)。
#   訳文にも語彙にも ゔ / ヴ は 1 件も出てこないので、打てなくても失うものは無い。
#   ★合成(う + 濁点)も試したが駄目 —— この明朝は濁点を足すとき**清音の側も描き直す**
#     ので、「が − か」で濁点だけを取り出すことができない。
assert ('う', 'ゔ') in daku
daku = [p for p in daku if p != ('う', 'ゔ')]


def u(c):
    return f'0x{ord(c):04X}'


with open(Path(__file__).parent / 'tables.h', 'w') as f:
    f.write('/* gen_tables.py が gamepad-kana-table.ts から生成。手で編集しない */\n')
    f.write('static const unsigned short GP_KANA[10][5] = {\n')
    for r in kana:
        f.write('  {' + ','.join(u(c) if c else '0' for c in r) + '},\n')
    f.write('};\n')
    f.write('static const unsigned short GP_ENG[10][5] = {\n')
    for r in eng:
        f.write('  {' + ','.join(u(c) if c else '0' for c in r) + '},\n')
    f.write('};\n')
    f.write('typedef struct { unsigned short from; unsigned short to[2]; } GpYouon;\n')
    f.write(f'enum {{ GP_YOUON_N = {len(youon)}, GP_DAKU_N = {len(daku)}, GP_HANDAKU_N = {len(handaku)} }};\n')
    f.write('static const GpYouon GP_YOUON[GP_YOUON_N] = {\n')
    for a, b in youon:
        to = [u(c) for c in b] + (['0'] if len(b) == 1 else [])
        f.write(f'  {{{u(a)}, {{{",".join(to)}}}}},\n')
    f.write('};\n')
    f.write('typedef struct { unsigned short sei, on; } GpAB;\n')
    f.write('static const GpAB GP_DAKU[GP_DAKU_N] = {\n')
    for a, b in daku:
        f.write(f'  {{{u(a)}, {u(b)}}},\n')
    f.write('};\n')
    f.write('static const GpAB GP_HANDAKU[GP_HANDAKU_N] = {\n')
    for a, b in handaku:
        f.write(f'  {{{u(a)}, {u(b)}}},\n')
    f.write('};\n')

print(f'tables.h: kana 10x5 / eng 10x5 / youon {len(youon)} / daku {len(daku)} / handaku {len(handaku)}')
