#!/bin/sh
# PC-98 版の検査: 台本を**ホストと QuuBee の両方**で流し、記録（ZENMAI.LOG）が
# 1 バイトも違わないことを確かめる。★Open Watcom + DOS/4GW で建てた芯が、開発機の gcc で
# 建てた同じ芯と同じに動くことの証拠（訳・語彙そのものの正しさは test_translate / cmd_test_host が JS と照合済み）。
#
#   sh test-pc98.sh            # 建ててから流す。画面は pc98-out/<台本>-1x.png に残る
#
# ★QuuBee は**拡張メモリ 2MB**で流す（動かすのに要るものの下限。本体が要るメモリを増やしたらここで気付く）。
# 台本: ../test/walkthrough.txt（英語 211 手）と pc98-test/*.txt（かなで打つもの・ローマ字の打鍵で打つもの）。
# その前に、かな入力の核（test_kana_input）と本文の組み方（test_render_pc98）をホストで確かめる。
# pc98-test/layout.txt は組み方を画面で見るための台本（#!line）—— 画面は pc98-out/layout-1x.png。
# 要るもの: build-pc98.sh の道具一式・node・QuuBee のリポジトリ（QB_DIR、既定 ~/development/qb）
set -e
cd "$(dirname "$0")"
sh build-pc98.sh >/dev/null
# かな入力の核（打鍵 → 入力欄の字）
cc -std=c11 -Wall test_kana_input.c kana_input.c kana_input_data.c -o pc98-out/test_kana_input
pc98-out/test_kana_input || exit 1
# 本文の組み方（禁則・ぶら下げ・英字の語・ふりがなの位置）
cc -std=gnu11 -Wall -DPC98_HOST -I. test_render_pc98.c pc98_text.c pc98_gfx.c pc98_jis.c misaki_data.c \
    jp_text.c ruby_data.c -o pc98-out/test_render_pc98
pc98-out/test_render_pc98 || exit 1
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
fail=0
# ★story の探し方（pack.h）: パックは識別だけを持ち、story は横に置く。識別の合わない story は使わない
story_case() {   # 名前 期待（ok = 起動する / それ以外 = 出るはずの文言）[台本（既定 = 英語）]
    ( cd "$TMP/story" && "$OLDPWD/pc98-out/zenmai-host" "$OLDPWD/${3:-pc98-test/english.txt}" >out.txt 2>&1 ) || true
    if [ "$2" = ok ]; then got=$([ -s "$TMP/story/ZENMAI.LOG" ] && echo y)
    else got=$(grep -q "$2" "$TMP/story/out.txt" && echo y); fi
    if [ "$got" = y ]; then
        echo "OK  story: $1"
    else
        echo "NG  story: $1"; cat "$TMP/story/out.txt"; fail=1
    fi
    rm -f "$TMP/story/ZENMAI.LOG"
}
mkdir -p "$TMP/story" && cp pc98-out/ZORK1.ZMP "$TMP/story/"
story_case "story が無い → 置くべき識別を言う" "no story file (release 119 / serial 880429)"
cp pc98-out/ZORK1.Z3 "$TMP/story/GAME.DAT"
story_case "別の名前（.DAT）でも識別で見つける" ok
python3 -c "import sys; b=bytearray(open(sys.argv[1],'rb').read()); b[0x17]^=1; open(sys.argv[2],'wb').write(b)" \
    pc98-out/ZORK1.Z3 "$TMP/story/GAME.DAT"
cp "$TMP/story/GAME.DAT" "$TMP/story/ZORK1.Z3"
story_case "識別の合わない story は使わない" "no story file"
# ★表の要約値（schema）が本体と違うパックは読む前に断る（ctab.py）
cp pc98-out/ZORK1.Z3 "$TMP/story/ZORK1.Z3"
python3 - pc98-out/ZORK1.ZMP "$TMP/story/ZORK1.ZMP" <<'PY'
import struct, sys
b = bytearray(open(sys.argv[1], 'rb').read())
n = struct.unpack_from('<H', b, 6)[0]
for i in range(n):
    sid, off, ln = struct.unpack_from('<4sII', b, 8 + 12 * i)
    if sid == b'TRAN':
        b[off] ^= 1
open(sys.argv[2], 'wb').write(b)
PY
story_case "表の版が本体と違うパックは断る" "the tables are for another version of Zenmai" pc98-test/kana.txt
story_case "英語で遊ぶなら表は読まない（版が違っても起動する）" ok
# ★パックの無い story は英語だけの作品として並ぶ（題はファイル名・セーブもその名前）
rm -f "$TMP/story/"*
cp pc98-out/ZORK2.Z3 "$TMP/story/"
story_case "パックの無い story（ZORK2.Z3）を英語で遊ぶ" ok pc98-test/zork2.txt
[ -s "$TMP/story/ZORK2.SAV" ] && echo "OK  story: パックの無い story のセーブは ZORK2.SAV" || { echo "NG  story: ZORK2.SAV が無い"; fail=1; }
rm -f "$TMP/story/"*
story_case "作品が 1 つも無い → 置くものを言う" "no game here"
for s in ../test/walkthrough.txt pc98-test/*.txt; do
    name=$(basename "$s" .txt)
    mkdir -p "$TMP/$name/host"
    cp pc98-out/*.ZMP pc98-out/*.Z3 pc98-out/*.INI "$TMP/$name/host/"     # ★作品 = パック + story（pack.h）。全部置く
    ( cd "$TMP/$name/host" && "$OLDPWD/pc98-out/zenmai-host" "$OLDPWD/$s" )
    PC98_EXTMEM=2 node pc98_run.js "$s" "$TMP/$name/qb" >"$TMP/$name/run.txt" 2>&1 || { cat "$TMP/$name/run.txt"; fail=1; continue; }
    cp "$TMP/$name/qb/screen-1x.png" "pc98-out/$name-1x.png"
    if cmp -s "$TMP/$name/host/ZENMAI.LOG" "$TMP/$name/qb/ZENMAI.LOG"; then
        echo "OK  $name: $(wc -l < "$TMP/$name/host/ZENMAI.LOG") 行一致（$(head -1 "$TMP/$name/run.txt")）"
    else
        echo "NG  $name: 記録が違う"
        diff "$TMP/$name/host/ZENMAI.LOG" "$TMP/$name/qb/ZENMAI.LOG" | head -20
        fail=1
    fi
done
# 参考: 拡張 1MB でも日本語の台本が通るか（★1MB は目標ではないので落とさない。届かなくなったら気付くための表示。
#   確保の順番を変えると届かなくなる —— 大きなもの（story・表）を先に、本文の環の塊を後に・段 8）
if PC98_EXTMEM=1 node pc98_run.js pc98-test/kana.txt "$TMP/kana/qb1" >/dev/null 2>&1 \
    && cmp -s "$TMP/kana/host/ZENMAI.LOG" "$TMP/kana/qb1/ZENMAI.LOG"; then
    echo "参考 拡張 1MB: kana が通る"
else
    echo "参考 拡張 1MB: ★kana が通らない（要件の 2MB では動く）"
fi
# ★セーブは作品ごとの名前（ZORK1.ZMP → ZORK1.SAV）。kana.txt はセーブの往復をする
if [ -s "$TMP/kana/host/ZORK1.SAV" ] && [ ! -e "$TMP/kana/host/ZENMAI.SAV" ]; then
    echo "OK  セーブは ZORK1.SAV"
else
    echo "NG  セーブの名前"; ls "$TMP/kana/host"; fail=1
fi
# ★ローマ字の台本は kana.txt と同じコマンドを打鍵で打つので、記録も同じでなければならない
if cmp -s "$TMP/kana/host/ZENMAI.LOG" "$TMP/romaji/host/ZENMAI.LOG"; then
    echo "OK  romaji = kana: 打鍵で打っても記録が同じ"
else
    echo "NG  romaji と kana の記録が違う"
    diff "$TMP/kana/host/ZENMAI.LOG" "$TMP/romaji/host/ZENMAI.LOG" | head -20
    fail=1
fi
# ★起動画面の曲（PMD86 / PMD.COM で鳴る・PMD が無くても起動する・RETURN で消える）
node test-pc98-music.js || fail=1
exit $fail
