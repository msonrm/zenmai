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
    # ★罫線は ASCII で引く。同梱フォントは「使う字だけ」なので、
    #   訳文に出てこない記号(─ など)は**入っていない**(照合して分かった)
    put('-' * 48, BOTH, 1)


put('ライセンスと出どころ', JA)
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
put('KH Dot Font —— 画面の字', JA)
put('KH Dot Font -- the glyphs on screen', EN)
rule()
put('KH-Dot-Hibiya-24 / KH-Dot-Kagurazaka-12', BOTH, 1)
put('Copyright (c) 1990-2015 Keitarou Hiraki and Font Silo', BOTH, 1)
put('SIL Open Font License 1.1', BOTH, 1)
put('http://jikasei.me/font/kh-dotfont/', BOTH, 1)
put('')

put('作品名は商標であり、上の許諾には含まれない。', JA)
put('ここでは名前・ロゴに作品名を使わない。', JA)
put('Game titles are trademarks and are not covered above;', EN)
put('this project does not use them in its name or logo.', EN)

# ---- オプション画面の文言 ----
# ★ここに出る文字列は全部「こちらのもの」。原作の文は 1 つも出ないので、
#   訳の表は通さず完成行として持つ(「その文字列は誰のものか」)。
UI = [
    ('TITLE',   'せってい',   'OPTIONS'),
    ('LICENSE', 'ライセンス', 'LICENSE'),
    ('CLOSE',   'とじる',     'CLOSE'),
    ('HINT',    'O きめる   X とじる', 'O SELECT   X CLOSE'),
    # ★矢印(↑↓)も同梱フォントに無い。ひらがなで書く
    ('SCROLL',  'じょうげで おくる   X もどる', 'UP/DOWN SCROLL   X BACK'),
]

# ---- 画面幅で折り返す ----
# ★専用画面で描くので、**折り返しはここで済ませておく**(C 側は行を y に並べるだけ)。
#   半角 12px / 全角 24px、本文幅は W - 2*MARGIN = 576px。
TEXT_W = 640 - 2 * 32


def width(t):
    return sum(12 if ord(c) < 0x80 else 24 for c in t)


def wrap(t):
    if width(t) <= TEXT_W:
        return [t]
    out = []
    if ' ' in t.strip():                      # 英文は語で折る
        cur = ''
        for w in t.split(' '):
            cand = (cur + ' ' + w) if cur else w
            if width(cand) > TEXT_W and cur:
                out.append(cur)
                cur = w
            else:
                cur = cand
        if cur:
            out.append(cur)
        return out
    cur = ''                                  # 日本語は字で折る
    for c in t:
        if width(cur + c) > TEXT_W:
            out.append(cur)
            cur = c
        else:
            cur += c
    if cur:
        out.append(cur)
    return out


wrapped = []
for lang, dim, text in lines:
    for part in wrap(text):
        wrapped.append((lang, dim, part))
lines = wrapped

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

used = set()
for _, _, t in lines:
    used.update(t)
for _, ja, en in UI:
    used.update(ja)
    used.update(en)
(HERE / 'ui_chars.txt').write_text(''.join(sorted(used)), encoding='utf-8')

longest = max((len(t) for _, _, t in lines), default=0)
print('ui_data.h: ライセンス %d 行 / %d 字 / 最長 %d 字' % (len(recs), len(pool), longest))
