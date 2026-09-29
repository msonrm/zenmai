"""表の書き出し（gen_translate.py / gen_cmd.py / gen_ruby.py が使う）。

★1 つの定義（型・欄・行）から 4 つを出す（2026-09-30・段 8 の B）:
  <mod>_data.h  型と宣言。★表は「ポインタ + 数の変数」で宣言する（配列ではない）。
                昔の名前（TR_EXACT_N・VK_WALK など）はマクロで残すので、使う側（translate.c ほか）は変わらない
  <mod>_data.c  表そのもの（PS1 / SDL が焼き込む）
  <mod>_tab.c   読み込みの登録（PC-98 が links する。表はパックの節から読む・tabload.c）
  節（--sec）   パックに入れる表（gen_pack.py が束ねる）

★節の書式（数はすべてリトルエンディアン）:
  0    4   schema（下の要約値）
  4    2   表の数 n
  6    2   0
  8    28n 表の索引: 名前 16 字（NUL 詰め）・行の数 u32・位置 u32（節の頭から）・1 行のバイト数 u16・0 u16
  ...      表の中身（4 バイト境界に揃える）。★1 行 = 欄を宣言の順に詰めたもの（構造体の詰め物は入れない）
★C の構造体をそのまま書かないのは、詰め方がコンパイラで違い得るから（Watcom の -zp4 と gcc）。
  読む側（tabload.c）が欄ごとに offsetof の位置へ展開する。
★schema = 表の名前・型・欄の大きさと、extra（UI の文言の並びなど、本体が番号で指すもの）の要約値。
  本体（<mod>_data.h の <PFX>_SCHEMA）とパックの節で違えば、読む前に断る。
"""
import struct
import zlib

CTYPES = {'unsigned int': ('I', 4), 'unsigned short': ('H', 2), 'short': ('h', 2),
          'unsigned char': ('B', 1), 'char': ('b', 1)}


class Struct:
    def __init__(self, name, fields):
        self.name = name
        self.fields = fields            # [(ctype, 欄の名前)]

    def typedef(self):
        return 'typedef struct { ' + ' '.join(f'{t} {n};' for t, n in self.fields) + f' }} {self.name};\n'


class Table:
    """rows: 構造体なら欄の順のタプルの列、そうでなければ値の列。count_macro: 昔の名前（TR_EXACT_N など）"""

    def __init__(self, var, ctype, rows, count_macro=None, doc=''):
        self.var = var
        self.ctype = ctype              # Struct か、スカラーの型名
        self.rows = list(rows)
        self.count_macro = count_macro
        self.doc = doc

    def is_struct(self):
        return isinstance(self.ctype, Struct)

    def cname(self):
        return self.ctype.name if self.is_struct() else self.ctype

    def fields(self):
        return self.ctype.fields if self.is_struct() else [(self.ctype, None)]

    def fmt(self):
        return '<' + ''.join(CTYPES[t][0] for t, _ in self.fields())

    def row_c(self, r):
        return '{' + ','.join(str(v) for v in r) + '}' if self.is_struct() else str(r)

    def pack(self):
        f = self.fmt()
        char = self.ctype == 'char'
        return b''.join(struct.pack(f, *(r if self.is_struct() else (r - 256 if char and r > 127 else r,)))
                        for r in self.rows)


def schema(tables, extra=''):
    s = repr([(t.var, t.cname(), [(CTYPES[ct][1], n) for ct, n in t.fields()]) for t in tables]) + extra
    return zlib.crc32(s.encode('utf-8'))


def emit(mod, pfx, gen, structs, tables, head_extra='', schema_extra='', sec_path=None, here=None):
    """mod = 'translate' など（ファイル名）・pfx = 'TR' など（マクロの頭）・gen = 生成器の名前"""
    sch = schema(tables, schema_extra)
    if sec_path:
        write_sec(sec_path, tables, sch)
        return
    guard = f'{mod.upper()}_DATA_H'
    with open(here / f'{mod}_data.h', 'w') as f:
        f.write(f'/* {gen} が生成（ctab.py）。手で編集しない */\n#ifndef {guard}\n#define {guard}\n')
        for s in structs:
            f.write(s.typedef())
        f.write(head_extra)
        f.write(f'#define {pfx}_SCHEMA 0x{sch:08X}u   /* パックの節と突き合わせる（ctab.py） */\n')
        for t in tables:
            f.write(f'extern const {t.cname()} *{t.var}; extern unsigned {t.var}_n;{"   /* " + t.doc + " */" if t.doc else ""}\n')
            if t.count_macro:
                f.write(f'#define {t.count_macro} {t.var}_n\n')
        f.write('#endif\n')
    with open(here / f'{mod}_data.c', 'w') as f:
        f.write(f'/* {gen} が生成（ctab.py）。手で編集しない。PS1 / SDL が焼き込む表（PC-98 は {mod}_tab.c + パック） */\n')
        f.write(f'#include "{mod}_data.h"\n')
        for t in tables:
            rows = t.rows or [tuple(0 for _ in t.fields()) if t.is_struct() else 0]   # ★空の配列は書けない
            if t.is_struct():
                body = ',\n  '.join(t.row_c(r) for r in rows)
            else:                       # 値の列は詰めて書く（u16 は 16 進 = 字のコード）
                cell = (lambda v: f'0x{v:04X}') if t.ctype == 'unsigned short' else str
                body = ',\n  '.join(','.join(cell(v) for v in rows[i:i + 24]) for i in range(0, len(rows), 24))
            f.write(f'static const {t.cname()} {t.var}_a[] = {{\n  ' + body + '\n};\n')
            f.write(f'const {t.cname()} *{t.var} = {t.var}_a; unsigned {t.var}_n = {len(t.rows)};\n')
    with open(here / f'{mod}_tab.c', 'w') as f:
        f.write(f'/* {gen} が生成（ctab.py）。手で編集しない。PC-98 版: 表はパックの節から読む（tabload.c） */\n')
        f.write(f'#include <stddef.h>\n#include "tabload.h"\n#include "{mod}_data.h"\n')
        for t in tables:
            f.write(f'const {t.cname()} *{t.var}; unsigned {t.var}_n;\n')
        f.write(f'const TlTab {mod}_tabs[] = {{\n')
        for t in tables:
            if t.is_struct():
                fl = ','.join(f'{{offsetof({t.cname()},{n}),{CTYPES[ct][1]}}}' for ct, n in t.fields())
            else:
                fl = f'{{0,{CTYPES[t.ctype][1]}}}'
            f.write(f'  {{"{t.var}", (const void **)&{t.var}, &{t.var}_n, sizeof({t.cname()}), '
                    f'{len(t.fields())}, {{{fl}}}}},\n')
        f.write(f'}};\nconst int {mod}_tabs_n = {len(tables)};\n')
        f.write(f'const unsigned long {mod}_schema = {pfx}_SCHEMA;\n')


def write_sec(path, tables, sch):
    idx, body = b'', b''
    base = 8 + 28 * len(tables)
    for t in tables:
        while (base + len(body)) % 4:
            body += b'\0'
        data = t.pack()
        rs = struct.calcsize(t.fmt())
        name = t.var.encode('ascii')
        assert len(name) < 16, t.var
        idx += name.ljust(16, b'\0') + struct.pack('<IIHH', len(t.rows), base + len(body), rs, 0)
        body += data
    with open(path, 'wb') as f:
        f.write(struct.pack('<IHH', sch, len(tables), 0) + idx + body)


def sec_path():
    """生成器の引数 `--sec 出力` を読む（無ければ None = C を書く）"""
    import sys
    a = sys.argv[1:]
    return a[a.index('--sec') + 1] if '--sec' in a else None
