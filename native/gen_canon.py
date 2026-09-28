#!/usr/bin/env python3
"""起動画面の曲 —— J. S. Bach『音楽の捧げもの』BWV 1079 の「2 声のカノン（Quaerendo invenietis・謎カノン）」→ canon_data.c/h。

★原曲は 1747 年の作品で著作権は切れている。音は下の DUX に**自分で書き起こした**もの
  （参照: Kompy 浄書の解 6a『Canon 1 a 2 Quaerendo invenietis』= IMSLP の Musop.pdf 35 頁。
  Wikimedia Commons に Internet Archive 経由の写しがある）。浄書の版面は使っていない。
★謎カノンは 1 本の旋律だけが書かれていて、後の声は「鏡に映して読む」。この解（6a）では
  **後の声 = 先の声を 10 拍（四分音符）遅らせ、上下を反転したもの**（MIDI の番号で和がいつも 110。
  C4 ↔ D3・D4 ↔ C3）。だから書き起こすのは先の声だけで、後の声はここで計算する。
  ★ただし調号ごと鏡に映すので半音まで厳密な反転にはならない所がある —— 15 小節目の二分音符
  （先の声 F3）は、6a も 6b も後の声を A♭3 と解いている（厳密な反転なら A♮3）。その 1 音だけ例外を持つ。
  ★検算: 後の声を計算したものは、浄書のチェロ譜（4〜24 小節）と音名・臨時記号まで一致した（2026-09-29）。
★形: 1〜19 小節 →（反復）5〜19 小節 → 20〜24 小節。最後は 2 声とも C で伸ばして終わる（6a のとおり）。
★トリル（浄書の tr = 先の声の 8・23 小節の D4、後の声の 11 小節の C3）は**上の音から**入れる（バロックの作法）。
  上の音は調の中の隣: 先の声 D4 → E♭4・後の声 C3 → D3。32 分音符で上・下を 3 回くり返し、最後の下を伸ばす。
  ★トリルの 2 音目からは**弾き直さない**（レガート = 高さだけ変える）。弾き直すと 1 音ごとに立ち上がりの音が付く。

使い方: python3 gen_canon.py
"""
from pathlib import Path

HERE = Path(__file__).parent
TR = ('Eb4', 'D3')                     # D4 のトリル（後の声では C3 のトリル）

# 先の声（6a のヴィオラ）。(音名, 長さ[四分音符], 後の声の例外, トリル) 。音名 None = 休符。1 小節 = 4 拍（2/2）
# トリル = (先の声の上の音, 後の声の上の音)。後の声のトリルは反転した同じ音に付く
DUX = {
    1: [(None, 3), ('C4', .5), ('D4', .5)],
    2: [('Eb4', 1), ('E4', 1), ('F4', 1), ('F#4', 1)],
    3: [('G4', 2), ('Ab4', 2)],
    4: [('B3', 2), (None, 1), ('G4', 1)],
    5: [('F#4', 1), ('F4', 2), ('E4', 1)],
    6: [('Eb4', 1), ('D4', 2), ('C4', 1)],
    7: [('B3', 1), ('G3', 1), ('C4', 1), ('F4', 1)],
    8: [('Eb4', 2), ('D4', 1, None, TR), ('C4', .5), ('D4', .5)],
    9: [('C4', .5), ('C5', .5), ('Bb4', .5), ('Ab4', .5), ('G4', .5), ('F4', .5), ('Eb4', .5), ('G4', .5)],
    10: [('F4', .5), ('G4', .5), ('F4', .5), ('Eb4', .5), ('D4', .5), ('Eb4', .5), ('F4', .5), ('D4', .5)],
    11: [('Eb4', .5), ('C4', .5), ('A3', .5), ('C4', .5), ('F#3', 1), ('G3', .5), ('A3', .5)],
    12: [('B3', .5), ('C4', .5), ('D4', .5), ('Eb4', .5), ('F4', 1), ('Eb4', .5), ('D4', .5)],
    13: [('Eb4', .5), ('D4', .5), ('C4', .5), ('Eb4', .5), ('D4', 1), ('C4', .5), ('B3', .5)],
    14: [('C4', 1), ('D4', 1), ('G3', 2)],
    15: [('F3', 2, 'Ab3'), (None, .5), ('Eb3', .5), ('F#3', .5), ('G3', .5)],
    16: [(None, .5), ('A3', .5), ('B3', .5), ('C4', .5), (None, .5), ('B3', .5), ('C4', .5), ('D4', .5)],
    17: [('Eb4', 1), ('E4', 1), ('F4', 1), ('F#4', 1)],
    18: [('G4', 2), ('Ab4', 2)],
    19: [('B3', 2), (None, 1), ('G4', 1)],
    20: [('F#4', 1), ('F4', 2), ('E4', 1)],
    21: [('Eb4', 1), ('D4', 2), ('C4', 1)],
    22: [('B3', 1), ('G3', 1), ('C4', 1), ('F4', 1)],
    23: [('Eb4', 2), ('D4', 1, None, TR), ('C4', .5), ('D4', .5)],
    24: [('C4', 4)],
}
FORM = list(range(1, 20)) + list(range(5, 20)) + list(range(20, 25))
DELAY, AXIS = 10, 110                  # 後の声: 10 拍遅れ・和 110
COMES_END = ('C3', 5)                  # 後の声は最後の 5 拍を C3 で伸ばす（23 小節 4 拍目から）
TAIL = 4                               # 繰り返す前の間（拍）

STEP = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}


def midi(n):
    return 12 * (int(n[-1]) + 1) + STEP[n[0]] + (1 if '#' in n else -1 if 'b' in n[1:] else 0)


LEGATO = 0x80                          # 音の番号の bit7 = 弾き直さない（高さだけ変える）
UNIT = 8                               # 四分音符 = 32 分音符 8 つ


def trill(main, upper, d):
    """上の音から 32 分音符で 3 往復し、最後の主音を伸ばす。2 音目からはレガート"""
    out = []
    for k in range(6):
        out.append([(upper if k % 2 == 0 else main) | (LEGATO if k else 0), 1])
    out[-1][1] = d - 5
    return out


def main():
    for b, ev in DUX.items():
        assert sum(e[1] for e in ev) == 4, f'{b} 小節の拍が 4 でない'
    # 先の声を形どおりに並べる（長さは 32 分音符の数）。e = (始まり, 長さ, 音, 後の声の例外, トリル)
    dux, t = [], 0
    for b in FORM:
        for e in DUX[b]:
            d = int(e[1] * UNIT)
            alt = midi(e[2]) if len(e) > 2 and e[2] else None
            tr = e[3] if len(e) > 3 else None
            dux.append((t, d, None if e[0] is None else midi(e[0]), alt, tr))
            t += d
    total = t
    # 後の声 = 反転して遅らせたもの。最後は C3 で伸ばす
    end = total - COMES_END[1] * UNIT
    comes = [(0, DELAY * UNIT, None, None)]
    for (s0, d, m, alt, tr) in dux:
        s0 += DELAY * UNIT
        if s0 >= end:
            break
        d = min(d, end - s0)
        comes.append((s0, d, None if m is None else alt if alt else AXIS - m, midi(tr[1]) if tr else None))
    comes.append((end, total - end, midi(COMES_END[0]), None))
    dux = [(s0, d, m, midi(tr[0]) if tr else None) for (s0, d, m, _, tr) in dux]

    def pack(v):
        # (音, 32 分音符の数) —— 音 0 = 休符。続く休符はつなぐ
        out = []
        for (_, d, m, up) in v:
            if up:
                out += trill(m, up, d)
                continue
            n = m or 0
            if out and n == 0 and out[-1][0] == 0:
                out[-1][1] += d
            else:
                out.append([n, d])
        out.append([0, TAIL * UNIT])
        for n, d in out:
            assert 0 < d < 256, d
        return out

    v = [pack(dux), pack(comes)]
    total = sum(d for _, d in v[0])
    assert total == sum(d for _, d in v[1])
    with open(HERE / 'canon_data.h', 'w') as f:
        f.write('/* gen_canon.py が生成。手で編集しない */\n#ifndef CANON_DATA_H\n#define CANON_DATA_H\n'
                '#include <stdint.h>\n')
        f.write(f'enum {{ CANON_N0 = {len(v[0])}, CANON_N1 = {len(v[1])}, CANON_UNITS = {total}, CANON_LEGATO = 0x80 }};\n')
        f.write('/* 1 音 = { MIDI の音の番号（0 = 休符・bit7 = 弾き直さない）, 長さ（32 分音符の数）} */\n')
        f.write('extern const uint8_t canon_v0[CANON_N0][2];   /* 先の声 */\n')
        f.write('extern const uint8_t canon_v1[CANON_N1][2];   /* 後の声 */\n#endif\n')
    with open(HERE / 'canon_data.c', 'w') as f:
        f.write('/* gen_canon.py が生成。手で編集しない */\n#include "canon_data.h"\n')
        for i, vv in enumerate(v):
            f.write(f'const uint8_t canon_v{i}[CANON_N{i}][2] = {{\n')
            for k in range(0, len(vv), 10):
                f.write('    ' + ', '.join(f'{{{n},{d}}}' for n, d in vv[k:k + 10]) + ',\n')
            f.write('};\n')
    print(f'canon_data: 先の声 {len(v[0])} 音 / 後の声 {len(v[1])} 音 / {total // (UNIT * 4)} 小節ぶん')


if __name__ == '__main__':
    main()
