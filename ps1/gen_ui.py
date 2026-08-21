#!/usr/bin/env python3
"""オプション画面の文言と、同梱物のライセンス全文を ui_data.h に出す。

★画面に書き写さず、**実ファイルを読む**。ブラウザ版(web/index.html の #license)と
同じ理由 —— 書き写すと LICENSE を直したときにここだけ古くなる。

PS1 版は `.psexe` 1 本で配るので**外部ファイルが置けない**。MIT も zlib も
「複製物に著作権表示とライセンス本文を含める」ことを条件にしているから、
全文を焼き込むのが唯一の道になる。

使い方: python3 gen_ui.py
"""
from pathlib import Path

HERE = Path(__file__).parent
ROOT = HERE.parent

JA, EN, BOTH = 1, 2, 0

# (lang, dim, 本文) の並び。dim = 控えめの色(ライセンス全文)
lines = []


def put(text, lang=BOTH, dim=0):
    lines.append((lang, dim, text))


def put_file(path):
    for ln in Path(path).read_text(encoding='utf-8').rstrip().splitlines():
        put(ln.rstrip(), BOTH, 1)


def rule():
    put('────────────────', BOTH, 1)


put('ライセンスと出典', JA)
put('LICENSE & CREDITS', EN)
put('')
put('このソフトは自由に使えるものだけで組み立ててある。', JA)
put('Built only from freely licensed parts.', EN)
put('')

rule()
put('Zenmai —— このソフト', JA)
put('Zenmai -- this software', EN)
rule()
put_file(ROOT / 'LICENSE')
put('')

rule()
put('Zork I —— 動かしている物語（story file）', JA)
put('Zork I -- the story file being run', EN)
put('https://github.com/historicalsource/zork1', BOTH, 1)
rule()
put_file(ROOT / 'vendor/zork1/LICENSE')
put('')

rule()
put('MojoZork —— Z-machine の実装', JA)
put('MojoZork -- the Z-machine implementation', EN)
put('https://github.com/icculus/mojozork', BOTH, 1)
rule()
put_file(HERE / 'vendor/LICENSE.txt')
put('')

rule()
put('KH ドットフォント —— 画面の字', JA)
put('KH Dot Font -- the glyphs on screen', EN)
rule()
put('24 日比谷 / 12 神楽坂', BOTH, 1)
put('Copyright (c) 1990-2015 Keitarou Hiraki and Font Silo', BOTH, 1)
put('SIL Open Font License 1.1', BOTH, 1)
put('http://jikasei.me/font/kh-dotfont/', BOTH, 1)
put('')

put('作品名は商標であり、上の許諾には含まれない。', JA)
put('この企画は名前・ロゴに作品名を使わない。', JA)
put('Game titles are trademarks and are not covered above;', EN)
put('this project does not use them in its name or logo.', EN)

# ---- オプション画面の文言 ----
# ★ここに出る文字列は全部「こちらのもの」。原作の文は 1 つも出ないので、
#   訳の表は通さず完成行として持つ(「その文字列は誰のものか」)。
UI = [
    ('TITLE',   'せってい',   'OPTIONS'),
    ('LICENSE', 'ライセンス', 'LICENSE'),
    ('CLOSE',   'とじる',     'CLOSE'),
    ('HINT',    '○ きめる  × とじる', 'O SELECT   X CLOSE'),
]

pool = []
recs = []
for lang, dim, text in lines:
    off = len(pool)
    pool.extend(ord(c) for c in text)
    recs.append((off, len(text), dim, lang))

out = ['/* gen_ui.py が生成。手で編集しない */',
       '/* 同梱物のライセンス全文と出典。★実ファイルから作っている(書き写していない) */',
       '',
       'typedef struct { unsigned int off; unsigned short len;',
       '                 unsigned char dim; unsigned char lang; } LicLine;',
       '',
       'static const unsigned short LIC_POOL[%d] = {' % max(len(pool), 1)]
for i in range(0, len(pool), 16):
    out.append('    ' + ', '.join('0x%04X' % c for c in pool[i:i + 16]) + ',')
if not pool:
    out.append('    0,')
out.append('};')
out.append('')
out.append('static const LicLine LIC_LINES[%d] = {' % len(recs))
for off, ln, dim, lang in recs:
    out.append('    { %d, %d, %d, %d },' % (off, ln, dim, lang))
out.append('};')
out.append('')
out.append('#define LIC_N %d' % len(recs))
out.append('')
out.append('/* ---- オプション画面の文言(日本語 / 英語) ---- */')
for name, ja, en in UI:
    for suf, text in (('JA', ja), ('EN', en)):
        out.append('static const unsigned short UI_%s_%s[%d] = { %s };'
                   % (name, suf, len(text), ', '.join('0x%04X' % ord(c) for c in text)))
        out.append('#define UI_%s_%s_N %d' % (name, suf, len(text)))
out.append('')
(HERE / 'ui_data.h').write_text('\n'.join(out))

longest = max((len(t) for _, _, t in lines), default=0)
print('ui_data.h: ライセンス %d 行 / %d 字 / 最長 %d 字' % (len(recs), len(pool), longest))
