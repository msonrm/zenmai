#!/usr/bin/env python3
"""部屋ごとの曲（アンビエントの見本）→ pc98-music/{FIELD,HOUSE,WATER,TEMPLE,DEEP}.MML。

★「曲をちゃんと作る」のではなく、**部屋が替わると音色と速さが変わって区別が付く**ことを見せる見本
  （msonrm・2026-09-30 の「アンビエントぽい、メロディともいえないようなのを軽く」）。
  だから旋律は**決まった種から組む音階上の散歩**（乱数の種は曲ごとに固定 = 毎回同じ曲になる）。
    FIELD   外（家のまわり・森）   フルート + 弦・ゆっくり・長調の五音
    HOUSE   家の中                 チェンバロが爪弾く・短調の五音・少し速い
    WATER   ダム・川・湖           ベルの流れる 8 分・リディア・エコー
    TEMPLE  神殿・古い部屋         リード + 弦の空虚 5 度・フリギア・とても遅い
    DEEP    洞窟・迷路・鉱山       低い音がぽつり・ドローン・いちばん遅い（★INI の既定）
★どの部屋がどの曲かは `ZORK1.INI`（手で書く設定・部屋名は英語の状態行）。
★MML → `.M` は `node mc98.js pc98-music/*.MML`（MC.EXE = build-mc.sh）。
使い方: python3 gen_ambient_mml.py
"""
import random
from pathlib import Path

HERE = Path(__file__).parent
OUT = HERE / 'pc98-music'
NAMES = ['c', 'c+', 'd', 'd+', 'e', 'f', 'f+', 'g', 'g+', 'a', 'a+', 'b']
BAR = 96                               # 全音符 = 96 クロック（4/4 の 1 小節）
BARS = 16
LOUD = 12                              # 搬送波 TL の足し分（gen_canon_mml と同じ考え方）

# 音色: (ALG, FB, [OP1..OP4 = (DT, ML, TL, KS, AR, DR, SR, SL, RR)])。★ALG 4 = OP2・OP4 が音を出す
PATCH = {
    'flute':  (4, 3, [(0, 1, 38, 0, 22, 0, 0, 0, 6), (0, 1, 18, 0, 20, 2, 0, 1, 6),
                      (0, 2, 60, 0, 24, 0, 0, 0, 6), (1, 2, 38, 0, 18, 2, 0, 1, 6)]),
    'harpsi': (4, 6, [(0, 4, 32, 2, 31, 11, 4, 6, 8), (0, 1, 14, 1, 31, 7, 3, 4, 8),
                      (3, 1, 26, 2, 31, 12, 5, 7, 8), (7, 2, 22, 1, 31, 7, 3, 4, 8)]),
    'strings': (4, 7, [(0, 1, 24, 0, 18, 0, 0, 0, 7), (3, 1, 16, 0, 16, 1, 0, 1, 6),
                       (0, 1, 26, 0, 18, 0, 0, 0, 7), (7, 1, 18, 0, 16, 1, 0, 1, 6)]),
    'reed':   (4, 5, [(0, 1, 28, 0, 24, 4, 0, 2, 7), (0, 1, 14, 0, 22, 2, 0, 1, 7),
                      (0, 3, 40, 0, 26, 6, 0, 3, 7), (3, 1, 24, 0, 22, 2, 0, 1, 7)]),
    # ★やわらかい pad: 帰還（FB）を絞り、2 組を上下にずらさない。ストリングスの FB 7 は鋸歯を狙った音で、
    #   ずっと鳴らすと「ざらざらした雑音」に聞こえた（msonrm・2026-09-30）
    'pad':    (4, 1, [(0, 1, 44, 0, 14, 0, 0, 0, 5), (0, 1, 20, 0, 14, 2, 0, 1, 5),
                      (0, 2, 56, 0, 14, 0, 0, 0, 5), (0, 1, 30, 0, 14, 2, 0, 1, 5)]),
    'bell':   (4, 0, [(0, 14, 52, 2, 31, 14, 6, 15, 8), (3, 1, 14, 1, 31, 5, 2, 3, 6),
                      (0, 1, 30, 1, 31, 8, 3, 6, 6), (7, 1, 16, 1, 31, 5, 2, 3, 6)]),
    'bass':   (4, 6, [(0, 0, 24, 0, 31, 8, 2, 4, 8), (0, 1, 12, 0, 31, 4, 1, 2, 8),
                      (0, 1, 22, 0, 31, 10, 3, 5, 8), (3, 1, 18, 0, 31, 4, 1, 2, 8)]),
}

# 曲の表。roots = 2 小節ごとのコードの根（root からの半音）・lead = 旋律の並び方
SONGS = {
    'FIELD': dict(title='Field', tempo=40, root=60, scale=[0, 2, 4, 7, 9], roots=[0, -3, -7, -5], seed=11,
                  lead='flute', pad='pad', walk=(1, 2), rest=0.15, lens=[96, 48, 72, 48], bell=True, oct_lead=0),
    'HOUSE': dict(title='House', tempo=52, root=57, scale=[0, 3, 5, 7, 10], roots=[0, -4, 3, -2], seed=23,
                  lead='harpsi', pad='pad', walk=(1, 2), rest=0.3, lens=[24, 48, 24, 12, 12, 48], bell=True, oct_lead=0),
    'WATER': dict(title='Water', tempo=50, root=62, scale=[0, 2, 4, 6, 7, 9, 11], roots=[0, 4, -1, 2], seed=37,
                  lead='bell', pad='pad', walk=(1, 1), rest=0.1, lens=[12], bell=False, oct_lead=0, echo=True),
    'TEMPLE': dict(title='Temple', tempo=30, root=50, scale=[0, 1, 3, 5, 7, 8, 10], roots=[0, 0, 5, 1], seed=41,
                   lead='reed', pad='pad', walk=(1, 1), rest=0.25, lens=[96, 144, 96, 48], bell=True, oct_lead=1),
    'DEEP': dict(title='Deep', tempo=26, root=40, scale=[0, 3, 5, 7, 10], roots=[0, 0, -2, 3], seed=53,
                 lead='bass', pad='pad', walk=(1, 3), rest=0.55, lens=[96, 192, 96], bell=False, oct_lead=1, drone=True),
}


def voice(num, name):
    alg, fb, ops = PATCH[name]
    rows = [f'@ {num}  {alg} {fb}  ={name}']
    for k, (dt, ml, tl, ks, ar, dr, sr, sl, rr) in enumerate(ops):
        if k in (1, 3):
            tl = max(0, tl - LOUD)
        rows.append(f'  {ar:2d} {dr:2d} {sr:2d} {rr:2d} {sl:2d} {tl:3d} {ks:2d} {ml:2d} {dt:2d}  0')
    return '\n'.join(rows)


def chunks(clk):
    out = []
    while clk > 255:
        out.append(200)
        clk -= 200
    return out + [clk]


def notes(seq):
    """[(midi または None, クロック)] → MML の語の列"""
    out, cur = [], None
    for m, c in seq:
        if m is None:
            out += [f'r%{x}' for x in chunks(c)]
            continue
        o = m // 12 - 1
        if o != cur:
            out.append(f'o{o}')
            cur = o
        cl = chunks(c)
        for j, x in enumerate(cl):
            out.append(f'{NAMES[m % 12]}%{x}' + ('&' if j < len(cl) - 1 else ''))
    return out


def wrap(tokens, width=72):
    lines, cur = [], ''
    for t in tokens:
        if len(cur) + len(t) + 1 > width:
            lines.append(cur)
            cur = ''
        cur += (' ' if cur else '') + t
    if cur:
        lines.append(cur)
    return lines


def build(name, s):
    rnd = random.Random(s['seed'])
    scale = s['scale']
    span = [s['root'] + 12 * (s['oct_lead'] + o) + d for o in (0, 1) for d in scale]   # 旋律の音（2 オクターブ）
    total = BAR * BARS
    # 旋律: 音階の上の散歩（休みを混ぜる）
    idx = rnd.randrange(len(span) // 2, len(span))
    lead, t, k = [], 0, 0
    while t < total:
        c = min(s['lens'][k % len(s['lens'])], total - t)
        k += 1
        if rnd.random() < s['rest']:
            lead.append((None, c))
        else:
            idx = max(0, min(len(span) - 1, idx + rnd.choice([-1, 1]) * rnd.randint(*s['walk'])))
            lead.append((span[idx], c))
        t += c
    # 和音: 2 小節ごとに根だけ（★5 度の C パートは外した = ざらつきの元・msonrm 2026-09-30）
    pad = []
    for b in range(0, BARS, 2):
        r = s['root'] - 12 + s['roots'][(b // 2) % len(s['roots'])]
        pad.append((r, BAR * 2))
    # 鐘（SSG）: 1 小節に 1 回ぐらい、音階の高い音をぽつり
    bell = []
    for b in range(BARS):
        if s.get('bell') and rnd.random() < 0.55:
            pos = rnd.choice([0, 24, 48])
            m = s['root'] + 24 + rnd.choice(scale)
            bell += [(None, pos), (m, 12), (None, BAR - pos - 12)] if pos else [(m, 12), (None, BAR - 12)]
        else:
            bell.append((None, BAR))
    # ドローン（SSG）: 根の音を低く伸ばす
    drone = [(s['root'] - 12 + s['roots'][(b // 2) % len(s['roots'])], BAR * 2) for b in range(0, BARS, 2)] if s.get('drone') else None
    parts = {'A': lead, 'B': pad, 'G': bell}
    if drone:
        parts['H'] = drone
    for p, seq in parts.items():
        assert sum(c for _, c in seq) == total, (name, p, sum(c for _, c in seq))
    lines = [
        f'; ★gen_ambient_mml.py が生成。手で編集しない（旋律は決まった種から組む音階上の散歩）',
        f'#Title\t\t{s["title"]}', '#Composer\tZenmai', '#Memo\t\tAmbient sample for room music.',
        '#Filename\t.M', f'#FFFile\t\t{name}.FF', f'#Tempo\t\t{s["tempo"]}', '',
        voice(0, s['lead']), voice(1, s['pad']),
        '',
    ]
    echo = 'W12,-6,1 ' if s.get('echo') else ''
    lines.append(f'A\t@0 V127 p2 q2 L {echo}')
    lines += [f'A\t{x}' for x in wrap(notes(lead))]
    lines.append('B\t@1 V118 p1 L')
    lines += [f'B\t{x}' for x in wrap(notes(pad))]
    lines.append('G\t@6 v14 L')
    lines += [f'G\t{x}' for x in wrap(notes(bell))]
    if drone:
        lines.append('H\t@0 v6 L')
        lines += [f'H\t{x}' for x in wrap(notes(drone))]
    text = '\n'.join(lines) + '\n'
    (OUT / f'{name}.MML').write_bytes(text.replace('\n', '\r\n').encode('cp932'))


def main():
    OUT.mkdir(exist_ok=True)
    for name, s in SONGS.items():
        build(name, s)
    print('ambient:', ' '.join(SONGS), '（各 16 小節）')


if __name__ == '__main__':
    main()
