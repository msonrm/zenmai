#!/usr/bin/env python3
"""PC-98 のテキスト VRAM に置く字のコード（Unicode → 漢字 ROM の位置）。

★**正典はここ 1 つ。** 画面の試作（`pc98-mock/layout.py`）もこれを読む。

- 区点を総なめして、euc_jp と cp932 の**両方の読み**を同じ位置へ落とす。
  ★cp932 でしか引けない形（`～` U+FF5E・`−` U+2212・13 区の NEC 特殊文字）もこれで入る
- `—` U+2014 はどちらにも無いので `―` U+2015 に寄せる
- JIS → 漢字 ROM の位置は下の 2 つの入れ替え表（QuuBee の makefont.cjs と同じ）。
  ★**QuuBee の上では画素まで一致を確認済み**（pc98-mock）。実機の ROM での並びは未確認

実行すると `pc98_jis.{c,h}`（UTF-16 → テキスト VRAM の語、Unicode 順の表）を書き、
★訳・語彙の JSON に出る字がすべて引けるかを確かめる（引けなければ失敗する）:

    python3 pc98_jis.py
"""
import json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))

J78 = [(0x3646, 0x7421), (0x4b6a, 0x7422), (0x4d5a, 0x7423), (0x596a, 0x7424)]
J90 = [(0x724d, 0x3033), (0x7274, 0x3229), (0x695a, 0x3342), (0x5978, 0x3349), (0x635e, 0x3376),
       (0x5e75, 0x3443), (0x6b5d, 0x3452), (0x7074, 0x375b), (0x6268, 0x395c), (0x6922, 0x3c49),
       (0x7057, 0x3f59), (0x6c4d, 0x4128), (0x5464, 0x445b), (0x626a, 0x4557), (0x5b6d, 0x456e),
       (0x5e39, 0x4573), (0x6d6e, 0x4676), (0x6a24, 0x4768), (0x5b58, 0x4930), (0x5056, 0x4b79),
       (0x692e, 0x4c79), (0x6446, 0x4f36)]


def swap(j, t):
    for a, b in t:
        if j == a: return b
        if j == b: return a
    return j


def rom_of_jis(j):
    """JIS X 0208 の区点（0x2121..）→ 漢字 ROM の位置"""
    return swap(swap(j, J90), J78)


def vram_word(p):
    """漢字 ROM の位置 → テキスト VRAM の左半分の語（右半分は | 0x80）"""
    return ((p & 0xff) << 8) | ((p >> 8) - 0x20)


def _names(hi, lo):
    """区点 1 つの読み（euc_jp と cp932）"""
    out = set()
    try:
        out.add(bytes([hi | 0x80, lo | 0x80]).decode('euc_jp'))
    except UnicodeDecodeError:
        pass
    s1 = (hi + 1) // 2 + (0x70 if hi <= 0x5e else 0xb0)
    s2 = lo + (0x1f if hi % 2 else 0x7e)
    if hi % 2 and lo >= 0x60:
        s2 += 1
    try:
        out.add(bytes([s1, s2]).decode('cp932'))
    except UnicodeDecodeError:
        pass
    return {u for u in out if len(u) == 1 and 0x7f < ord(u) <= 0xffff}


_ROM = None


def rom_table():
    """{Unicode: 漢字 ROM の位置}"""
    global _ROM
    if _ROM is None:
        _ROM = {}
        for hi in range(0x21, 0x7f):
            for lo in range(0x21, 0x7f):
                for u in _names(hi, lo):
                    _ROM.setdefault(ord(u), rom_of_jis((hi << 8) | lo))
        _ROM[0x2014] = _ROM[0x2015]          # — → ―
    return _ROM


def rom_of(ch):
    """全角 1 字 → 漢字 ROM の位置（引けなければ KeyError）"""
    return rom_table()[ord(ch)]


def main():
    t = rom_table()
    need = set()
    # ★`--work 名前` を繰り返せる（既定 zork1）。作品の訳・語彙の字がこの漢字 ROM に全部あるかを、パックを作る前に見る
    works = [sys.argv[i + 1] for i, a in enumerate(sys.argv) if a == '--work'] or ['zork1']
    for f in [f'{w}-{k}.json' for w in works for k in ('ja', 'cmd')]:
        s = json.dumps(json.load(open(os.path.join(HERE, '..', 'assets', f), encoding='utf-8')),
                       ensure_ascii=False)
        need |= {c for c in s if ord(c) > 0x7f}
    miss = sorted(c for c in need if ord(c) not in t)
    if miss:
        print('引けない字:', ' '.join(f'{c}(U+{ord(c):04X})' for c in miss), file=sys.stderr)
        sys.exit(1)
    keys = sorted(t)
    rows = lambda vs: ''.join('    ' + ', '.join(f'0x{v:04X}' for v in vs[i:i + 12]) + ',\n'
                              for i in range(0, len(vs), 12))
    with open(os.path.join(HERE, 'pc98_jis.c'), 'w', encoding='utf-8') as f:
        f.write('/* pc98_jis.py が生成。手で編集しない。\n'
                ' * UTF-16 → PC-98 テキスト VRAM の語（全角の左半分。右半分は | 0x80）。Unicode 順 */\n'
                '#include "pc98_jis.h"\n\n'
                'const unsigned short pc98_jis_u[PC98_JIS_N] = {\n' + rows(keys) + '};\n\n'
                'const unsigned short pc98_jis_v[PC98_JIS_N] = {\n'
                + rows([vram_word(t[k]) for k in keys]) + '};\n')
    with open(os.path.join(HERE, 'pc98_jis.h'), 'w', encoding='utf-8') as f:
        f.write('/* pc98_jis.py が生成。手で編集しない */\n#ifndef PC98_JIS_H\n#define PC98_JIS_H\n'
                f'enum {{ PC98_JIS_N = {len(keys)} }};\n'
                'extern const unsigned short pc98_jis_u[PC98_JIS_N];\n'
                'extern const unsigned short pc98_jis_v[PC98_JIS_N];\n#endif\n')
    print(f'pc98_jis.c: {len(keys)} 字（訳・語彙の {len(need)} 字はすべて引ける）')


if __name__ == '__main__':
    main()
