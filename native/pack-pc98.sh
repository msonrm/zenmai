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
# ★起動画面の曲 = PMD の曲（CANON.M）と、鳴らす常駐ドライバ（86 = PMD86.COM・26K = PMD.COM）
cp pc98-music/*.M pc98-music/*.INI pc98-music/PMD86.COM pc98-music/PMD.COM "$OUT/"
cp pc98-out/FIELD.MAG pc98-out/HOUSE.MAG pc98-out/WATER.MAG pc98-out/TEMPLE.MAG pc98-out/DEEP.MAG "$OUT/"   # 縁の絵柄（build-pc98.sh が作る）

# 文書: UTF-8 で書いて Shift_JIS・CRLF にする
# ★— (U+2014) は Shift_JIS に無いので ― (U+2015) に寄せる（本体の字の表と同じ）
sjis() { python3 -c "import sys; sys.stdout.buffer.write(sys.stdin.read().replace('—', '―').replace('\n', '\r\n').encode('cp932'))"; }

# ★PMD を常駐させてから起動する（/K = ESC・GRPH キーで曲を止めない）。すでに常駐していれば PMD は何も変えずに戻る。
#   FM 音源が 26K だけの機種は ZENMAI26.BAT（PMD.COM）。どちらでも、常駐できなければ曲なしで動く
printf 'SET DOS16M=1\nPMD86 /K\nZENMAI\n' | sjis > "$OUT/ZENMAI.BAT"
printf 'SET DOS16M=1\nPMD /K\nZENMAI\n' | sjis > "$OUT/ZENMAI26.BAT"

# ★README.TXT は PC-98 時代のフリーソフトの流儀（頭に名前・版・作者・日付を NEC 罫線の枠で・1 行 76 桁決め打ち）。
#   台本 pc98-readme.txt（1 段落 1 行）を fmt_readme.py が折る。日付は環境変数 PACK_DATE（YYYY.MM.DD）で決められる
python3 fmt_readme.py pc98-readme.txt "$VER" "${PACK_DATE:-$(date +%Y.%m.%d)}" | sjis > "$OUT/README.TXT"

sjis < ../LICENSE > "$OUT/ZENMAI.TXT"
for n in 1 2 3; do sjis < ../vendor/zork$n/LICENSE > "$OUT/ZORK$n.TXT"; done
sjis < vendor/LICENSE.txt > "$OUT/MOJOZORK.TXT"
sjis < vendor/mozc/LICENSE > "$OUT/MOZC.TXT"

( cd "$OUT" && zip -q -X "../$NAME.zip" ZENMAI.BAT ZENMAI26.BAT ZENMAI.EXE $WORKS ZENMAI.INI ZORK1.INI CANON.M FIELD.M HOUSE.M WATER.M TEMPLE.M DEEP.M FIELD.MAG HOUSE.MAG WATER.MAG TEMPLE.MAG DEEP.MAG PMD86.COM PMD.COM DOS4GW.EXE README.TXT ZENMAI.TXT ZORK1.TXT ZORK2.TXT ZORK3.TXT MOJOZORK.TXT MOZC.TXT )
echo "OK: pc98-out/$NAME.zip ($(du -h "pc98-out/$NAME.zip" | cut -f1)) —— https://quubee.pages.dev/ にドロップする"
