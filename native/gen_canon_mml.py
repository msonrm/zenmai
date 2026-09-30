#!/usr/bin/env python3
"""起動画面の曲 → PMD の MML（pc98-music/CANON.MML）。

★音（2 声のカノン）は gen_canon.py の build() が作る（謎カノンの解 6a・書き起こしの根拠はそちらの頭書き）。
  ここはそれを **PMD の MML に組む**だけ:
    A  FM1  先の声  ★リード（明るい鋸歯ぎみのシンセ）
    B  FM2  後の声  ★太いシンセ（先の声と聞き分けられる音色）
    G  SSG1 先の声を重ねる（速く減衰する「ぽろん」= 1989 年の PC-98 ゲームの層）
    H  SSG2 後の声を重ねる
    K/R SSG ドラム（★26K でも鳴る。86 ボードなら PMD がリズム音源も同時に鳴らす）
★長さは内部クロックで直に書く（全音符 = 96 → 32 分音符 = 3）。★トリルの 2 音目からは `&`（弾き直さず高さだけ）。
★曲は頭へ戻る（`L`）。全パートの長さは同じ（assert で確かめる）。
★MML → `.M` は KAJA 氏の MC.EXE（PMD の作者が 2019 に公開した自由ソースから建てる・native/build-mc.sh）。
  `.M` を作り直したら `pc98-music/CANON.M` も差し替える（sh native/mc98.sh）

使い方: python3 gen_canon_mml.py
"""
from pathlib import Path

from gen_canon import build, LEGATO

HERE = Path(__file__).parent
OUT = HERE / 'pc98-music' / 'CANON.MML'
NAMES = ['c', 'c+', 'd', 'd+', 'e', 'f', 'f+', 'g', 'g+', 'a', 'a+', 'b']
CLK = 3                                # 32 分音符 = 3 クロック（全音符 96）

# 音色（@ 番号 ALG FB / OP1〜OP4 各 AR DR SR RR SL TL KS ML DT AMS）。★ALG 4 = (OP1→OP2) + (OP3→OP4)。OP2・OP4 が音を出す
VOICES = '''\
@ 0  4 6  =ZUN-lead
  31  9  5  5  4  28  0  1   0  0
  31  5  3  6  3   0  1  1   0  0
  31  9  5  5  4  34  0  2   3  0
  31  5  3  6  3   0  1  1  -3  0
@ 1  4 6  =Synth-bass
  31  8  2  8  4  24  0  0   0  0
  31  4  1  8  2   0  0  1   0  0
  31 10  3  8  5  22  0  1   0  0
  31  4  1  8  2   0  0  1   3  0
'''

# ドラム（K = 順番・R = パターン）。★@ の番号は足し算: 1 バス・2 スネア・128 ハイハット閉・512 クラッシュ・4/8/16 タム
DRUMS = {
    # 8 分で数える 1 小節: バス・ハイハット・スネア……
    0: 'l8 @129c @128c @130c @128c @129c @129c @130c @128c',
    # 4 小節目のフィル: 最後の拍をタムで駆け下りる
    1: 'l8 @129c @128c @130c @128c @130c @128c l16 @16c @8c @4c @1c',
    # 終わりの 1 小節: バス + クラッシュを伸ばす
    2: '@513c1',
    # 休み 1 小節
    3: 'r1',
}


def chunks(clk):
    """1 音符に書ける長さは 255 クロックまで。超えるぶんは割る"""
    out = []
    while clk > 255:
        out.append(200)
        clk -= 200
    return out + [clk]


def part(events, transpose=0):
    """[(音, 32 分音符の数)] → MML の語の列（オクターブは変わったときだけ o で切る）。
    長い音は `&`（タイ）でつなぎ、トリルの 2 音目からも `&`（弾き直さず高さだけ変える）"""
    out, cur = [], None
    for k, (b, d) in enumerate(events):
        cl = chunks(d * CLK)
        if b == 0:
            out += [f'r%{c}' for c in cl]
            continue
        m = (b & 0x7F) + transpose
        tie_next = k + 1 < len(events) and (events[k + 1][0] & LEGATO) != 0
        o = m // 12 - 1                # C4 = 60 → o4
        if o != cur:
            out.append(f'o{o}')
            cur = o
        for j, c in enumerate(cl):
            last = j == len(cl) - 1
            out.append(f'{NAMES[m % 12]}%{c}' + ('&' if (not last or tie_next) else ''))
    return out


def wrap(tokens, width=70):
    lines, cur = [], ''
    for t in tokens:
        if len(cur) + len(t) + 1 > width:
            lines.append(cur)
            cur = ''
        cur += (' ' if cur else '') + t
    if cur:
        lines.append(cur)
    return lines


def main():
    v, total = build()
    bars = total // 32
    lines = [
        ';★gen_canon_mml.py が生成。手で編集しない（音は gen_canon.py・組み方はそちら）',
        '#Title\t\tQuaerendo invenietis (BWV 1079)',
        '#Composer\tJ. S. Bach',
        '#Arranger\tZenmai',
        '#Memo\t\tCanon a 2 - solution 6a. mirrored, 10 beats late.',
        '#Filename\t.M',
        '#FFFile\t\tCANON.FF',
        '#Tempo\t\t66',
        '',
        VOICES,
    ]
    # FM
    for name, ev, voice, vol, pan in (('A', v[0], 0, 127, 2), ('B', v[1], 1, 127, 1)):
        lines.append(f'{name}\t@{voice} V{vol} p{pan} q1 L')
        lines += [f'{name}\t{s}' for s in wrap(part(ev))]
    # SSG（同じ音を重ねる）
    for name, ev in (('G', v[0]), ('H', v[1])):
        lines.append(f'{name}\t@4 v13 L')
        lines += [f'{name}\t{s}' for s in wrap(part(ev))]
    # ドラム
    for n, mml in DRUMS.items():
        lines.append(f'R{n}\t{mml}')
    # 順番: 1 小節目は休み・次の 4 小節ずつ（基本 3 + フィル 1）を 9 回・最後の 2 小節は基本 + クラッシュ・40 小節目は休み
    seq = 'R3 [R0R0R0R1]9 R0 R2 R3'
    assert 1 + 9 * 4 + 1 + 1 + 1 == bars
    lines.append(f'K\tL {seq}')
    OUT.parent.mkdir(exist_ok=True)
    OUT.write_bytes(('\n'.join(lines) + '\n').replace('\n', '\r\n').encode('cp932'))   # ★DOS の道具なので CRLF
    print(f'CANON.MML: {bars} 小節 / A {len(v[0])} 音・B {len(v[1])} 音')


if __name__ == '__main__':
    main()
