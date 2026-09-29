#!/usr/bin/env python3
"""作品の束（パック・.ZMP）を作る。PC-98 版はこれを起動時に読む（pack.c）。

  python3 gen_pack.py story.z3 "作品名" 出力.ZMP

★**Zenmai は Z-machine で、作品はファイルとして外から渡す**形にするための入れ物（2026-09-29）。
いまの節は作品名と story だけ。訳・語彙・ふりがなの表は、後で節として足せる（読む側は知らない節を飛ばす）。

書式（数はすべてリトルエンディアン）:
  0   4   'ZMPK'
  4   2   書式の版（1）
  6   2   節の数 n
  8   12n 節の索引: 名前 4 字（ASCII）・位置 u32（ファイルの頭から）・長さ u32
  ...     節の中身（索引の順に詰める）

節:
  NAME  作品名（UTF-8・起動画面に出す）
  STRY  story ファイルそのまま（Z-machine の版 3 だけ —— MojoZork が版 3 の処理系なので）
"""
import struct
import sys

VERSION = 1


def main():
    story_path, name, out_path = sys.argv[1:4]
    story = open(story_path, 'rb').read()
    if story[0] != 3:
        sys.exit(f'gen_pack: {story_path} は Z-machine の版 {story[0]}（版 3 だけ扱える）')
    sections = [(b'NAME', name.encode('utf-8')), (b'STRY', story)]
    off = 8 + 12 * len(sections)
    head = b'ZMPK' + struct.pack('<HH', VERSION, len(sections))
    body = b''
    for sid, data in sections:
        head += sid + struct.pack('<II', off + len(body), len(data))
        body += data
    with open(out_path, 'wb') as f:
        f.write(head + body)


if __name__ == '__main__':
    main()
