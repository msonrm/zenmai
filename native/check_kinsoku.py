#!/usr/bin/env python3
"""禁則の字の並びが **C（render.c）と Python の参照実装（gen_mock.py）で同じ**か。

★規則を 2 つの言語で持っている以上、片方だけ足して片方を忘れる道がある。
  そうなると `golden.py` が作るゴールデンと C の実挙動が静かにずれ、
  ★**画素照合が「移植のせいで壊れた」ように見える**（原因は表の食い違いなのに）。

★C 側は switch の `case` を、Python 側は集合を、それぞれ**その場で読んで**突き合わせる
  —— どちらかを写した第三の表を持たない（写した瞬間に 3 か所を揃える仕事が増える）。
"""
import re
import sys
from pathlib import Path

HERE = Path(__file__).parent
sys.path.insert(0, str(HERE.parent / 'ps1-mock'))
import gen_mock                                          # noqa: E402

src = (HERE / 'render.c').read_text(encoding='utf-8')


def c_codes(fn):
    try:
        body = src.split('static int %s(uint16_t c)' % fn)[1].split('}\n')[0]
    except IndexError:
        sys.exit('★render.c の %s() を読めない（形が変わった？）' % fn)
    codes = set(int(m, 16) for m in re.findall(r'case 0x([0-9A-Fa-f]{4}):', body))
    if not codes:
        sys.exit('★%s() から 1 字も読めなかった（読み方が壊れている）' % fn)
    return codes


ng = 0
for name, fn, pyset in (('行頭', 'no_head', gen_mock.NO_HEAD),
                        ('行末', 'no_tail', gen_mock.NO_TAIL)):
    a, b = c_codes(fn), set(ord(c) for c in pyset)
    if a == b:
        print('✓ %s禁則: C と Python で一致（%d 字）' % (name, len(a)))
    else:
        print('✗ %s禁則が食い違う —— C だけ「%s」/ Python だけ「%s」'
              % (name, ''.join(chr(x) for x in sorted(a - b)),
                 ''.join(chr(x) for x in sorted(b - a))))
        ng += 1

# ★カナリア: 突き合わせが素通りしていないか（在るはずのない字を足して赤になること）
if c_codes('no_head') == set(ord(c) for c in gen_mock.NO_HEAD) | {ord('A')}:
    print('✗ ★カナリア: 違う集合が一致してしまう = 突き合わせが死んでいる')
    ng += 1
else:
    print('✓ ★カナリア: 違う集合はちゃんと食い違う')

print()
if ng:
    print('--- ★%d 件 食い違った ---' % ng)
    sys.exit(1)
print('--- 禁則の字は 2 つの実装で同じ ---')
