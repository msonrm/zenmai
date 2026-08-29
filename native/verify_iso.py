#!/usr/bin/env python3
"""焼いた CD イメージ(MODE2/2352)の中身を照合する。

★**焼けたことと、正しく焼けたことは別**。ISO9660 を自前で辿って、
  SYSTEM.CNF の BOOT 行が指す EXE が実在し、元の .psexe と**バイト単位で同じ**かを見る。
  (エミュレータも実機も無い環境で確かめられるのはここまで。起動そのものは実機の仕事)

使い方: python3 verify_iso.py zenmai-zork.bin zenmai-zork.psexe
"""
import sys
from pathlib import Path

RAW, DATA_OFF, DATA_LEN = 2352, 24, 2048


def sector(img, n):
    return img[n * RAW + DATA_OFF:n * RAW + DATA_OFF + DATA_LEN]


def read_file(img, lba, size):
    out = bytearray()
    n = 0
    while len(out) < size:
        out += sector(img, lba + n)
        n += 1
    return bytes(out[:size])


def records(img, lba, size):
    """ディレクトリレコードを (名前, LBA, サイズ) で返す"""
    buf = read_file(img, lba, size)
    i = 0
    while i < len(buf):
        ln = buf[i]
        if ln == 0:
            i = (i // DATA_LEN + 1) * DATA_LEN     # 次のセクタ境界へ
            if i >= len(buf):
                break
            continue
        ext = int.from_bytes(buf[i + 2:i + 6], 'little')
        sz = int.from_bytes(buf[i + 10:i + 14], 'little')
        nl = buf[i + 32]
        name = buf[i + 33:i + 33 + nl].decode('latin-1')
        yield name, ext, sz
        i += ln


def main():
    img = Path(sys.argv[1]).read_bytes()
    psexe = Path(sys.argv[2]).read_bytes()
    assert len(img) % RAW == 0, f'MODE2/2352 ではない({len(img)} B)'

    pvd = sector(img, 16)
    assert pvd[1:6] == b'CD001', 'PVD が無い'
    sysid = pvd[8:40].decode('latin-1').strip()
    volid = pvd[40:72].decode('latin-1').strip()
    root = pvd[156:190]
    root_lba = int.from_bytes(root[2:6], 'little')
    root_size = int.from_bytes(root[10:14], 'little')
    print(f'system={sysid!r} volume={volid!r} root=LBA {root_lba} / {root_size} B')
    assert sysid == 'PLAYSTATION', '★system id が PLAYSTATION でない(実機が弾く)'

    entries = {n.split(';')[0]: (l, s) for n, l, s in records(img, root_lba, root_size)
               if n not in ('\x00', '\x01')}
    print('収録:', ', '.join(f'{k}({v[1]} B)' for k, v in entries.items()))

    cnf = read_file(img, *entries['SYSTEM.CNF']).decode('latin-1')
    boot = next(l for l in cnf.splitlines() if l.startswith('BOOT'))
    name = boot.split('\\')[-1].split(';')[0].strip()
    print(f'BOOT → {name}')
    assert name in entries, f'★{name} が CD に無い(起動しない)'

    exe = read_file(img, *entries[name])
    assert exe[:8] == b'PS-X EXE', '★PS-EXE ヘッダが壊れている'
    pc = int.from_bytes(exe[0x10:0x14], 'little')
    sp = int.from_bytes(exe[0x30:0x34], 'little')
    print(f'PS-EXE: PC=0x{pc:08X} SP=0x{sp:08X} / {len(exe)} B')
    assert exe == psexe, f'★中身が元の .psexe と違う({len(exe)} vs {len(psexe)})'
    print('✓ 焼いた EXE は元の .psexe とバイト単位で同一')

    lic = sector(img, 0)[:16]
    print('ライセンス領域:', '空(未改造の実機では起動しない)' if not any(lic) else repr(lic))


main()
