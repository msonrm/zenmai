#!/usr/bin/env python3
"""ライセンス頁が **使っているフォントだけ** を名乗っているかを見る。

★2026-08-30 に実機で指摘された —— PortMaster 版は KH ドットフォントの字形を
もう使っていない（Noto を FreeType で描いている）のに、ライセンス頁には
KH ドットフォントの項が残っていた。★**使っていないフォントの名を出すのは嘘**で、
かつ実際に使っている Noto の帰属が画面に無かった（OFL 条件 2 は同梱の
licenses/ で満たしてはいるが、画面が嘘なのは別の問題）。

UiLine の font フィールド（0=常に / 1=焼いた版 / 2=FreeType）で出し分けている。
ここではその結果を**デコードして突き合わせる**。
"""
import re
import sys
from pathlib import Path

HERE = Path(__file__).parent
src = (HERE / 'ui_data.h').read_text(encoding='utf-8')
pool = [int(x, 16) for x in
        re.findall(r'0x([0-9A-F]{4})', src.split('UI_POOL')[1].split('};')[0])]
# ★UiLine は (off, len, dim, lang, page, font, rule) の 7 つ。
#   ★列が増えたら**読めなくなって止まる**のが正しい（黙って 0 件にしない）。
recs = [tuple(int(v) for v in m) for m in
        re.findall(r'\{ (\d+), (\d+), (\d+), (\d+), (\d+), (\d+), (\d+) \},', src)]
if not recs:
    sys.exit('★UI_LINES を読めない（gen_ui.py の出力形式が変わった？）')


def shown(kind):
    """font=kind のビルドがライセンス頁に出す全文（言語は問わない）"""
    return ' '.join(''.join(chr(c) for c in pool[o:o + l])
                    for o, l, dim, lang, pg, f, rl in recs
                    if pg == 2 and f in (0, kind))


ng = 0
CASES = [
    (1, '焼いた版（PS1）',            'KHドットフォント', 'Noto'),
    (2, 'FreeType 版（PortMaster）', 'Noto',            'KHドットフォント'),
]
for kind, name, must, must_not in CASES:
    t = shown(kind)
    if must not in t:
        print(f'✗ {name}: 使っているはずの「{must}」が出ていない')
        ng += 1
    elif must_not in t:
        print(f'✗ {name}: 使っていない「{must_not}」が出てしまう')
        ng += 1
    else:
        print(f'✓ {name}: 「{must}」だけを名乗る')

# ★どちらのビルドでも OFL の全文は出ていること（条件 2）
for kind, name, _, _ in CASES:
    if 'SIL Open Font License' in shown(kind):
        print(f'✓ {name}: OFL の本文が焼かれている')
    else:
        print(f'✗ {name}: OFL の本文が無い')
        ng += 1

# ★カナリア: 突き合わせが素通りしていないか（在るはずのない語で引く）
if 'ZZ-NOT-A-FONT-ZZ' in shown(1):
    print('✗ ★カナリア: 在るはずのない語が見つかる = 読み方が壊れている')
    ng += 1
else:
    print('✓ ★カナリア: 在るはずのない語はちゃんと見つからない')

print()
if ng:
    print(f'--- ★{ng} 件 食い違った ---')
    sys.exit(1)
print('--- ライセンス頁は使っているフォントだけを名乗っている ---')
