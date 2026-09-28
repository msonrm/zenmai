#!/bin/sh
# PC-98 版を QuuBee（https://quubee.pages.dev/）にドロップできる書庫にする → pc98-out/zenmai98.zip
#
#   sh pack-pc98.sh
#
# ★QuuBee は書庫の中の .bat を「起動レシピ」として読んで起動する。DOS/4GW は SET DOS16M=1 が要るので、
#   それを書いた ZENMAI.BAT を入れる（実機でも同じ .BAT で起動する）。
# ★説明書とライセンスの文書は Shift_JIS・CRLF（DOS の流儀）。配るときの義務（Mozc の著作権表示と
#   BSD 3-Clause の全文など）はここで満たす。
set -e
cd "$(dirname "$0")"
sh build-pc98.sh >/dev/null
OUT=pc98-out/zenmai98
rm -rf "$OUT" pc98-out/zenmai98.zip
mkdir -p "$OUT"
cp pc98-out/ZENMAI.EXE pc98-out/DOS4GW.EXE "$OUT/"

# 文書: UTF-8 で書いて Shift_JIS・CRLF にする
# ★— (U+2014) は Shift_JIS に無いので ― (U+2015) に寄せる（本体の字の表と同じ）
sjis() { python3 -c "import sys; sys.stdout.buffer.write(sys.stdin.read().replace('\u2014', '\u2015').replace('\n', '\r\n').encode('cp932'))"; }

printf 'SET DOS16M=1\nZENMAI\n' | sjis > "$OUT/ZENMAI.BAT"

sjis > "$OUT/README.TXT" <<'EOF'
Zenmai（ぜんまい）PC-98 版 —— 日本語で読み、日本語で打つ Zork I

■ 起動
  ZENMAI.BAT を実行する（DOS/4GW を使う。SET DOS16M=1 が要る）。

■ 打ち方
  ローマ字 …… 普段の打ち方。例: yuubinbakowoakeru → ゆうびんばこをあける
  カナキー …… カナ錠を入れると、かなをそのまま打てる
  CAPS    …… 英字のまま打つ。英語のコマンド（OPEN MAILBOX など）もそのまま通る
  BS で 1 字消す。Enter で送る。

■ 本文を遡る
  ROLL DOWN / ↑ で遡り、ROLL UP / ↓ で戻る。

■ やめる
  「やめる」と打つ（ゲームが確かめてくる）。セーブは「せーぶ」、続きは「ろーど」（ZENMAI.SAV）。

■ 出どころとライセンス
  Zork I の story file  …… historicalsource/zork1（MIT License）            → ZORK1.TXT
  Z-machine（MojoZork） …… Copyright (c) 2015-2025 Ryan C. Gordon（zlib License）→ MOJOZORK.TXT
  ローマ字の表          …… Mozc の romanji-hiragana.tsv
                           Copyright 2010-2018, Google Inc.（BSD 3-Clause）   → MOZC.TXT
  ふりがなの字形        …… 美咲ゴシック Copyright(C) 2002-2021 Num Kadoma
                           「改変の有無に関わらず、また商業的な利用であっても、自由にご利用、
                             複製、再配布することができますが、全て無保証」
  DOS4GW.EXE           …… DOS/4GW（Tenberry Software）。Open Watcom 1.9 に同梱のもの

  https://github.com/msonrm/zenmai
EOF

sjis < ../vendor/zork1/LICENSE > "$OUT/ZORK1.TXT"
sjis < vendor/LICENSE.txt > "$OUT/MOJOZORK.TXT"
sjis < vendor/mozc/LICENSE > "$OUT/MOZC.TXT"

( cd "$OUT" && zip -q -X ../zenmai98.zip ZENMAI.BAT ZENMAI.EXE DOS4GW.EXE README.TXT ZORK1.TXT MOJOZORK.TXT MOZC.TXT )
echo "OK: pc98-out/zenmai98.zip ($(du -h pc98-out/zenmai98.zip | cut -f1)) —— https://quubee.pages.dev/ にドロップする"
