#!/usr/bin/env python3
"""pairs.jsonl → pairs.h(ホスト照合テスト用の C データ)。"""
import json
import sys
from pathlib import Path

HERE = Path(__file__).parent
pairs = [json.loads(l) for l in (HERE / 'pairs.jsonl').read_text().splitlines()]

out = ['/* gen_pairs_h.py が生成 */']
out.append('typedef struct { const unsigned char *raw; int raw_len; '
           'const unsigned short *ja; int ja_len; const char *echo; } Pair;')
rows = []
for i, (raw, ja, echo) in enumerate(pairs):
    rb = raw.encode('utf-8')
    out.append(f'static const unsigned char raw_{i}[] = {{'
               + ','.join(str(b) for b in rb) + (',0};' if rb else '0};'))
    if ja is None:
        rows.append(f'  {{raw_{i}, {len(rb)}, 0, -1, {json.dumps(echo) if echo else "0"}}},')
    else:
        jb = [ord(c) for c in ja]
        out.append(f'static const unsigned short ja_{i}[] = {{'
                   + (','.join(f'0x{c:04X}' for c in jb) if jb else '0') + '};')
        rows.append(f'  {{raw_{i}, {len(rb)}, ja_{i}, {len(jb)}, {json.dumps(echo) if echo else "0"}}},')
out.append(f'enum {{ PAIR_N = {len(pairs)} }};')
out.append('static const Pair PAIRS[] = {')
out.extend(rows)
out.append('};')
(HERE / 'pairs.h').write_text('\n'.join(out) + '\n')
print(f'pairs.h: {len(pairs)} 行')
