#!/usr/bin/env python3
"""zork1-ja.json の ruby(送り仮名アラインメント済み 387 語)→ ruby_data.c/h。

分節規則の正典は src/ruby.js(jp_text.c が C 移植)。
キーは長さ降順で並べる(「台所の窓」を「窓」に食われないように)。

  python3 gen_ruby.py              → ruby_data.{c,h}・ruby_tab.c（ctab.py）
  python3 gen_ruby.py --sec 出力   → パックの節（RUBY）だけ
"""
import json
from pathlib import Path
from ctab import Struct, Table, emit, sec_path

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

RbSeg = Struct('RbSeg', [('unsigned int', 'bo'), ('unsigned short', 'bl'), ('unsigned int', 'yo'), ('unsigned short', 'yl')])
RbKey = Struct('RbKey', [('unsigned int', 'ko'), ('unsigned short', 'kl'), ('unsigned short', 'seg_off'), ('unsigned short', 'seg_n')])
emit('ruby', 'RB', 'gen_ruby.py', [RbSeg, RbKey], [
    Table('rb_pool', 'unsigned short', pool),
    Table('rb_segs', RbSeg, segs),
    Table('rb_keys', RbKey, kout, 'RB_KEY_N'),
], sec_path=sec_path(), here=HERE)

print(f'ruby_data: {len(kout)} 語 / segs {len(segs)} / pool {len(pool) * 2}B')
