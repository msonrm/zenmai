#!/usr/bin/env python3
"""英文を T9 打鍵台本(sim.py --script 形式)に変換する。

使い方: python3 t9gen.py <開始フィールド> "open mailbox" "read leaflet" ... > out.script
各文字 = 押下 60 + 間 60 フィールド。文字列間に START(確定)と処理待ち 1400 を挟む。
"""
import sys

ROWS = ['1', '2abc', '3def', '4ghi', '5jkl', '6mno', '7pqrs', '8tuv', '9wxyz', '0']
ROW_BTN = ['', 'LEFT', 'UP', 'RIGHT', 'DOWN', 'L1', 'L1+LEFT', 'L1+UP', 'L1+RIGHT', 'L1+DOWN']
VOWEL_BTN = ['R1', 'SQ', 'TRI', 'CIR', 'X']


def keys_for(ch):
    if ch == ' ':
        return 'SEL'
    for row, chars in enumerate(ROWS):
        if ch in chars:
            btn = ROW_BTN[row]
            v = VOWEL_BTN[chars.index(ch)]
            return f'{btn}+{v}' if btn else v
    raise SystemExit(f'T9 に無い文字: {ch!r}')


f = int(sys.argv[1])
print('# t9gen.py が生成')
for phrase in sys.argv[2:]:
    print(f'# --- {phrase} ---')
    for ch in phrase:
        print(f'{f}-{f + 60}:{keys_for(ch)}  # {ch!r}')
        f += 120
    print(f'{f}-{f + 60}:START')   # 窓は広めに(描画で 20 フィールド程度は塞がる)
    f += 3000                       # ターン処理(VM+描画+スクロール)が sim 時間を食う
print(f'# 終了目安: {f}', file=sys.stderr)
