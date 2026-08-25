#!/usr/bin/env python3
"""turns.json + KH フォント → glyphs.h / content.h(C 直埋め込み)。

- グリフ: 使う字だけ。本文 = 日比谷24(全角 24×24 / 半角 12×24、行 0〜23)、
  ルビ = 神楽坂12(実描画は 'la' 原点の行 1〜12 → その 12 行だけを切り出す)
- 本文: ルビ分節(gen_mock.segment = ruby.js と同じ規則)まで済ませた論理行の列。
  折り返し・行送りは C 側(それが今回作る部品)

使い方: python3 gen_data.py <フォント dir>
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent / 'ps1-mock'))
import gen_mock  # segment()
from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).parent
FONT_DIR = Path(sys.argv[1])

asset = json.loads((HERE.parent / 'assets' / 'zork1-ja.json').read_text())
ruby = asset['ruby']
keys = sorted(ruby.keys(), key=len, reverse=True)
turns = json.loads((HERE / 'turns.json').read_text())

f24 = ImageFont.truetype(str(FONT_DIR / 'KH-Dot-Hibiya-24.ttf'), 24)
f12 = ImageFont.truetype(str(FONT_DIR / 'KH-Dot-Kagurazaka-12.ttf'), 12)

# ---- 本文の構造化 ----
pool, items, lines, tarr = [], [], [], []
base_chars, ruby_chars = set('＞あ'), set()

# 入力エンジンが出力しうる全字種(何を打っても描けること)+ 行インジケータ
INPUT_CHARS = ('あいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほ'
               'まみむめもやゆよらりるれろわゐゑをん'
               # ★ゔ は無い —— KH ドットフォントに字面が無いので、gen_tables.py の
               #   PS1 適応で う→ゔ の濁点を落としてある（打てないから描けなくてよい）
               'がぎぐげござじずぜぞだぢづでどばびぶべぼぱぴぷぺぽ'
               'ぁぃぅぇぉゃゅょゎっー、。？「」　')
# 手順外のコマンドを確定したときの案内(かなだけで書く —— ルビ無しでも読めるように)
NOTICE = '（このためしばんは、きまったてじゅんだけすすむ）'
base_chars.update(INPUT_CHARS)
base_chars.update(NOTICE)
# ★オプション画面で使う字(gen_ui.py が書き出す)。無いとその字だけ空白になる
ui_txt = HERE / 'ui_chars.txt'
if ui_txt.exists():
    base_chars.update(ui_txt.read_text(encoding='utf-8'))
base_chars.update(chr(c) for c in range(0x20, 0x7F))   # 英語版(T9 入力・VM 出力)用に ASCII 全部
# ★`~` は KH ドットフォントに無い。Z-machine v3 のアルファベット表にも無く、
#   `feed_cmd` も入力から弾くので**画面に出ようがない**。焼くと .notdef になるだけ。
base_chars.discard('~')

# 日本語版: 訳アセットに出うる全字種(訳文・語・ルビの親字と読み)
base_chars.add('　')
base_chars.add('：')
for s in asset['exact'].values():
    base_chars.update(s)
for v in asset['props'].values():
    base_chars.update(v['ja'])
for s in asset['words'].values():
    base_chars.update(s)
for t in asset['assembled'] + asset['templates']:
    base_chars.update(t['ja'])
for segs_ in asset['ruby'].values():
    for seg_, yomi_ in segs_:
        base_chars.update(seg_)
        if yomi_:
            ruby_chars.update(yomi_)
# 入力側(コマンド変換)の字種: 語彙・断り書き・手引き
import gen_cmd
cmd_asset = json.loads((HERE.parent / 'assets' / 'zork1-cmd.json').read_text())
for v in cmd_asset['verbs'].values():
    for s in v['ja']:
        base_chars.update(s)
for o in cmd_asset['objects'].values():
    for s in o['ja']:
        base_chars.update(s)
for h_ in cmd_asset.get('hypernyms', []):
    base_chars.update(h_['form'])
    for s in h_.get('yomi', []):
        base_chars.update(s)
for s in cmd_asset.get('guide', {}).values():
    base_chars.update(s)
for s in gen_cmd.UI_FRAGS:
    base_chars.update(s)
for p, _ in gen_cmd.PARTICLES:
    base_chars.update(p)
for s in list(gen_cmd.DIRS) + gen_cmd.ALL_WORDS + list(gen_cmd.PARSER_WORDS) + list(gen_cmd.YESNO):
    base_chars.update(s)
for s in gen_cmd.ROLE_JA.values():
    base_chars.update(s)
base_chars = {c for c in base_chars if ord(c) >= 0x20}   # 制御文字は除く


def put(s):
    off = len(pool)
    pool.extend(ord(c) for c in s)
    return off


for t in turns:
    cmd = t['ja']
    if cmd:
        base_chars.update(cmd)
    loff = len(lines)
    for line in t['lines']:
        if not line.strip():
            lines.append((0, 0))
            continue
        segs = gen_mock.segment(line, ruby, keys)
        # ルビ無しの連続分節をまとめる(segment は不一致文字を 1 字ずつ返す)
        merged = []
        for seg, yomi in segs:
            if not yomi and merged and not merged[-1][1]:
                merged[-1] = (merged[-1][0] + seg, None)
            else:
                merged.append((seg, yomi))
        ioff = len(items)
        for seg, yomi in merged:
            base_chars.update(seg)
            boff = put(seg)
            roff, rlen = 0, 0
            if yomi:
                ruby_chars.update(yomi)
                roff, rlen = put(yomi), len(yomi)
            items.append((len(seg), rlen, boff, roff))
        lines.append((ioff, len(items) - ioff))
    cmd_off = put(cmd) if cmd else 0
    tarr.append((cmd_off, len(cmd) if cmd else 0, loff, len(lines) - loff))

# ---- グリフ描画 ----

# ★KH ドットフォントは U+2014(—) を**中央の小さな四角**で描く（15 書体すべてで同じ。
#   実測した）。訳文も画面の文言も**ダッシュのつもりで — を使っている**ので、
#   そのままだと「Zenmai ‥ このソフト」のような点 2 つに見える。
#   同じ全角の横棒 U+2015(―) の字面を借りて描く —— ★**符号位置は変えない**ので、
#   訳の原簿にも gen_ui.py にも手を入れずに済む（ここは PS1 の適応層）。
GLYPH_ALIAS = {'—': '―'}


def render24(ch):
    ch = GLYPH_ALIAS.get(ch, ch)
    w = int(f24.getlength(ch))
    img = Image.new('1', (24, 24), 0)
    ImageDraw.Draw(img).text((0, 0), ch, font=f24, fill=1, anchor='la')
    return w, [sum(1 << x for x in range(24) if img.getpixel((x, y))) for y in range(24)]


def render12(ch):
    ch = GLYPH_ALIAS.get(ch, ch)
    img = Image.new('1', (12, 14), 0)
    ImageDraw.Draw(img).text((0, 0), ch, font=f12, fill=1, anchor='la')
    return [sum(1 << x for x in range(12) if img.getpixel((x, y))) for y in range(1, 13)]


base = sorted((ord(c), *render24(c)) for c in base_chars)
rubyg = sorted((ord(c), render12(c)) for c in ruby_chars)

# ★**フォントに無い字は「空白」ではなく .notdef（黒い四角）になる**。
#   gen_ui.py の照合は「glyphs.h に載っているか」しか見ないので、**ここは素通りする**
#   —— 載ってはいるのだ、.notdef として。実際 `ゔ` が黒い四角のまま配布されていた。
#   フォントに確実に無い符号位置を 1 つ選んでその字面を採り、同じものを弾く。
_notdef = render24('\uE000')[1]        # 私用領域 = どのフォントにも無い
_bad = [chr(c) for c, w, rows in base if rows == _notdef]
if _bad:
    raise SystemExit('★フォントに無い字を焼こうとしている（黒い四角になる）: '
                     + ''.join(_bad)
                     + '\n  その字を使わないようにするか、字面を持っている書体を探すこと')

# ---- 出力 ----
with open(HERE / 'glyphs.h', 'w') as f:
    f.write('/* gen_data.py が生成。手で編集しない */\n')
    f.write('typedef struct { unsigned short code; unsigned char width; } GInfo;\n')
    f.write(f'enum {{ BASE_N = {len(base)}, RUBY_N = {len(rubyg)} }};\n')
    f.write('static const GInfo base_info[BASE_N] = {\n')
    f.write(''.join(f'  {{0x{c:04X}, {w}}},\n' for c, w, _ in base) + '};\n')
    f.write('static const unsigned base_rows[BASE_N][24] = {\n')
    f.write(''.join('  {' + ','.join(f'0x{r:06X}' for r in rows) + '},\n' for _, _, rows in base) + '};\n')
    f.write('static const GInfo ruby_info[RUBY_N] = {\n')
    f.write(''.join(f'  {{0x{c:04X}, 12}},\n' for c, _ in rubyg) + '};\n')
    f.write('static const unsigned short ruby_rows[RUBY_N][12] = {\n')
    f.write(''.join('  {' + ','.join(f'0x{r:03X}' for r in rows) + '},\n' for _, rows in rubyg) + '};\n')

with open(HERE / 'content.h', 'w') as f:
    f.write('/* gen_data.py が生成。手で編集しない(定義は content_data.c) */\n')
    f.write('#ifndef CONTENT_H\n#define CONTENT_H\n')
    f.write('typedef struct { unsigned char blen, rlen; unsigned short boff, roff; } Item;\n')
    f.write('typedef struct { unsigned short off, cnt; } Line;  /* cnt==0 は空行 */\n')
    f.write('typedef struct { unsigned short cmd_off, cmd_len, line_off, line_cnt; } Turn;\n')
    f.write(f'enum {{ TURN_N = {len(tarr)}, EXTRA_NOTICE_LEN = {len(NOTICE)} }};\n')
    f.write('extern const unsigned short pool[];\n')
    f.write('extern const Item citems[];\n')
    f.write('extern const Line clines[];\n')
    f.write('extern const Turn cturns[TURN_N];\n')
    f.write('extern const unsigned short extra_notice[EXTRA_NOTICE_LEN];\n')
    f.write('#endif\n')

with open(HERE / 'content_data.c', 'w') as f:
    f.write('/* gen_data.py が生成。手で編集しない */\n#include "content.h"\n')
    f.write('const unsigned short pool[] = {\n  ' +
            ','.join(f'0x{c:04X}' for c in pool) + '\n};\n')
    f.write('const Item citems[] = {\n' +
            ''.join(f'  {{{b},{r},{bo},{ro}}},\n' for b, r, bo, ro in items) + '};\n')
    f.write('const Line clines[] = {\n' +
            ''.join(f'  {{{o},{c}}},\n' for o, c in lines) + '};\n')
    f.write('const Turn cturns[TURN_N] = {\n' +
            ''.join(f'  {{{a},{b},{c},{d}}},\n' for a, b, c, d in tarr) + '};\n')
    f.write('const unsigned short extra_notice[EXTRA_NOTICE_LEN] = {\n  ' +
            ','.join(f'0x{ord(c):04X}' for c in NOTICE) + '\n};\n')

print(f'glyphs.h: 本文 {len(base)} 字 / ルビ {len(rubyg)} 字, '
      f'content.h: {len(tarr)} ターン {len(lines)} 行 {len(items)} 項 pool {len(pool)}')
