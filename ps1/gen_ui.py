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
import re
from pathlib import Path

HERE = Path(__file__).parent
ROOT = HERE.parent

JA, EN, BOTH = 1, 2, 0
# ★同梱フォントに入っている字。**ここに無い字は空白で描かれる**ので、
#   一覧に出す表記はこれで濾す（glyphs.h は「使う字だけ」を焼いてある）
FONT = set(int(m, 16) for m in
           re.findall(r'\{0x([0-9A-Fa-f]{4}),', (Path(__file__).parent / 'glyphs.h').read_text()))
# ★本文幅。zm_dual_main.c の PAGE_X と**必ず同じ余白**にすること(縁取りの内側)
TEXT_W = 640 - 2 * 48


def width(t):
    return sum(12 if ord(c) < 0x80 else 24 for c in t)


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
# ★本文は持たない。**コントローラの図**そのものが説明になる（web 版と同じ考え方）。
#   図の札は入力表 GP_KANA / GP_ENG から引くので、ここには**固定の札だけ**を置く。
# ---- 頁 2: つかえる ことば ----
# ★ここは**システムの言葉**（進行そのものへの指示）。物を取る・扉を開けるといった
#   ゲームの中の行動は含めない —— web 版の設定ダイアログ「システムの言葉」と同じ範囲。
# ★一覧は**書き写さない**。役目の並びは web/main.js の SYS を読み、打つ言葉は
#   asset（zork1-cmd.json）と gen_cmd.py の PARSER_WORDS から引く。
#   写すと語彙を足し引きしたときに手引きだけ古くなり、**打てない言葉を案内する**
#   （web 版が一度そうなった、とコメントに残っている）。
import json as _json
import re as _re
import gen_cmd as _gc                      # PARSER_WORDS（PS1 側の写しはこの 1 本だけ）

_asset = _json.loads((ROOT / 'assets' / 'zork1-cmd.json').read_text())
_web = (ROOT / 'web' / 'main.js').read_text()
_blk = _web[_web.index('const SYS = ['):]
_blk = _blk[:_blk.index('\n  ]')]
SYS = _re.findall(r"\['([^']+)', '([^']+)'\]", _blk)
assert SYS, '★web/main.js の SYS を読めなかった（形が変わった？）'

# ★同梱フォントに無い字を含む役目名の言い換え。web 版の言い方をそのまま出したいが、
#   PS1 のフォントは「使う字だけ」なので、無い字は**空白で描かれてしまう**。
#   （「描写」の「写」が入っていなかった。下の照合が拾う）
RELABEL = {
    'VERBOSE': '部屋の説明を長く',
    'BRIEF': '部屋の説明を短く',
    'SUPER': '部屋の説明をごく短く',
}

# 英語の言い添え。★これはこちらの文（原作にも web にも無い）
EN_NOTE = {
    'SCORE': 'your points so far', 'SAVE': 'store the game',
    'RESTORE': 'continue a stored game', 'RESTART': 'start over',
    'QUIT': 'stop playing', 'VERSION': 'which release this is',
    'DIAGNOSE': 'how badly you are hurt', 'VERBOSE': 'full room descriptions',
    'BRIEF': 'short descriptions', 'SUPER': 'shortest descriptions',
    '@again': 'repeat the last command',
}


def _kana(words, n=2):
    """打つのはかなだけなので、かなの形を出す（web 版の pickKana と同じ）"""
    hira = [w for w in words if _re.fullmatch(r'[ぁ-んー]+', w)]
    kata = [w for w in words if _re.fullmatch(r'[ァ-ヶー]+', w)]
    return (hira or kata)[:n]


page(P_CMDS)
put('ゲームの中の行動ではなく、', JA, 1)
put('進行そのものへの指示。', JA, 1)
put('Not actions in the world --', EN, 1)
put('instructions to the game itself.', EN, 1)
put('')

_rows = []
for _label, _key in SYS:
    _w = _kana(list(_gc.PARSER_WORDS)) if _key == '@again' \
        else _kana(_asset['verbs'].get(_key, {}).get('ja', []))
    assert _w, '★打つ言葉が引けなかった: ' + _key
    _rows.append((RELABEL.get(_key, _label), _w, _key))

_lw = max(width(r[0]) for r in _rows)      # 役目の欄はいちばん長いものに揃える
for _label, _w, _key in _rows:
    _pad = ' ' * ((_lw - width(_label)) // 12 + 2)
    _line = _label + _pad + ' / '.join(_w)
    if width(_line) > TEXT_W and len(_w) > 1:   # 入らなければ別の言い方は落とす
        _line = _label + _pad + _w[0]
    put(_line, JA)

put('', JA)
_ew = max(len(k.lower().lstrip('@')) for _, k in SYS)
for _label, _key in SYS:
    _cmd = _key.lower().lstrip('@')
    put(_cmd + ' ' * (_ew - len(_cmd) + 2) + EN_NOTE[_key], EN)

# ---- 頁 3: ライセンス ----
page(P_LICENSE)
rule()
put('Zenmai —— このソフト', JA)
put('Zenmai -- this software', EN)
rule()
put_file(ROOT / 'LICENSE')
put('')

rule()
put('Zork I —— オリジナル', JA)
put('Zork I -- the original', EN)
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
# ★正式名称は「KH ドットフォント」だが、**小さい「ォ」が同梱フォントに無い**。
#   「使用フォント」も同じ理由で書けないので、いまは「使用書体」と出す。
#   （下の WANT に ォ を入れてあるので、glyphs.h を作り直せば両方書き換えられる）
put('KH Dot Font —— 使用書体', JA)
put('KH Dot Font -- the font used', EN)
rule()
put('KH-Dot-Hibiya-24 / KH-Dot-Kagurazaka-12', BOTH, 1)
put('Copyright (c) 1990-2015 Keitarou Hiraki and Font Silo', BOTH, 1)
put('SIL Open Font License 1.1', BOTH, 1)
put('http://jikasei.me/font/kh-dotfont/', BOTH, 1)
put('')

put('Zork という名称は商標です。', JA)
put('Zork is a trademark.', EN)

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
# ★「もじの うちかた」の図の固定札。★動く札（十字・面ボタン・R1）は**入力表から引く**ので
#   ここには無い —— 図に出ている字と実際に入る字が違う、が最悪の事故（web 版の教訓）。
# ★英語面は**かなの行/段ではない** —— プッシュホン式(T9)で、十字がキー・面ボタンが
#   そのキーの何文字目か、を選ぶ。だから ROW / VOWEL は直訳の誤りになる。
# ★SELECT は**言語で役が違う**（日本語 = 濁点/半濁点のトグル、英語 = 空白）。
#   札は実際の役をそのまま名指しする（「くうはく」と書いていたのは誤り）。
HELP = [
    ('L1 はまやらわ',    'L1 KEYS 6-0'),
    ('L2 ようおん',      'L2 SHIFT'),
    ('R2 ん を',         'R2 0'),
    ('SELECT てん まる', 'SELECT SPACE'),
    # ★START だけは「字の打ち方」ではないので括弧に入れて、別の種類だと示す
    ('（START かくてい）', '(START SEND)'),
    # ★この一行が**左右の役をすでに言っている**ので、群の見出しは持たない。
    #   ひらがなだけだと入りきらないので漢字を交ぜる
    ('左手で行をえらび 右手で文字をうつ', 'LEFT HAND PICKS THE KEY, RIGHT HAND TYPES'),
    # ★っ は日本語だけ（英語面の L2 はシフト）
    ('R2 ながおしでけす   L2 と R2 で っ', 'HOLD R2 TO DELETE'),
]

# ---- 画面幅で折り返す ----
# ★専用画面で描くので、**折り返しはここで済ませておく**(C 側は行を y に並べるだけ)。
#   半角 12px / 全角 24px。★左右の余白は**縁取りの内側**にとるので、
#   zm_dual_main.c の PAGE_X と**必ず同じ値**にすること。


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


# ★頁の末尾の空行は落とす。**画面に収まっているのに 1 行だけ動く**（十字キーの上下で
#   カタッと下がる）のは、末尾の空行が行数に入っていたせい。
# ★落とす判定は**頁 × 言語ごと**にやる —— 画面に出るのはその組み合わせで濾したあとの
#   並びなので、「全体の末尾」だけ見ても足りない（日本語の表と英語の表の境目に置いた
#   空行が、日本語から見ると末尾になっていた）。
_drop = set()
for _pg in range(PAGE_N):
    for _view in (JA, EN):
        _idx = [i for i, ln in enumerate(lines)
                if ln[0] == _pg and ln[1] in (BOTH, _view)]
        while _idx and not lines[_idx[-1]][3]:
            _drop.add(_idx.pop())
lines = [ln for i, ln in enumerate(lines) if i not in _drop]

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
helps = [[sref(ja if s == 0 else en) for ja, en in HELP] for s in (0, 1)]
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
out.append('/* 図の固定札: L1 / L2 / R2 / SELECT / START */')
out.append('#define UI_HELP_N %d' % len(HELP))
out.append('static const UiStr UI_HELP[2][UI_HELP_N] = {')
for row in helps:
    out.append('    { ' + ', '.join(row) + ' },')
out.append('};')
out.append('')
(HERE / 'ui_data.h').write_text('\n'.join(out))

used = set()
for _, _, _, t in lines:
    used.update(t)
for ja, en in ITEM + HELP + [BOOT]:
    used.update(ja)
    used.update(en)
# ★いまは出せないが**次に glyphs.h を作り直すときは入れておきたい字**。
#   ここに書いておけば gen_data.py が拾う（フォントのある環境で作り直したら、
#   「KH ドットフォント」と正式名称で書けるようになる）
WANT = 'ォ'
(HERE / 'ui_chars.txt').write_text(''.join(sorted(set(used) | set(WANT))), encoding='utf-8')

# ★**出す字は全部フォントに入っていなければならない**。glyphs.h は「使う字だけ」を
#   焼いてあり、無い字は**黙って空白で描かれる**。ここで止める（実際に「写」で踏んだ）。
_missing = sorted(c for c in used if ord(c) not in FONT)   # WANT は出さないので除く
if _missing:
    raise SystemExit('★同梱フォントに無い字を出そうとしている: ' + ''.join(_missing)
                     + '\n  言い換えるか、フォントのある環境で glyphs.h を作り直すこと')

per = [sum(1 for r in recs if r[4] == p) for p in range(PAGE_N)]
print('ui_data.h: %d 行 (頁ごと %s) / %d 字' % (len(recs), per, len(pool)))
