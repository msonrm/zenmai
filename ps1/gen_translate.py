#!/usr/bin/env python3
"""zork1-ja.json → translate_data.c/h(訳の層の静的データ)。

Translator(src/translate.js)のコンストラクタ相当をビルド時に実行する:
  - exact/props/words の正規化と | 分割の展開
  - テンプレートの「文字列と穴」へのコンパイル(正規表現はここで消える)
  - 具体性(穴を除いた字数)降順の安定ソート
挙動の正典は JS。ここを直すときは translate.js と突き合わせること。

使い方: python3 gen_translate.py
"""
import json
import re
from pathlib import Path

HERE = Path(__file__).parent
asset = json.loads((HERE.parent / 'assets' / 'zork1-ja.json').read_text())

norm = lambda s: re.sub(r'\s+', ' ', s).strip()

# ---- コンストラクタ再現 ----
exact = {}
for en, ja in asset['exact'].items():
    exact[norm(en)] = ja
props = {}
for en, v in asset['props'].items():
    props[norm(en)] = v['ja']

SLOT_RE = re.compile(r'\{([A-Z0-9,?!-]+)\}')

patterns = []          # (en_norm, len, segs_en, ja, names, quotes)
for t in asset['assembled'] + asset['templates']:
    en = norm(t['en'])
    if not re.search(r'\{[A-Z]', en):
        if en not in exact:
            exact[en] = t['ja']
        continue
    names, quotes, segs = [], [], []   # segs: ('lit', str) | ('hole', quoted)
    i = 0
    for m in SLOT_RE.finditer(en):
        segs.append(('lit', en[i:m.start()]))
        names.append(m.group(1).rstrip(','))
        quoted = m.start() > 0 and en[m.start() - 1] == '"' and \
            m.end() < len(en) and en[m.end()] == '"'
        quotes.append(quoted)
        segs.append(('hole', quoted))
        i = m.end()
    segs.append(('lit', en[i:]))
    plen = len(SLOT_RE.sub('', en))
    patterns.append((en, plen, segs, t['ja'], names, quotes))

patterns.sort(key=lambda p: -p[1])     # 安定ソート = JS(V8)と同じ

# | 分割の展開(exact → props の順で走査。JS の [...exact, ...props] と同じ)
for en, ja in list(exact.items()) + list(props.items()):
    if '|' not in en:
        continue
    e = [x.strip() for x in en.split('|')]
    j = [x.strip() for x in ja.split('|')]
    if len(e) != len(j):
        continue
    for a, b in zip(e, j):
        if a and b and a not in exact:
            exact[a] = b

notrans = []
for en in asset.get('notrans', []):
    notrans.append(norm(en))
    for part in en.split('|'):
        if norm(part):
            notrans.append(norm(part))
# JS は Set(重複除去・挿入順)。包含判定は全要素走査なので順序は挙動に影響しない
seen = set()
notrans = [x for x in notrans if not (x in seen or seen.add(x))]

words = dict(asset.get('words', {}))

# ---- JA テンプレートを穴参照つきセグメントへ ----
# JS は out.replace('{'+name+'}', v) を names 順に行う(毎回そのとき先頭の 1 個所だけ)。
# 同名スロットが複数あっても、names 順に「未消費の最初の {NAME}」へ割り当てれば同じ。


def compile_ja(ja, names):
    used = [False] * len(ja)
    holes = {}                          # 文字位置 → capture index
    for idx, name in enumerate(names):
        tok = '{' + name + '}'
        pos = -1
        start = 0
        while True:
            pos = ja.find(tok, start)
            if pos < 0 or not used[pos]:
                break
            start = pos + 1
        if pos < 0:
            continue                    # JA 側に無い名前は捨てられる(JS と同じ)
        for k in range(pos, pos + len(tok)):
            used[k] = True
        holes[pos] = (idx, len(tok))
    segs = []                           # ('lit', str) | ('ref', idx)
    i = 0
    while i < len(ja):
        if i in holes:
            idx, tlen = holes[i]
            segs.append(('ref', idx))
            i += tlen
        else:
            j = i
            while j < len(ja) and j not in holes:
                j += 1
            segs.append(('lit', ja[i:j]))
            i = j
    return segs


# ---- C データ生成 ----
en_pool = bytearray()
ja_pool = []


def en_off(s):
    b = s.encode('utf-8')   # 非 ASCII キーも保持(VM 出力とは一致しないだけ = JS と同じ)
    off = len(en_pool)
    en_pool.extend(b)
    return off, len(b)


def ja_off(s):
    off = len(ja_pool)
    ja_pool.extend(ord(c) for c in s)
    return off, len(s)


def pair_table(d):
    rows = []
    for en, ja in d.items():
        eo, el = en_off(en)
        jo, jl = ja_off(ja)
        rows.append((en, eo, el, jo, jl))
    rows.sort(key=lambda r: r[0].encode('utf-8'))   # バイト順(C の memcmp と一致)
    return rows


exact_rows = pair_table(exact)
props_rows = pair_table(props)
words_rows = pair_table(words)

segs_out = []                          # (off, len, kind, slot)
pats_out = []                          # (seg_off, en_n, ja_n, has_echo)
K_LIT, K_HOLE, K_QHOLE, K_JLIT, K_JREF = 0, 1, 2, 3, 4
F_ECHO, F_VERB, F_SAID = 1, 2, 4
for en, plen, segs, ja, names, quotes in patterns:
    seg_off = len(segs_out)
    en_n = 0
    hidx = 0
    for kind, v in segs:
        if kind == 'lit':
            if v:
                o, l = en_off(v)
                segs_out.append((o, l, K_LIT, 0))
                en_n += 1
        else:
            flags = (F_ECHO if names[hidx] == 'ECHO' else 0) | \
                    (F_VERB if names[hidx] == 'VERB' else 0) | \
                    (F_SAID if names[hidx] == 'SAID' else 0)
            segs_out.append((0, 0, K_QHOLE if v else K_HOLE, flags))
            en_n += 1
            hidx += 1
    ja_n = 0
    for kind, v in compile_ja(ja, names):
        if kind == 'lit':
            if v:
                o, l = ja_off(v)
                segs_out.append((o, l, K_JLIT, 0))
                ja_n += 1
        else:
            segs_out.append((0, 0, K_JREF, v))
            ja_n += 1
    pats_out.append((seg_off, en_n, ja_n, 1 if 'ECHO' in names else 0))

nt_rows = []
for s in notrans:
    o, l = en_off(s)
    nt_rows.append((o, l))

with open(HERE / 'translate_data.h', 'w') as f:
    f.write('/* gen_translate.py が生成。手で編集しない */\n')
    f.write('#ifndef TRANSLATE_DATA_H\n#define TRANSLATE_DATA_H\n')
    f.write('typedef struct { unsigned int eo; unsigned short el; unsigned int jo; unsigned short jl; } TrPair;\n')
    f.write('typedef struct { unsigned int off; unsigned short len; unsigned char kind; unsigned char slot; } TrSeg;\n')
    f.write('typedef struct { unsigned short seg_off, en_n, ja_n, has_echo; } TrPat;\n')
    f.write('enum { TRK_LIT, TRK_HOLE, TRK_QHOLE, TRK_JLIT, TRK_JREF };\n')
    f.write('enum { TRF_ECHO = 1, TRF_VERB = 2, TRF_SAID = 4 };\n')
    f.write(f'enum {{ TR_EXACT_N = {len(exact_rows)}, TR_PROPS_N = {len(props_rows)}, '
            f'TR_WORDS_N = {len(words_rows)}, TR_PATS_N = {len(pats_out)}, TR_NT_N = {len(nt_rows)} }};\n')
    f.write('extern const char tr_en_pool[];\n')
    f.write('extern const unsigned short tr_ja_pool[];\n')
    f.write('extern const TrPair tr_exact[TR_EXACT_N];\n')
    f.write('extern const TrPair tr_props[TR_PROPS_N];\n')
    f.write('extern const TrPair tr_words[TR_WORDS_N];\n')
    f.write('extern const TrSeg tr_segs[];\n')
    f.write('extern const TrPat tr_pats[TR_PATS_N];\n')
    f.write('extern const TrPair tr_notrans[TR_NT_N];\n')
    f.write('#endif\n')

with open(HERE / 'translate_data.c', 'w') as f:
    f.write('/* gen_translate.py が生成。手で編集しない */\n#include "translate_data.h"\n')
    f.write('const char tr_en_pool[] = {\n  ' +
            ','.join(str(b) for b in en_pool) + '\n};\n')
    f.write('const unsigned short tr_ja_pool[] = {\n  ' +
            ','.join(f'0x{c:04X}' for c in ja_pool) + '\n};\n')
    for name, rows in [('tr_exact', exact_rows), ('tr_props', props_rows), ('tr_words', words_rows)]:
        f.write(f'const TrPair {name}[] = {{\n' +
                ''.join(f'  {{{eo},{el},{jo},{jl}}},\n' for _, eo, el, jo, jl in rows) + '};\n')
    f.write('const TrSeg tr_segs[] = {\n' +
            ''.join(f'  {{{o},{l},{k},{s}}},\n' for o, l, k, s in segs_out) + '};\n')
    f.write('const TrPat tr_pats[] = {\n' +
            ''.join(f'  {{{a},{b},{c},{d}}},\n' for a, b, c, d in pats_out) + '};\n')
    f.write('const TrPair tr_notrans[] = {\n' +
            ''.join(f'  {{{o},{l},0,0}},\n' for o, l in nt_rows) + '};\n')

print(f'translate_data: exact {len(exact_rows)} / props {len(props_rows)} / words {len(words_rows)} '
      f'/ patterns {len(pats_out)} / notrans {len(nt_rows)} '
      f'/ en_pool {len(en_pool)}B / ja_pool {len(ja_pool) * 2}B')
