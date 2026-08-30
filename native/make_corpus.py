#!/usr/bin/env python3
"""コマンド変換の等価性検証コーパスを作る(固定表 + 系統的な組み合わせ)。"""
import json
import random
import re
from pathlib import Path

HERE = Path(__file__).parent
asset = json.loads((HERE.parent / 'assets' / 'zork1-cmd.json').read_text())

cases = []

# 固定表(run-cmd.js)の入力を吸い上げる
src = (HERE.parent / 'test' / 'run-cmd.js').read_text()
for m in re.finditer(r"^\s*\['([^']+)',", src, re.M):
    cases.append(m.group(1))

verbs_ja = [ja for v in asset['verbs'].values() for ja in v['ja']]
objs_ja = [o['ja'][0] for o in asset['objects'].values()]
all_objs_ja = [ja for o in asset['objects'].values() for ja in o['ja']]

rng = random.Random(42)
V = lambda: rng.choice(verbs_ja)
O = lambda: rng.choice(all_objs_ja)

# 系統: 全動詞 × 代表目的語 / 全目的語 × 代表動詞
for v in verbs_ja:
    cases.append(f'{rng.choice(objs_ja)}を{v}')
    cases.append(v)                        # 動詞単独(needsObject / bare)
for o in all_objs_ja:
    cases.append(f'{o}をとる')
    cases.append(o)                        # 名詞のみ

# 型のバリエーション
PATTERNS = ['{o}を{v}', '{o}で{v}', '{o}に{v}', '{o}から{v}', '{o}の中に{p}を{v}',
            '{o}の下を{v}', '{o}の上に{p}をおく', '{p}を{o}で{v}', '{o}と{p}を{v}',
            '{o}いがいをとる', '{o}と{p}いがいをとる', '{o}へいく', '{v}、{o}']
for pat in PATTERNS:
    for _ in range(120):
        cases.append(pat.format(o=O(), v=V(), p=O()))

# ★コマンド不適語（本文に出るが原作の語彙に無い語）。**読みも表記も両方**通す ——
#   ここが黙って別の物に化けていた（`てんじょう` → `じょう` = 錠）ので、
#   JS と C が同じ答えを返すことをコーパスで見張る。
for nc in asset.get('nocmd', []):
    for w in [nc['form']] + list(nc.get('yomi') or []):
        cases.append(f'{w}をみる')
        cases.append(f'{w}をとる')

# 方角・パーサ語・ALL・否定・未知語
cases += ['きた', '北', 'みなみへいく', 'うえにのぼる', 'したへおりる', 'なかにはいる', 'そとへでる',
          'ぜんぶとる', 'ぜんぶをとる', 'すべてをおく', 'もういちど', 'はい', 'いいえ',
          'とびらをあけない', 'たべません', 'やらぬ', 'しなかった',
          'ひるめしをたべる', 'どあをあける', 'xyzzy', 'open mailbox', ' east ',
          'カタカナヲトル', 'ケンヲトル', 'ｹﾞｰﾑをやめる', 'ゆうびんばこを開ける']
# ランダムかな混入
KANAS = 'あいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほまみむめもやゆよらりるれろわをん'
for _ in range(150):
    w = ''.join(rng.choice(KANAS) for _ in range(rng.randint(2, 6)))
    cases.append(f'{w}を{V()}')
    cases.append(f'{O()}を{w}')

seen = set()
out = [c for c in cases if not (c in seen or seen.add(c))]
(HERE / 'corpus.txt').write_text('\n'.join(out) + '\n')
print(f'corpus.txt: {len(out)} 件')
