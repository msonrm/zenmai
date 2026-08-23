#!/usr/bin/env python3
"""オプションの読み物 3 頁と画面の文言を ui_data.h に出す。

★ライセンスは画面に書き写さず、**実ファイルを読む**。ブラウザ版(web/index.html の
  #license)と同じ理由 —— 書き写すと LICENSE を直したときにここだけ古くなる。

PS1 版は `.psexe` 1 本で配るので**外部ファイルが置けない**。MIT も zlib も
「複製物に著作権表示とライセンス本文を含める」ことを条件にしているから、
全文を焼き込むのが唯一の道になる。

★頁は 3 つ(もじの うちかた / つかえる ことば / ライセンス)。**器は 1 つ**で、
  C 側は「行の並びを縦に送る」だけ。中身はここのデータでしかない。

使い方: python3 gen_ui.py
"""
from pathlib import Path

HERE = Path(__file__).parent
ROOT = HERE.parent

JA, EN, BOTH = 1, 2, 0
# ★本文幅。zm_dual_main.c の PAGE_X と**必ず同じ余白**にすること(縁取りの内側)
TEXT_W = 640 - 2 * 48
P_TYPING, P_CMDS, P_LICENSE = 0, 1, 2
PAGE_N = 3

lines = []          # (page, lang, dim, 本文)。dim = 控えめの色
cur = P_TYPING


def page(n):
    global cur
    cur = n


def put(text, lang=BOTH, dim=0):
    lines.append((cur, lang, dim, text))


def put_file(path):
    for ln in Path(path).read_text(encoding='utf-8').rstrip().splitlines():
        put(ln.rstrip(), BOTH, 1)


def rule():
    # ★罫線は ASCII で引く。同梱フォントは「使う字だけ」なので、
    #   訳文に出てこない記号(─ など)は**入っていない**(照合して分かった)
    # ★長さは本文幅から決める。固定の 48 本だと余白を変えた途端に**折り返して
    #   「---」が次行にこぼれる**(実際にこぼした)
    put('-' * (TEXT_W // 12), BOTH, 1)


# ---- 頁 1: もじの うちかた ----
# ★★ 仮の中身。動きを見るための置きもので、文言はこれから決める。
#    同梱フォントに無い字を足さないよう、ひらがな・カタカナ・英字だけで書いてある。
page(P_TYPING)
put('（ここは かりの ないよう）', JA, 1)
put('(placeholder text)', EN, 1)
put('')
put('じゅうじキー で ぎょうを えらび、', JA)
put('ボタン で だんを えらぶ。', JA)
put('The D-pad picks the row,', EN)
put('the face buttons pick the vowel.', EN)
put('')
put('START  うちおわり（かくてい）', JA)
put('SELECT  くうはく', JA)
put('R2 ながおし  けす', JA)
put('START  send the line', EN)
put('SELECT  space', EN)
put('hold R2  backspace', EN)

# ---- 頁 2: つかえる ことば ----
# ★★ 仮の中身。本番は cmd_data から作る(手で書き写すと本体とずれる)。
page(P_CMDS)
put('（ここは かりの ないよう）', JA, 1)
put('(placeholder text)', EN, 1)
put('')
put('みる  とる  おく  あける  よむ', JA)
put('きた  みなみ  ひがし  にし', JA)
put('ほぞんする  さいかいする', JA)
put('look  take  drop  open  read', EN)
put('north  south  east  west', EN)
put('save  restore', EN)

# ---- 頁 3: ライセンス ----
page(P_LICENSE)
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

# ---- 画面の文言 ----
# ★ここに出る文字列は全部「こちらのもの」。原作の文は 1 つも出ないので、
#   訳の表は通さず完成行として持つ(「その文字列は誰のものか」)。
# ★項目名はそのまま頁の見出しになる(並びは P_* と同じ)。
ITEM = [
    ('もじの うちかた', 'HOW TO TYPE'),
    ('つかえる ことば', 'COMMANDS'),
    ('ライセンス',      'LICENSE'),
]
# ★★ボタンの案内は**画面に書かない**。Start で開いたら Start で閉じる、開いた先で
#   フェイスボタンを押せば決まる —— これは当時から今まで浸透している作法なので、
#   いちいち書くと**画面がその分だけ狭くなるだけ**になる。
#   (だから決定を ○/× で入れ替える必要も無くなった = **どのフェイスボタンでも決まる**)
# ★起動の言語メニューに出す一行だけは残す。**メニューがあること**は作法ではないし、
#   物語の紙面にシステムの字は混ぜないので、全員が通るここでしか知らせられない。
BOOT = ('ゲームちゅう START で メニュー', 'PRESS START IN GAME FOR THE MENU')

# ---- 画面幅で折り返す ----
# ★専用画面で描くので、**折り返しはここで済ませておく**(C 側は行を y に並べるだけ)。
#   半角 12px / 全角 24px。★左右の余白は**縁取りの内側**にとるので、
#   zm_dual_main.c の PAGE_X と**必ず同じ値**にすること。


def width(t):
    return sum(12 if ord(c) < 0x80 else 24 for c in t)


def wrap(t):
    if width(t) <= TEXT_W:
        return [t]
    out = []
    if ' ' in t.strip():                      # 英文は語で折る
        cur_ = ''
        for w in t.split(' '):
            cand = (cur_ + ' ' + w) if cur_ else w
            if width(cand) > TEXT_W and cur_:
                out.append(cur_)
                cur_ = w
            else:
                cur_ = cand
        if cur_:
            out.append(cur_)
        return out
    cur_ = ''                                 # 日本語は字で折る
    for c in t:
        if width(cur_ + c) > TEXT_W:
            out.append(cur_)
            cur_ = c
        else:
            cur_ += c
    if cur_:
        out.append(cur_)
    return out


wrapped = []
for pg, lang, dim, text in lines:
    for part in wrap(text):
        wrapped.append((pg, lang, dim, part))
lines = wrapped

pool = []
recs = []
for pg, lang, dim, text in lines:
    off = len(pool)
    pool.extend(ord(c) for c in text)
    recs.append((off, len(text), dim, lang, pg))

out = ['/* gen_ui.py が生成。手で編集しない */',
       '/* オプションの読み物 3 頁 + 画面の文言。',
       '   ★ライセンス全文は実ファイルから作っている(書き写していない) */',
       '',
       'typedef struct { unsigned int off; unsigned short len; unsigned char dim;',
       '                 unsigned char lang; unsigned char page; } UiLine;',
       'typedef struct { const unsigned short *s; unsigned short n; } UiStr;',
       '',
       'static const unsigned short UI_POOL[%d] = {' % max(len(pool), 1)]
for i in range(0, len(pool), 16):
    out.append('    ' + ', '.join('0x%04X' % c for c in pool[i:i + 16]) + ',')
if not pool:
    out.append('    0,')
out.append('};')
out.append('')
out.append('static const UiLine UI_LINES[%d] = {' % len(recs))
for off, ln, dim, lang, pg in recs:
    out.append('    { %d, %d, %d, %d, %d },' % (off, ln, dim, lang, pg))
out.append('};')
out.append('')
out.append('#define UI_LINE_N %d' % len(recs))
out.append('#define UI_PAGE_N %d' % PAGE_N)
out.append('')
out.append('/* ---- 画面の文言(添字 = lang_en) ---- */')

strs = []


def sref(text):
    """文字列を UI_Sn として吐き、UiStr の初期化子を返す"""
    i = len(strs)
    strs.append((i, text))
    return '{ UI_S%d, %d }' % (i, len(text))


items = [[sref(ja if s == 0 else en) for ja, en in ITEM] for s in (0, 1)]
boots = [sref(BOOT[0]), sref(BOOT[1])]

for i, text in strs:
    out.append('static const unsigned short UI_S%d[%d] = { %s };'
               % (i, len(text), ', '.join('0x%04X' % ord(c) for c in text)))
out.append('')
out.append('static const UiStr UI_ITEM[2][UI_PAGE_N] = {')
for row in items:
    out.append('    { ' + ', '.join(row) + ' },')
out.append('};')
out.append('static const UiStr UI_BOOT[2] = { %s, %s };' % (boots[0], boots[1]))
out.append('')
(HERE / 'ui_data.h').write_text('\n'.join(out))

used = set()
for _, _, _, t in lines:
    used.update(t)
for ja, en in ITEM + [BOOT]:
    used.update(ja)
    used.update(en)
(HERE / 'ui_chars.txt').write_text(''.join(sorted(used)), encoding='utf-8')

per = [sum(1 for r in recs if r[4] == p) for p in range(PAGE_N)]
print('ui_data.h: %d 行 (頁ごと %s) / %d 字' % (len(recs), per, len(pool)))
