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
for s in ../test/walkthrough.txt pc98-test/*.txt; do
    name=$(basename "$s" .txt)
    mkdir -p "$TMP/$name/host"
    cp pc98-out/ZORK1.ZMP "$TMP/$name/host/"     # ★作品はパックから読む（pack.h）
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
# ★ローマ字の台本は kana.txt と同じコマンドを打鍵で打つので、記録も同じでなければならない
if cmp -s "$TMP/kana/host/ZENMAI.LOG" "$TMP/romaji/host/ZENMAI.LOG"; then
    echo "OK  romaji = kana: 打鍵で打っても記録が同じ"
else
    echo "NG  romaji と kana の記録が違う"
    diff "$TMP/kana/host/ZENMAI.LOG" "$TMP/romaji/host/ZENMAI.LOG" | head -20
    fail=1
fi
exit $fail
