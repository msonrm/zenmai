#!/usr/bin/env python3
"""zork1-ja.json の ruby(送り仮名アラインメント済み 387 語)→ ruby_data.c/h。

分節規則の正典は src/ruby.js(jp_text.c が C 移植)。
キーは長さ降順で並べる(「台所の窓」を「窓」に食われないように)。
"""
import json
from pathlib import Path

HERE = Path(__file__).parent
ruby = json.loads((HERE.parent / 'assets' / 'zork1-ja.json').read_text())['ruby']

keys = sorted(ruby.keys(), key=len, reverse=True)

pool = []
def put(s):
    off = len(pool)
    pool.extend(ord(c) for c in s)
    return off, len(s)

segs = []      # (boff, blen, yoff, ylen)
kout = []      # (koff, klen, seg_off, seg_n)
for k in keys:
    koff, klen = put(k)
    seg_off = len(segs)
    for seg, yomi in ruby[k]:
        bo, bl = put(seg)
        yo, yl = put(yomi) if yomi else (0, 0)
        segs.append((bo, bl, yo, yl))
    kout.append((koff, klen, seg_off, len(segs) - seg_off))

with open(HERE / 'ruby_data.h', 'w') as f:
    f.write('/* gen_ruby.py が生成。手で編集しない */\n')
    f.write('#ifndef RUBY_DATA_H\n#define RUBY_DATA_H\n')
    f.write('typedef struct { unsigned int bo; unsigned short bl; unsigned int yo; unsigned short yl; } RbSeg;\n')
    f.write('typedef struct { unsigned int ko; unsigned short kl; unsigned short seg_off, seg_n; } RbKey;\n')
    f.write(f'enum {{ RB_KEY_N = {len(kout)} }};\n')
    f.write('extern const unsigned short rb_pool[];\n')
    f.write('extern const RbSeg rb_segs[];\n')
    f.write('extern const RbKey rb_keys[RB_KEY_N];\n')
    f.write('#endif\n')

with open(HERE / 'ruby_data.c', 'w') as f:
    f.write('/* gen_ruby.py が生成。手で編集しない */\n#include "ruby_data.h"\n')
    f.write('const unsigned short rb_pool[] = {\n  ' +
            ','.join(f'0x{c:04X}' for c in pool) + '\n};\n')
    f.write('const RbSeg rb_segs[] = {\n' +
            ''.join(f'  {{{a},{b},{c},{d}}},\n' for a, b, c, d in segs) + '};\n')
    f.write('const RbKey rb_keys[] = {\n' +
            ''.join(f'  {{{a},{b},{c},{d}}},\n' for a, b, c, d in kout) + '};\n')

print(f'ruby_data: {len(kout)} 語 / segs {len(segs)} / pool {len(pool) * 2}B')
