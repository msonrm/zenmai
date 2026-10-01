#!/usr/bin/env python3
"""配る書庫の README.TXT を、PC-98 時代のフリーソフトの流儀で整える（pack-pc98.sh から呼ぶ）。

  python3 fmt_readme.py pc98-readme.txt 0.5.0-beta 2026.10.01 > README.UTF8

★流儀: 頭に名前・版・作者・日付などを NEC 罫線の枠で囲み、1 行は決め打ちの桁数（全角 = 2 桁。80 桁の画面で
  折り返さずに読めるよう 76 桁まで）。本文は台本（pc98-readme.txt）に 1 段落 1 行で書き、ここで折る。
  - 禁則（行頭に 、。）」 など・行末に 「（ など）と、英数字の語を割らないことを守る
  - 字下げ（行頭の空白）と、箇条書きの印（- ・ ★ ※ 1. など）はぶら下げて折る
  - 行頭が `|` の行はそのまま（表や罫線用）。`|` の次の 1 字（空白）は取る
  - `@VER@` `@DATE@` を置き換える。1 行が 76 桁を超える `|` の行があれば止まる
"""
import re, sys, unicodedata

W = 76
ERR = []
HEAD_NG = set('、。，．）」』】〕〉》！？・ー:;,.!?)]}ぁぃぅぇぉっゃゅょァィゥェォッャュョ々')
END_NG = set('（「『【〔〈《([{')


def cw(c):
    """1 字の桁数。ASCII と半角カナは 1、ほかは全角として 2（PC-98 の漢字 ROM の字はみな 2 桁）"""
    o = ord(c)
    if o < 0x80 or 0xFF61 <= o <= 0xFF9F:
        return 1
    return 2


def width(s):
    return sum(cw(c) for c in s)


def is_word(c):
    return c.isascii() and (c.isalnum() or c in "_'-./:=#@")


def wrap(text, first, rest):
    out, cur, cw_ = [], first, width(first)
    i, n = 0, len(text)
    start_of_line = True
    while i < n:
        j, w = i, cw_
        while j < n and w + cw(text[j]) <= W:
            w += cw(text[j])
            j += 1
        if j >= n:
            out.append(cur + text[i:j].rstrip())
            break
        # 折る位置 j（text[j] が次の行の頭になる）。禁則と英数字の語を守って手前へ戻す
        k = j
        while k > i + 1 and (text[k] in HEAD_NG or text[k - 1] in END_NG or (is_word(text[k]) and is_word(text[k - 1]))):
            k -= 1
        if k <= i + 1:
            k = j
        out.append(cur + text[i:k].rstrip())
        while k < n and text[k] == ' ':
            k += 1
        i = k
        cur, cw_ = rest, width(rest)
    return out


def body_line(line):
    if line.startswith('|'):
        s = line[1:]
        if s.startswith(' '):
            s = s[1:]
        if width(s) > W:
            print(f'{width(s)} 桁ある（{W} まで）: {s}', file=sys.stderr)
            ERR.append(s)
        return [s]
    if not line.strip():
        return ['']
    m = re.match(r'^(\s*)((?:[-・※★]|\d+\.)\s*)?', line)
    indent, mark = m.group(1), m.group(2) or ''
    text = line[m.end():]
    first = indent + mark
    rest = indent + ' ' * width(mark)
    return wrap(text, first, rest)


def box(rows):
    """NEC 罫線（JIS X 0208 の罫線）の枠。内側は 72 桁（─ 36 個）"""
    inner = 72
    top = '┌' + '─' * (inner // 2) + '┐'
    mid = '├' + '─' * (inner // 2) + '┤'
    bot = '└' + '─' * (inner // 2) + '┘'
    out = [top]
    for r in rows:
        if r is None:
            out.append(mid)
            continue
        left, right = r if isinstance(r, tuple) else (r, '')
        pad = inner - 2 - width(left) - width(right)
        if pad < 1:
            sys.exit(f'枠に入らない: {left}{right}')
        out.append('│ ' + left + ' ' * (pad) + right + ' │')
    out.append(bot)
    return out


def main():
    src, ver, date = sys.argv[1:4]
    lines = [l.rstrip('\n') for l in open(src, encoding='utf-8')]
    head, body = [], []
    in_head = True
    for l in lines:
        if in_head and l == '@BODY@':
            in_head = False
            continue
        (head if in_head else body).append(l)
    rows = []
    for l in head:
        l = l.replace('@VER@', ver).replace('@DATE@', date)
        if l == '---':
            rows.append(None)
        elif '\t' in l:                      # 題: 左と右
            a, b = l.split('\t', 1)
            rows.append((a, b))
        elif '|' in l:                       # 項目: 名札（8 桁に揃える）と値
            a, b = l.split('|', 1)
            rows.append(a + ' ' * (8 - width(a)) + b)
        else:
            rows.append(l)
    out = box(rows)
    for l in body:
        l = l.replace('@VER@', ver).replace('@DATE@', date)
        out += body_line(l)
    for l in out:
        if width(l) > W:
            sys.exit(f'{width(l)} 桁: {l}')
    if ERR:
        sys.exit(1)
    sys.stdout.write('\n'.join(out) + '\n')


main()
