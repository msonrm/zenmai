#!/usr/bin/env python3
"""作品の束（パック・.ZMP）を作る。PC-98 版はこれを起動時に読む（pack.c）。

  python3 gen_pack.py story.z3 出力.ZMP [--sec 名前=ファイル ...] title="Zork I" author=... [key=value ...]
  （--sec の節は生成器が作る: gen_translate.py / gen_cmd.py / gen_ruby.py --sec ファイル）

★**Zenmai は Z-machine で、作品はファイルとして外から渡す**形にするための入れ物（2026-09-29）。
★**story はパックに入れない**。パックが持つのは「どの story 向けか」（識別）と、Zenmai の層（訳・語彙・ふりがな）だけ。
  story は遊ぶ人が横に置く（ZORK1.Z3 など）—— 自由に配れない作品でも、訳の束だけなら配れる形にするため
  （msonrm の判断・2026-09-29。Zork I は MIT なので書庫に story も同梱する）。
  ここで story を読むのは識別を取るためだけ。
★曲・絵は入れない（ライセンスも作者も違う演出なので、外のファイルと割り当ての .INI に置く）。

書式（数はすべてリトルエンディアン）:
  0   4   'ZMPK'
  4   2   書式の版（1）
  6   2   節の数 n
  8   12n 節の索引: 名前 4 字（ASCII）・位置 u32（ファイルの頭から）・長さ u32
  ...     節の中身（索引の順に詰める）
読む側は知らない節を飛ばす。

節:
  IDNT  ★必須。合う story の識別 10 バイト = release u16・serial 6 字（ASCII）・checksum u16
        （story のヘッダの 02h / 12h / 1Ch と同じ値。★ヘッダは大きい方が先だが、ここはリトルエンディアン）
  INFO  作品の情報（UTF-8・1 行 1 項目の key=value）。いまの鍵:
          title        起動画面に出す題
          story        story のファイル名の手がかり（無ければパックと同じ名前の .Z3 → 全部の .Z3 / .DAT を識別で探す）
          author       原作
          translation  訳（誰の・何の版か）
          license      ライセンスの表示
  TRAN  訳の表（gen_translate.py --sec）
  CMDS  入力の語彙の表（gen_cmd.py --sec）
  RUBY  ふりがなの表（gen_ruby.py --sec）
        ★3 つの表の節の書式は ctab.py。節の頭の要約値（schema）が本体と違えば、本体は読む前に断る
"""
import struct
import sys

VERSION = 1


def ident(story):
    if story[0] != 3:
        sys.exit(f'gen_pack: Z-machine の版 {story[0]}（版 3 だけ扱える —— MojoZork が版 3 の処理系なので）')
    release = story[2] << 8 | story[3]
    checksum = story[0x1C] << 8 | story[0x1D]
    return struct.pack('<H', release) + story[0x12:0x18] + struct.pack('<H', checksum)


def main():
    story_path, out_path = sys.argv[1:3]
    info, secs = [], []
    args = sys.argv[3:]
    while args:
        kv = args.pop(0)
        if kv == '--sec':
            sid, _, path = args.pop(0).partition('=')
            if len(sid) != 4:
                sys.exit(f'gen_pack: 節の名前は 4 字（{sid!r}）')
            secs.append((sid.encode('ascii'), open(path, 'rb').read()))
            continue
        k, _, v = kv.partition('=')
        if not k or '\n' in v:
            sys.exit(f'gen_pack: key=value で渡す（{kv!r}）')
        info.append(f'{k}={v}\n')
    sections = [(b'IDNT', ident(open(story_path, 'rb').read())),
                (b'INFO', ''.join(info).encode('utf-8'))] + secs
    off = 8 + 12 * len(sections)
    head = b'ZMPK' + struct.pack('<HH', VERSION, len(sections))
    body = b''
    for sid, data in sections:
        while len(body) % 4:            # ★節は 4 バイト境界から（表の中身の揃えを節の中で決めてある）
            body += b'\0'
        head += sid + struct.pack('<II', off + len(body), len(data))
        body += data
    with open(out_path, 'wb') as f:
        f.write(head + body)


if __name__ == '__main__':
    main()
