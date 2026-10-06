#!/bin/sh
# 原作の ZIL ソース（historicalsource・MIT・2025-11-20 公開）を zil/ へ取る。
# ★訳の原簿の抽出器（非公開）が `ZORK_SRC=zil/zork2` のように読む。追跡はしない（.gitignore）。
# ★版は固定する（原作の側が変わっても抽出の結果が揺れないように）。変えるときは下の hash と文書を直す。
set -e
cd "$(dirname "$0")/.."
mkdir -p zil
for w in "zork1 97b7b3d68c075dd9af7da499c3e9690ada3471fd" \
         "zork2 3da9661098809788a99cef00f00c865c6c204f96" \
         "zork3 3ec9ed412b5f3cafe65d83c727d07db1fe4a86a8"; do
  set -- $w
  [ -d "zil/$1/.git" ] || git clone -q "https://github.com/historicalsource/$1" "zil/$1"
  git -C "zil/$1" fetch -q origin
  git -C "zil/$1" checkout -q "$2"
  echo "$1 $(git -C "zil/$1" rev-parse --short HEAD)"
done
