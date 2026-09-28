#!/bin/sh
# PC-98 版を配る書庫にする → pc98-out/zenmai98-<版>.zip（QuuBee = https://quubee.pages.dev/ にもそのままドロップできる）
#
#   sh pack-pc98.sh
#
# ★版は pc98_version.h の 1 か所から（起動画面と同じ番号）。
# ★QuuBee は書庫の中の .bat を「起動レシピ」として読んで起動する。DOS/4GW は SET DOS16M=1 が要るので、
#   それを書いた ZENMAI.BAT を入れる（実機でも同じ .BAT で起動する）。
# ★**Zenmai は Z-machine で、Zork I は同梱の見本の作品**という書き方をする（商標の「ZORK」を前に出さない）。
#   書庫の名前にも Zork を入れない。
# ★説明書とライセンスの文書は Shift_JIS・CRLF（DOS の流儀）。配るときの義務（Mozc の著作権表示と
#   BSD 3-Clause の全文など）はここで満たす。
set -e
cd "$(dirname "$0")"
sh build-pc98.sh >/dev/null
VER=$(sed -n 's/^#define ZM98_VERSION "\(.*\)"/\1/p' pc98_version.h)
[ -n "$VER" ] || { echo "pc98_version.h から版を読めない" >&2; exit 1; }
NAME=zenmai98-$VER
OUT=pc98-out/$NAME
rm -rf "$OUT" "pc98-out/$NAME.zip"
mkdir -p "$OUT"
cp pc98-out/ZENMAI.EXE pc98-out/DOS4GW.EXE "$OUT/"

# 文書: UTF-8 で書いて Shift_JIS・CRLF にする
# ★— (U+2014) は Shift_JIS に無いので ― (U+2015) に寄せる（本体の字の表と同じ）
sjis() { python3 -c "import sys; sys.stdout.buffer.write(sys.stdin.read().replace('—', '―').replace('\n', '\r\n').encode('cp932'))"; }

printf 'SET DOS16M=1\nZENMAI\n' | sjis > "$OUT/ZENMAI.BAT"

sed "s/@VER@/$VER/" <<'DOC' | sjis > "$OUT/README.TXT"
Zenmai（ぜんまい）PC-98 版  ver. @VER@
―― 日本語で読み、日本語で打つ Z-machine

★これは beta 版です。PC-98 の実機ではまだ一度も動かしていません
  （ブラウザで動く PC-98 のエミュレータ QuuBee でだけ確かめています）。
  実機で試していただけると、とても助かります（下の「実機で試してくださる方へ」）。

■ これは何か
  Z-machine（1979 年に Infocom が作った、テキストアドベンチャーを動かす仮想機械）を
  PC-98 に載せ、出力を日本語に訳し、かなで打てるようにしたものです。
  見本の作品として Zork I を同梱しています（story file は 2025 年に MIT License で公開されたもの）。

■ 動かすのに要るもの（目安。実機では確かめていません）
  - 386 以上の CPU の PC-9801 / PC-9821
  - 16 色（アナログ）表示
  - MS-DOS と拡張メモリ（HIMEM.SYS など）

■ 起動
  ZENMAI.BAT を実行する（中で SET DOS16M=1 をしてから ZENMAI.EXE を起動します。
  DOS/4GW を使います）。起動画面で ↑↓ で言語を選び、Enter で始めます。

■ 打ち方（日本語）
  ローマ字 …… 普段の打ち方。例: yuubinbakowoakeru → ゆうびんばこをあける
  カナキー …… カナ錠を入れると、かなをそのまま打てる
  CAPS    …… 英字のまま打つ。英語のコマンド（OPEN MAILBOX など）もそのまま通る
  BS で 1 字消す。Enter で送る。
  英語（ENGLISH）を選んだときは、打ったとおりの英字になります。

■ 本文を遡る
  ROLL DOWN / ↑ で遡り、ROLL UP / ↓ で戻る。

■ やめる・セーブ
  「やめる」（英語は quit）と打つ（ゲームが確かめてくる）。
  セーブは「せーぶ」（save）、続きは「ろーど」（restore）。ファイルは ZENMAI.SAV。

■ 実機で試してくださる方へ
  次を https://github.com/msonrm/zenmai/issues で教えてください。画面の写真があると助かります。
  1. 機種・CPU・メモリ・MS-DOS の版
  2. 起動するか（紺の起動画面が出るか）
  3. ★ふりがなの位置: 漢字の真上に小さな灰色のかなが出るか。ずれたり、字と重なったりしないか
     （1 行を 24 ラスタにして字の上にふりがなの帯を作る、少し変わった設定をしています。
       ここがいちばん確かめたいところです）
  4. ローマ字・カナキー・CAPS・ROLL UP / DOWN が効くか
  5. やめたあと、DOS の画面が元どおりに戻るか

■ 商標
  「ZORK」は商標です。Zenmai は商標の権利者とは関係がありません。
  作品の画面に出る表記（ZORK is a registered trademark of Infocom, Inc.）は原作のままです。

■ 出どころとライセンス
  Zork I の story file  …… historicalsource/zork1（MIT License）                  → ZORK1.TXT
  Z-machine（MojoZork） …… Copyright (c) 2015-2025 Ryan C. Gordon（zlib License）→ MOJOZORK.TXT
  ローマ字の表          …… Mozc の romanji-hiragana.tsv
                           Copyright 2010-2018, Google Inc.（BSD 3-Clause）     → MOZC.TXT
  ふりがなの字形        …… 美咲ゴシック Copyright(C) 2002-2021 Num Kadoma
                           「改変の有無に関わらず、また商業的な利用であっても、自由にご利用、
                             複製、再配布することができますが、全て無保証」
  DOS4GW.EXE           …… DOS/4GW（Tenberry Software）。Open Watcom 1.9 に同梱の
                           royalty-free の実行時版

  https://github.com/msonrm/zenmai
DOC

sjis < ../vendor/zork1/LICENSE > "$OUT/ZORK1.TXT"
sjis < vendor/LICENSE.txt > "$OUT/MOJOZORK.TXT"
sjis < vendor/mozc/LICENSE > "$OUT/MOZC.TXT"

( cd "$OUT" && zip -q -X "../$NAME.zip" ZENMAI.BAT ZENMAI.EXE DOS4GW.EXE README.TXT ZORK1.TXT MOJOZORK.TXT MOZC.TXT )
echo "OK: pc98-out/$NAME.zip ($(du -h "pc98-out/$NAME.zip" | cut -f1)) —— https://quubee.pages.dev/ にドロップする"
