#!/bin/sh
# PC-98 版を配る書庫にする → pc98-out/zenmai98-<版>.zip（QuuBee = https://quubee.pages.dev/ にもそのままドロップできる）
#
#   sh pack-pc98.sh
#
# ★版は pc98_version.h の 1 か所から（起動画面と同じ番号）。
# ★QuuBee は書庫の中の .bat を「起動レシピ」として読んで起動する。DOS/4GW は SET DOS16M=1 が要るので、
#   それを書いた ZENMAI.BAT を入れる（実機でも同じ .BAT で起動する）。
# ★**Zenmai は Z-machine で、Zork I〜III は同梱の見本の作品**という書き方をする（商標の「ZORK」を前に出さない）。
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
# ★作品 = パック（.ZMP）+ story（.Z3）。Zork I は訳つき、II・III は英語だけ（段 8 の C）
WORKS="ZORK1.ZMP ZORK1.Z3 ZORK2.ZMP ZORK2.Z3 ZORK3.ZMP ZORK3.Z3"
for f in ZENMAI.EXE DOS4GW.EXE $WORKS; do cp "pc98-out/$f" "$OUT/"; done

# 文書: UTF-8 で書いて Shift_JIS・CRLF にする
# ★— (U+2014) は Shift_JIS に無いので ― (U+2015) に寄せる（本体の字の表と同じ）
sjis() { python3 -c "import sys; sys.stdout.buffer.write(sys.stdin.read().replace('—', '―').replace('\n', '\r\n').encode('cp932'))"; }

printf 'SET DOS16M=1\nZENMAI\n' | sjis > "$OUT/ZENMAI.BAT"

sed "s/@VER@/$VER/" <<'DOC' | sjis > "$OUT/README.TXT"
Zenmai（ぜんまい）PC-98 版  ver. @VER@
―― 日本語で読み、日本語で打つ Z-machine

★これは beta 版です。PC-98 の実機で動いたという報告を 1 件いただきました（ありがとうございます）。
  ほかはブラウザで動く PC-98 のエミュレータ QuuBee で確かめています。
  実機で試していただけると、とても助かります（下の「実機で試してくださる方へ」）。

■ これは何か
  Z-machine（1979 年に Infocom が作った、テキストアドベンチャーを動かす仮想機械）を
  PC-98 に載せ、出力を日本語に訳し、かなで打てるようにしたものです。
  見本の作品として Zork I・II・III を同梱しています（story file は 2025 年に MIT License で
  公開されたもの）。日本語に訳してあるのは Zork I です。II と III は英語のまま遊べます。

■ 動かすのに要るもの（目安）
  - 386 以上の CPU の PC-9801 / PC-9821
  - 16 色（アナログ）表示
  - MS-DOS と拡張メモリ 2MB 以上（HIMEM.SYS など）
    ★本体と作品で約 880KB を使うので、640KB（本体メモリだけ）では動きません。
      QuuBee では拡張メモリ 1MB でも動きました。1MB の実機で試した方は、ぜひ教えてください
  - FM 音源（PC-9801-26K / 86 相当）があれば、起動画面で曲が鳴ります（無くても動きます）

■ 起動
  ZENMAI.BAT を実行する（中で SET DOS16M=1 をしてから ZENMAI.EXE を起動します。
  DOS/4GW を使います）。起動画面で ←→ で作品を、↑↓ で言語を選び、RETURN キーで始めます。

  作品は ZENMAI.EXE と同じ場所に置いた 2 種類のファイルです:
    ZORK1.Z3 など   …… story file（Z-machine のプログラムそのもの）
    ZORK1.ZMP など  …… Zenmai の層（どの story 向けか・題・訳・入力の語彙・ふりがな）
  ★ZENMAI は .ZMP を読み、それに合う story を同じ場所から探します（名前が違っても、
    中身の版が合えば見つけます。版が違う story は使いません）。
  ★.ZMP の無い story file（Z-machine の版 3 のもの）も、英語の作品として起動画面に並びます。

■ 起動画面の曲
  J. S. バッハ『音楽の捧げもの』BWV 1079 より、2 声のカノン「Quaerendo invenietis」（謎カノン）。
  1 本だけ書かれた旋律を鏡に映して読むと、もう 1 声になる曲です。FM 音源の 2 声で鳴らしています。
  RETURN キーで止まってゲームが始まります。

■ 打ち方（日本語）
  ローマ字 …… 普段の打ち方。例: yuubinbakowoakeru → ゆうびんばこをあける
  カナキー …… カナキーをロックすると、かなをそのまま打てる
  CAPS    …… 英字のまま打つ。英語のコマンド（OPEN MAILBOX など）もそのまま通る
  BS で 1 字消す。RETURN で送る。
  英語（ENGLISH）を選んだときは、打ったとおりの英字になります。

■ 本文を遡る
  ROLL DOWN / ↑ で遡り、ROLL UP / ↓ で戻る。

■ やめる・セーブ
  「やめる」（英語は quit）と打つ（ゲームが確かめてくる）。
  セーブは「せーぶ」（save）、続きは「ろーど」（restore）。ファイルは ZORK1.SAV など（作品ごと）。
  ★0.2.0-beta までのセーブ ZENMAI.SAV は、名前を ZORK1.SAV に変えると続きから遊べます。

■ 実機で試してくださる方へ
  次を https://github.com/msonrm/zenmai/issues で教えてください。画面の写真があると助かります。
  1. 機種・CPU・メモリ・MS-DOS の版
  2. 起動するか（紺の起動画面が出るか）
  3. ★ふりがなの位置: 漢字の真上に小さな灰色のかなが出るか。ずれたり、字と重なったりしないか
     （1 行を 24 ラスタにして字の上にふりがなの帯を作る、少し変わった設定をしています。
       ここがいちばん確かめたいところです）
  4. ローマ字・カナキー・CAPS・ROLL UP / DOWN が効くか
  5. やめたあと、DOS の画面が元どおりに戻るか（画面下のファンクションキーの表示も）
  6. 起動画面で曲が鳴るか（FM 音源の種類: 26K / 86 / 内蔵 など）
  7. 拡張メモリが 1MB の機種で動くか（QuuBee では動きました）

■ 商標
  「ZORK」は商標です。Zenmai は商標の権利者とは関係がありません。
  作品の画面に出る表記（ZORK is a registered trademark of Infocom, Inc.）は原作のままです。

■ 出どころとライセンス
  Zork I の story file  …… historicalsource/zork1（MIT License。ZORK1.Z3）       → ZORK1.TXT
  Zork II の story file …… historicalsource/zork2（MIT License。ZORK2.Z3）       → ZORK2.TXT
  Zork III の story file…… historicalsource/zork3（MIT License。ZORK3.Z3）       → ZORK3.TXT
  Z-machine（MojoZork） …… Copyright (c) 2015-2025 Ryan C. Gordon（zlib License）→ MOJOZORK.TXT
  ローマ字の表          …… Mozc の romanji-hiragana.tsv
                           Copyright 2010-2018, Google Inc.（BSD 3-Clause）     → MOZC.TXT
  ふりがなの字形        …… 美咲ゴシック Copyright(C) 2002-2021 Num Kadoma
                           「改変の有無に関わらず、また商業的な利用であっても、自由にご利用、
                             複製、再配布することができますが、全て無保証」
  DOS4GW.EXE           …… DOS/4GW（Tenberry Software）。Open Watcom 1.9 に同梱の
                           royalty-free の実行時版
  起動画面の曲          …… J. S. Bach『音楽の捧げもの』BWV 1079（1747 年。著作権は切れている）。
                           音は Zenmai が楽譜から書き起こしたもの

  https://github.com/msonrm/zenmai
DOC

for n in 1 2 3; do sjis < ../vendor/zork$n/LICENSE > "$OUT/ZORK$n.TXT"; done
sjis < vendor/LICENSE.txt > "$OUT/MOJOZORK.TXT"
sjis < vendor/mozc/LICENSE > "$OUT/MOZC.TXT"

( cd "$OUT" && zip -q -X "../$NAME.zip" ZENMAI.BAT ZENMAI.EXE $WORKS DOS4GW.EXE README.TXT ZORK1.TXT ZORK2.TXT ZORK3.TXT MOJOZORK.TXT MOZC.TXT )
echo "OK: pc98-out/$NAME.zip ($(du -h "pc98-out/$NAME.zip" | cut -f1)) —— https://quubee.pages.dev/ にドロップする"
