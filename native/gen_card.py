#!/usr/bin/env python3
"""フォーマット済みのメモリーカード像(128KB)を作る / 中身を読む。

sim.py の `--card` に渡す土台。★card.c はフォーマットをしない
(他人のセーブを消す操作をゲームが黙ってやってよいことではない)ので、
検証には「BIOS でフォーマット済み」の状態を用意する必要がある。

  python3 gen_card.py /tmp/card.mcd          # 作る
  python3 gen_card.py /tmp/card.mcd --show   # 中身を読む(ディレクトリとタイトル)
"""
import sys
from pathlib import Path

FRAME = 128
BLK = 64


def xsum(b):
    c = 0
    for x in b:
        c ^= x
    return c


def blank():
    card = bytearray(FRAME * BLK * 16)

    def put(sector, data, fill=0):
        d = bytearray([fill]) * FRAME
        d[:len(data)] = data
        d[0x7F] = xsum(d[:0x7F])
        card[sector * FRAME:(sector + 1) * FRAME] = d

    put(0, b'MC')                                  # ヘッダ
    for b in range(1, 16):                         # ディレクトリ = 全部空き
        d = bytearray(FRAME)
        d[0] = 0xA0
        d[8] = d[9] = 0xFF
        d[0x7F] = xsum(d[:0x7F])
        card[b * FRAME:(b + 1) * FRAME] = d
    for f in range(16, 36):                        # 破損セクタ表 = 無し
        put(f, b'\xFF\xFF\xFF\xFF')
    for f in range(36, 56):                        # 置換データ
        card[f * FRAME:(f + 1) * FRAME] = b'\xFF' * FRAME
    put(63, b'MC')                                 # 書き込みテスト
    return card


def show(card):
    hdr = card[0:2]
    print('ヘッダ: %r %s' % (bytes(hdr), 'OK' if hdr == b'MC' else '★未フォーマット'))
    for b in range(1, 16):
        d = card[b * FRAME:(b + 1) * FRAME]
        state = int.from_bytes(d[0:4], 'little')
        if state == 0x51:
            name = bytes(d[0x0A:0x1F]).split(b'\x00')[0].decode('ascii', 'replace')
            size = int.from_bytes(d[4:8], 'little')
            t = card[b * BLK * FRAME:b * BLK * FRAME + FRAME]
            title = ''
            if t[0:2] == b'SC':
                title = bytes(t[4:68]).split(b'\x00\x00')[0].decode('shift_jis', 'replace').rstrip('\x00')
            print('ブロック %2d: 使用中 "%s" %d バイト / タイトル「%s」 アイコン %s'
                  % (b, name, size, title,
                     'あり' if t[2] in (0x11, 0x12, 0x13) else '★なし(%02X)' % t[2]))
            data = card[(b * BLK + 2) * FRAME:(b * BLK + 3) * FRAME]
            print('              データ長 %d バイト' % (data[0] | (data[1] << 8)))
        elif state not in (0xA0, 0xA1, 0xA2, 0xA3):
            print('ブロック %2d: ★不明な状態 %08X' % (b, state))


if __name__ == '__main__':
    path = Path(sys.argv[1])
    if '--show' in sys.argv:
        show(path.read_bytes())
    else:
        path.write_bytes(bytes(blank()))
        print('%s: フォーマット済みの像を作った (%d バイト)' % (path, path.stat().st_size))
