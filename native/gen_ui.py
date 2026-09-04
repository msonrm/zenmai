#!/usr/bin/env python3
"""オプションの読み物 3 頁と画面の文言を ui_data.h に出す。

★ライセンスは画面に書き写さず、**実ファイルを読む**。ブラウザ版(web/index.html の
  #license)と同じ理由 —— 書き写すと LICENSE を直したときにここだけ古くなる。

PS1 版は `.psexe` 1 本で配るので**外部ファイルが置けない**。MIT も zlib も
「複製物に著作権表示とライセンス本文を含める」ことを条件にしているから、
全文を焼き込むのが唯一の道になる。

★頁は 3 つ(ひらがな入力方法 / システムコマンド / ライセンス)。**器は 1 つ**で、
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
# ★本文幅。main.c の PAGE_X と**必ず同じ余白**にすること(縁取りの内側)
TEXT_W = 640 - 2 * 48


# ★二段組の欄の境。**描くものではない**（C 側がこれを見て右の桁へ飛ぶ）。
#   空白を並べて揃えるのをやめた理由は下の `_rows` の注記を見ること。
COL = '\t'


def width(t):
    """等幅を仮定した見積り幅。★**これは目安でしかない** —— FreeType 版の送り幅は
    字ごとに違う。折り返すか / 別の言い方を落とすか、の判断にだけ使う。"""
    return sum(0 if c == COL else (12 if ord(c) < 0x80 else 24) for c in t)


P_TYPING, P_CMDS, P_LICENSE = 0, 1, 2
PAGE_N = 3

# ★どのフォントのビルドで出すか。lang とまったく同じ仕組みの出し分け。
#   0 = 常に出す / 1 = 焼いたビットマップ版（PS1）だけ / 2 = FreeType 版（PortMaster）だけ
F_ANY, F_BAKED, F_FT = 0, 1, 2

lines = []          # (page, lang, font, dim, 本文, 罫線か)。dim = 控えめの色
cur = P_TYPING


def page(n):
    global cur
    cur = n


def put(text, lang=BOTH, dim=0, font=F_ANY, rule=0):
    lines.append((cur, lang, font, dim, text, rule))


def put_file(path, font=F_ANY, skip=0):
    ls = Path(path).read_text(encoding='utf-8').rstrip().splitlines()
    for ln in ls[skip:]:
        put(ln.rstrip(), BOTH, 1, font)


def rule():
    # ★罫線は**字を並べない**。以前は ─(U+2500) を「本文幅 ÷ 24」本だけ並べていたが、
    #   それは**等幅を前提にした本数**で、送り幅が字ごとに違う FreeType 版（PortMaster）
    #   では 22px × 22 本 = 484px にしかならず、**本文幅 544px に 60px 届かなかった**
    #   （実機の指摘・2026-08-30）。
    # ★本数を直せば直る話ではない —— 掛ける相手がフォントで変わるので、
    #   **字で引く限りどのビルドでも端は揃わない**。長さを指定して画素で引く。
    # ★印だけを置き、実際の長さは C 側が本文幅（PAGE_X の内側）から決める。
    put('─', BOTH, 1, F_ANY, 1)


# ---- 頁 1: ひらがな入力方法 ----
# ★本文は持たない。**コントローラの図**そのものが説明になる（web 版と同じ考え方）。
#   図の札は入力表 GP_KANA / GP_ENG から引くので、ここには**固定の札だけ**を置く。
# ---- 頁 2: システムコマンド ----
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

# ★役目名は web 版（`SYS`）の言い方をそのまま出す。**言い換えは持たない**。
#   以前は「描写」の「写」が同梱フォントに無くて「部屋の説明を長く」等に言い換えて
#   いたが、glyphs.h を焼き直して字が入ったので畳んだ。
#   （それでも足りない字が出たら、下の照合が生成を止める）

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
# ★前置きは書かない。見出しが「システムコマンド」なら、それが**進行への指示**で
#   あって物語の中の行動ではないことは通じる（書くと画面がその分だけ狭くなる）。
_rows = []
for _label, _key in SYS:
    _w = _kana(list(_gc.PARSER_WORDS)) if _key == '@again' \
        else _kana(_asset['verbs'].get(_key, {}).get('ja', []))
    assert _w, '★打つ言葉が引けなかった: ' + _key
    _rows.append((_label, _w, _key))

# ★桁を**空白で揃えない**。以前は「いちばん長い役目との差 ÷ 12」だけ半角空白を
#   並べていたが、それは**半角 = 全角の半分**という等幅の仮定に立っていて、
#   同梱フォントの実測では**全角 22px に対して半角空白は 5px**（1/4 以下）なので、
#   右の欄が**行ごとにばらばらの位置から始まっていた**（実機の指摘・2026-08-30）。
# ★欄の境（COL）だけを置き、桁の位置は **C 側が glyph_w で実測して決める**。
#   ここが知っているのは「どこで欄が変わるか」だけで、幅は知らないでよい。
_lw = max(width(r[0]) for r in _rows)      # 見積り（入るかどうかの判断にだけ使う）
for _label, _w, _key in _rows:
    _right = ' / '.join(_w)
    if _lw + 24 + width(_right) > TEXT_W and len(_w) > 1:   # 入らなければ別の言い方は落とす
        _right = _w[0]
    put(_label + COL + _right, JA)

put('', JA)
for _label, _key in SYS:
    put(_key.lower().lstrip('@') + COL + EN_NOTE[_key], EN)

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
# ★**どのフォントを使っているかはビルドで違う**ので、頭の帰属だけを出し分ける。
#   焼いたビットマップ版（PS1）= KH ドットフォント / FreeType 版（PortMaster）= Noto。
#   ★使っていないフォントの名を出すのは嘘なので、両方は出さない（実機の指摘・2026-08-30）。
#   OFL 1.1 の本文はどちらも同じなので、そこは共有する。
#
# ★正式名称は「KHドットフォント」（頒布元 http://jikasei.me/font/kh-dotfont/ の表記）。
put('KHドットフォント —— 使用フォント', JA, 0, F_BAKED)
put('KH Dot Font -- the font used', EN, 0, F_BAKED)
put('http://jikasei.me/font/kh-dotfont/', BOTH, 1, F_BAKED)

put('Noto Sans CJK JP —— 使用フォント', JA, 0, F_FT)
put('Noto Sans CJK JP -- the font used', EN, 0, F_FT)
put('https://github.com/notofonts/noto-cjk', BOTH, 1, F_FT)
rule()
# ★著作権表示は**フォント自身が名乗る文字列**をそのまま出す（name table の nameID 0 / 7）。
#   OFL 条件 2 が言う「上記の著作権表示」はこれのこと。語順を整えたりしない。
put('KH-Dot-Hibiya-24 / KH-Dot-Kagurazaka-12', BOTH, 1, F_BAKED)
put('Copyright (c) Keitarou Hiraki, Font Silo. 1990-2015', BOTH, 1, F_BAKED)

put('Noto Sans CJK JP (subset)', BOTH, 1, F_FT)
put('Copyright 2014-2021 Adobe (http://www.adobe.com/).', BOTH, 1, F_FT)
put('Noto is a trademark of Google Inc.', BOTH, 1, F_FT)
put('')
# ★**OFL も全文を焼く。** MIT と zlib と同じ理屈 —— OFL 1.1 の条件 2 も
#   「各複製物に著作権表示と**このライセンス**を含めること」を求めている。
#   ★`glyphs.h` はフォントの埋め込みビットマップと**バイト単位で同一**（全数照合済み）で、
#   「レンダリング結果」ではなく font software の抽出なので、条件 5 も掛かる。
#   FreeType 版が同梱する zenmai.otf も Noto の部分集合＝ font software なので同じ。
#   出どころと判断は native/vendor/kh-dotfont/README.md と vendor/noto-cjk/README.md。
#
# ★本文は両者で同一（OFL 1.1 そのもの）なので 1 回だけ焼く。
#   KH 側のファイルは 1 行目から OFL の本文なので、そちらを使う。
put_file(HERE / 'vendor/kh-dotfont/LICENSE')
put('')

# ★事実関係: Infocom の登録(Reg. 1,227,668 / 1983)は **2003-11-22 に取り消されている**
#   (Activision が更新しなかった)。だから「登録商標」とは書けない。
#   Activision は 2023 年に Microsoft へ、原作は 2025 年に MIT で公開された。
#   ★ゲームの banner が出す `ZORK is a registered trademark of Infocom, Inc.` は
#   **原作の文なのでそのまま**。ここはこちらの文なので、いまの姿を書く。
put('Zork は Infocom の商標です。', JA)
put('現在の権利者は Microsoft です。', JA)
put('このソフトは、どちらとも関係がありません。', JA)
put('Zork is a trademark of Infocom.', EN)
put('The rights are now held by Microsoft.', EN)
put('This software is not affiliated with either.', EN)

# ---- 画面の文言 ----
# ★ここに出る文字列は全部「こちらのもの」。原作の文は 1 つも出ないので、
#   訳の表は通さず完成行として持つ(「その文字列は誰のものか」)。
# ★項目名はそのまま頁の見出しになる(並びは P_* と同じ)。★ただし**頁を持つのは頭の 3 つだけ**。
ITEM = [
    ('ひらがな入力方法', 'HOW TO TYPE'),
    ('システムコマンド', 'SYSTEM COMMANDS'),
    ('ライセンス',       'LICENSE'),
]
# ★★4 つめは**頁ではなく、その場でやること** ——「やめる」。
#   ★気軽にやめられないゲームは良くない（実機の指摘）。コマンドで打てばやめられるのは
#   確かだが、それを知らない人はメニューを探す。だから**近道をメニューに置く**。
#   ★これで**項目の数（4）と読み物の頁の数（3）が別になる**（UI_MENU_N / UI_PAGE_N）。
# ★★出す札も投げる語も**語彙の原簿から引く**。書き写すと、語を足し引きしたときに
#   ここだけ古くなって**押しても打てない言葉**を投げることになる（P_CMDS と同じ作法）。
QUIT_JA = _kana(_asset['verbs']['QUIT']['ja'])[0]      # やめる
QUIT_EN = 'quit'                                       # ★投げるのは小文字（打った通りに映る）
ITEM.append((QUIT_JA, QUIT_EN.upper()))                # 札は他の項目と同じ総大文字
MENU_N = len(ITEM)
# ★★ボタンの案内は**画面に書かない**。Start で開いたら Start で閉じる、開いた先で
#   フェイスボタンを押せば決まる —— これは当時から今まで浸透している作法なので、
#   いちいち書くと**画面がその分だけ狭くなるだけ**になる。
#   (だから決定を ○/× で入れ替える必要も無くなった = **どのフェイスボタンでも決まる**)
# ★起動の言語メニューに出す一行だけは残す。**メニューがあること**は作法ではないし、
#   物語の紙面にシステムの字は混ぜないので、全員が通るここでしか知らせられない。
BOOT = ('ゲーム中 START で メニュー', 'PRESS START IN GAME FOR THE MENU')
# ★起動メニューの題と選択肢。**C 側に直書きしない** —— ここに置けば
#   「出す字がフォントにあるか」の照合（このファイルの末尾）を一緒に通せる。
# ★綴りは他の画面と揃える（Zenmai / Zork。総大文字の ZENMAI ZORK I をやめた）。
# ★★**題は 2 段に分ける**。`Zenmai` は道具の名で、`Zork I` はその上で動く作品の名 ——
#   一行に並べると、説明文がどちらに掛かるのか読めない（実際そうなっていた）。
#   上段 = 名前 + 説明 + 罫線で閉じる / 下段 = 遊ぶ作品 + 言語、という積み方にする。
TITLE = 'Zenmai'
# ★一行の説明。**Zenmai だけの説明**であって Zork の説明ではない。
#   **言語を選ぶ前**の画面なので、どちらの人も読める側に倒して英語 1 本。
SUB = 'a Z-machine for Japanese and a game controller'
# ★起動画面の罫線も**字を並べない**（ライセンス頁の rule() と同じ理由）。
#   長さは C 側が UI_SUB を実測して決めるので、ここには持たない。
GAME = 'Zork I'
LANG = ('日本語', 'ENGLISH')
# ★「ひらがな入力方法」の図の固定札。★動く札（十字・面ボタン・R1）は**入力表から引く**ので
#   ここには無い —— 図に出ている字と実際に入る字が違う、が最悪の事故（web 版の教訓）。
# ★英語面は**かなの行/段ではない** —— プッシュホン式(T9)で、十字がキー・面ボタンが
#   そのキーの何文字目か、を選ぶ。だから ROW / VOWEL は直訳の誤りになる。
# ★SELECT は**言語で役が違う**（日本語 = 濁点/半濁点のトグル、英語 = 空白）。
#   札は実際の役をそのまま名指しする（「くうはく」と書いていたのは誤り）。
# ★かなのままにしてあるものは**そのボタンで出る字そのもの**（は/ま/や/ら/わ の行頭、
#   ん・を、っ）。字を名指ししている札なので、漢字に直すと別のものを指してしまう。
HELP = [
    ('L1 はまやらわ',       'L1 KEYS 6-0'),
    ('L2 拗音',             'L2 SCROLL'),
    ('R2 ん を',            'R2 0'),
    ('SELECT 濁点 半濁点',  'SELECT SPACE'),
    # ★START だけは「ひらがな入力方法」ではないので括弧に入れて、別の種類だと示す
    ('（START 確定）',      '(START SEND)'),
    # ★この一行が**左右の役をすでに言っている**ので、群の見出しは持たない
    ('左手側で行を選び、右手側で文字を入力', 'LEFT HAND PICKS THE KEY, RIGHT HAND TYPES'),
    # ★っ は日本語だけ（英語面の L2 はシフト）。
    # ★この行だけ**半角空白を使わない** —— 全角の字が続くところに半角の隙間が混ざると
    #   切れ目が読み取りにくい。区切りは全角空白 1 つで足りる
    ('R2長押しで1文字消す　L2とR2同時押しで「っ」を入力', 'HOLD R2 TO DELETE'),
]

# ---- 画面幅で折り返す ----
# ★専用画面で描くので、**折り返しはここで済ませておく**(C 側は行を y に並べるだけ)。
#   半角 12px / 全角 24px。★左右の余白は**縁取りの内側**にとるので、
#   main.c の PAGE_X と**必ず同じ値**にすること。


def wrap(t):
    # ★二段組の行は割らない。欄の境をまたいで折ると、右の欄だけが次行の頭に落ちて
    #   **表でなくなる**。入るかどうかは組む側（上の `_rows`）で見てある。
    if COL in t or width(t) <= TEXT_W:
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
# ★**フォントごとにも**濾すようになったので、末尾の判定も (頁 × 言語 × フォント) で見る
#   —— 焼いた版と FreeType 版で「最後の行」が違うため。
_keep = set()
for _pg in range(PAGE_N):
    for _view in (JA, EN):
        for _fv in (F_BAKED, F_FT):
            _idx = [i for i, ln in enumerate(lines)
                    if ln[0] == _pg and ln[1] in (BOTH, _view)
                    and ln[2] in (F_ANY, _fv)]
            while _idx and not lines[_idx[-1]][4]:   # [4] = 本文
                _idx.pop()
            _keep.update(_idx)
lines = [ln for i, ln in enumerate(lines) if i in _keep]

wrapped = []
for pg, lang, font, dim, text, rl in lines:
    for part in wrap(text):
        wrapped.append((pg, lang, font, dim, part, rl))
lines = wrapped

pool = []
recs = []
for pg, lang, font, dim, text, rl in lines:
    off = len(pool)
    pool.extend(ord(c) for c in text)
    recs.append((off, len(text), dim, lang, pg, font, rl))

out = ['/* gen_ui.py が生成。手で編集しない */',
       '/* オプションの読み物 3 頁 + 画面の文言。',
       '   ★ライセンス全文は実ファイルから作っている(書き写していない) */',
       '',
       'typedef struct { unsigned int off; unsigned short len; unsigned char dim;',
       '                 unsigned char lang; unsigned char page;',
       '                 unsigned char font;',
       '                 unsigned char rule; } UiLine;   /* font: 0=常に 1=焼いた版 2=FreeType',
       '                                                    rule: 1 = 本文幅いっぱいの横罫線 */',
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
for off, ln, dim, lang, pg, fnt, rl in recs:
    out.append('    { %d, %d, %d, %d, %d, %d, %d },' % (off, ln, dim, lang, pg, fnt, rl))
out.append('};')
out.append('')
out.append('#define UI_LINE_N %d' % len(recs))
out.append('#define UI_PAGE_N %d' % PAGE_N)
# ★★メニューの項目数は**頁の数とは別**（4 つめ「やめる」は頁を持たない）
out.append('#define UI_MENU_N %d' % MENU_N)
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
quits = [sref(QUIT_JA), sref(QUIT_EN)]
langs = [sref(LANG[0]), sref(LANG[1])]
title_, sub_ = sref(TITLE), sref(SUB)
game_ = sref(GAME)

for i, text in strs:
    out.append('static const unsigned short UI_S%d[%d] = { %s };'
               % (i, len(text), ', '.join('0x%04X' % ord(c) for c in text)))
out.append('')
out.append('static const UiStr UI_ITEM[2][UI_MENU_N] = {')
for row in items:
    out.append('    { ' + ', '.join(row) + ' },')
out.append('};')
out.append('static const UiStr UI_BOOT[2] = { %s, %s };' % (boots[0], boots[1]))
out.append('/* ★メニューの「やめる」が**打つ**語（語彙の原簿から引いてある）。')
out.append('   ★`GState->quit` を直接立てず、原作の QUIT を走らせて')
out.append('   「本当にやめますか」を訊かせる —— 原作がそのまま動くのが Zenmai の主張 */')
out.append('static const UiStr UI_QUIT[2] = { %s, %s };' % (quits[0], quits[1]))
out.append('/* 起動メニュー: 名前 / 説明 / 作品名 / 選択肢(添字 = lang_en)。')
out.append('   ★罫線は持たない —— 長さは main.c が UI_SUB を実測して画素で引く */')
out.append('static const UiStr UI_TITLE = %s;' % title_)
out.append('static const UiStr UI_SUB = %s;' % sub_)
out.append('static const UiStr UI_GAME = %s;' % game_)
out.append('static const UiStr UI_LANG[2] = { %s, %s };' % (langs[0], langs[1]))
out.append('/* 図の固定札: L1 / L2 / R2 / SELECT / START */')
out.append('#define UI_HELP_N %d' % len(HELP))
out.append('static const UiStr UI_HELP[2][UI_HELP_N] = {')
for row in helps:
    out.append('    { ' + ', '.join(row) + ' },')
out.append('};')
out.append('')
(HERE / 'ui_data.h').write_text('\n'.join(out))

used = set()
for ln in lines:
    used.update(ln[4])
for ja, en in ITEM + HELP + [BOOT, LANG]:
    used.update(ja)
    used.update(en)
used.update(QUIT_JA)                   # ★投げる語も画面に出る（打った通りが反響する）
used.update(QUIT_EN)
used.update(TITLE)
used.update(SUB)
used.update(GAME)
used.discard(COL)      # ★欄の境は描かない（字ではない）ので照合から外す
# ★いまは出せないが**次に glyphs.h を作り直すときは入れておきたい字**を書く場所。
#   ここに書いておけば gen_data.py が拾う。2026-08-25 に KH ドットフォントを取り直して
#   焼き直したので、いまは**言い換えている字が 1 つも無い**（だから空）。
WANT = ''
(HERE / 'ui_chars.txt').write_text(''.join(sorted(set(used) | set(WANT))), encoding='utf-8')

# ★**出す字は全部フォントに入っていなければならない**。glyphs.h は「使う字だけ」を
#   焼いてあり、無い字は**黙って空白で描かれる**。ここで止める（実際に「写」で踏んだ）。
_missing = sorted(c for c in used if ord(c) not in FONT)   # WANT は出さないので除く
if _missing:
    raise SystemExit('★同梱フォントに無い字を出そうとしている: ' + ''.join(_missing)
                     + '\n  言い換えるか、フォントのある環境で glyphs.h を作り直すこと')

per = [sum(1 for r in recs if r[4] == p) for p in range(PAGE_N)]
print('ui_data.h: %d 行 (頁ごと %s) / %d 字' % (len(recs), per, len(pool)))
