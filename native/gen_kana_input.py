#!/usr/bin/env python3
"""キーボードのかな入力の表を作る → kana_input_data.{c,h}（手で編集しない）。

- ローマ字: vendor/mozc/romanji-hiragana.tsv（Mozc の表・BSD 3-Clause）をそのまま C の表に
- 半角カナ（PC-98 のカナキー。JIS X 0201 の 0xA1〜0xDF）→ ひらがな
- 濁点・半濁点の合成（ｶﾞ のように後から来る ﾞ ﾟ を前の字にかぶせる）
- ★ゔ（U+3094）は JIS X 0208 に無く漢字 ROM で出せないので、ヴ（U+30F4）に寄せる

★出力の字はすべて PC-98 の漢字 ROM で引けること（pc98_jis.py）を確かめる。

    python3 gen_kana_input.py
"""
import os, sys, unicodedata

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import pc98_jis


def u16(s):
    return [ord(c) for c in s]


def main():
    rom = []
    for line in open(os.path.join(HERE, 'vendor', 'mozc', 'romanji-hiragana.tsv'), encoding='utf-8'):
        line = line.rstrip('\n')
        if not line or line.startswith('#'):
            continue
        cols = line.split('\t')
        inp, out = cols[0], cols[1]
        pend = cols[2] if len(cols) > 2 else ''
        assert all(0x20 < ord(c) < 0x7f for c in inp + pend), line
        assert len(inp) <= 4 and len(out) <= 3 and len(pend) <= 2, line
        rom.append((inp, out.replace('ゔ', 'ヴ'), pend))

    # 半角カナ → ひらがな（カタカナは 0x60 引く。ー・句読点・括弧はそのまま）
    han = []
    for b in range(0xA1, 0xE0):
        f = unicodedata.normalize('NFKC', bytes([b]).decode('cp932'))
        if 0x30A1 <= ord(f) <= 0x30F6:
            f = chr(ord(f) - 0x60)
        han.append(f)

    # 濁点・半濁点の合成: (もとの字, 濁点 1 / 半濁点 2) → 合成した字
    dak = []
    for u in range(0x3041, 0x3097):
        for mark, k in (('゙', 1), ('゚', 2)):
            c = unicodedata.normalize('NFC', chr(u) + mark)
            if len(c) == 1:
                dak.append((u, k, ord(c.replace('ゔ', 'ヴ'))))

    need = {c for _, o, _ in rom for c in o} | set(han) | {chr(c) for *_, c in dak}
    need = {c for c in need if ord(c) > 0x7f}
    miss = [c for c in need if ord(c) not in pc98_jis.rom_table()]
    # ★濁点の単独形（゛゜）は漢字 ROM にあるが、合成用の結合文字は無いので半角カナの ﾞ ﾟ は別扱い
    miss = [c for c in miss if c not in '゙゚']
    if miss:
        print('漢字 ROM で引けない字:', ' '.join(f'{c}(U+{ord(c):04X})' for c in miss), file=sys.stderr)
        sys.exit(1)

    with open(os.path.join(HERE, 'kana_input_data.c'), 'w', encoding='utf-8') as f:
        f.write('/* gen_kana_input.py が生成。手で編集しない。\n'
                ' * ★ローマ字の表は Mozc の romanji-hiragana.tsv から作った（BSD 3-Clause・\n'
                ' *   Copyright 2010-2018, Google Inc.）。出どころと全文は vendor/mozc/。 */\n'
                '#include "kana_input_data.h"\n\n'
                'const KiRomaji ki_romaji[KI_ROMAJI_N] = {\n')
        for inp, out, pend in rom:
            o = u16(out) + [0] * (3 - len(out))
            esc = inp.replace('\\', '\\\\').replace('"', '\\"')
            p = '"' + pend.replace('\\', '\\\\').replace('"', '\\"') + '"'
            f.write(f'    {{ "{esc}", {{ {", ".join(f"0x{c:04X}" for c in o)} }}, {p} }},\n')
        f.write('};\n\n/* 半角カナ 0xA1〜0xDE → ひらがな（ﾞ ﾟ は合成するので 0 ではなく単独形を置く） */\n'
                'const unsigned short ki_hankana[0xE0 - 0xA1] = {\n')
        vals = []
        for c in han:
            if c == '゙': c = '゛'
            if c == '゚': c = '゜'
            vals.append(ord(c))
        for i in range(0, len(vals), 12):
            f.write('    ' + ', '.join(f'0x{v:04X}' for v in vals[i:i + 12]) + ',\n')
        f.write('};\n\nconst KiDakuten ki_dakuten[KI_DAKUTEN_N] = {\n')
        for u, k, c in dak:
            f.write(f'    {{ 0x{u:04X}, {k}, 0x{c:04X} }},\n')
        f.write('};\n')
    with open(os.path.join(HERE, 'kana_input_data.h'), 'w', encoding='utf-8') as f:
        f.write('/* gen_kana_input.py が生成。手で編集しない */\n#ifndef KANA_INPUT_DATA_H\n#define KANA_INPUT_DATA_H\n'
                '/* ローマ字 1 行: 打った列 → 出る字（最大 3・0 詰め）と、次へ残す列（"" = 無し。`tch` → っ + `ch`） */\n'
                'typedef struct { const char *in; unsigned short out[3]; const char *pend; } KiRomaji;\n'
                'typedef struct { unsigned short base; unsigned char mark; unsigned short to; } KiDakuten;\n'
                f'enum {{ KI_ROMAJI_N = {len(rom)}, KI_DAKUTEN_N = {len(dak)} }};\n'
                'extern const KiRomaji ki_romaji[KI_ROMAJI_N];\n'
                'extern const unsigned short ki_hankana[0xE0 - 0xA1];\n'
                'extern const KiDakuten ki_dakuten[KI_DAKUTEN_N];\n#endif\n')
    print(f'kana_input_data.c: ローマ字 {len(rom)} 行・半角カナ {len(han)} 字・濁点の合成 {len(dak)} 組')


if __name__ == '__main__':
    main()
