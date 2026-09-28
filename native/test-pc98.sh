#!/bin/sh
# PC-98 版（段 1）の検査: 台本を**ホストと QuuBee の両方**で流し、記録（ZENMAI.LOG）が
# 1 バイトも違わないことを確かめる。★Open Watcom + DOS/4GW で建てた芯が、開発機の gcc で
# 建てた同じ芯と同じに動くことの証拠（訳・語彙そのものの正しさは test_translate / cmd_test_host が JS と照合済み）。
#
#   sh test-pc98.sh            # 建ててから流す。画面は pc98-out/<台本>-1x.png に残る
#
# 台本: ../test/walkthrough.txt（英語 211 手）と pc98-test/*.txt（かなで打つもの）。
# 要るもの: build-pc98.sh の道具一式・node・QuuBee のリポジトリ（QB_DIR、既定 ~/development/qb）
set -e
cd "$(dirname "$0")"
sh build-pc98.sh >/dev/null
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
fail=0
for s in ../test/walkthrough.txt pc98-test/*.txt; do
    name=$(basename "$s" .txt)
    mkdir -p "$TMP/$name/host"
    ( cd "$TMP/$name/host" && "$OLDPWD/pc98-out/zenmai-host" "$OLDPWD/$s" )
    node pc98_run.js "$s" "$TMP/$name/qb" >"$TMP/$name/run.txt" 2>&1 || { cat "$TMP/$name/run.txt"; fail=1; continue; }
    cp "$TMP/$name/qb/screen-1x.png" "pc98-out/$name-1x.png"
    if cmp -s "$TMP/$name/host/ZENMAI.LOG" "$TMP/$name/qb/ZENMAI.LOG"; then
        echo "OK  $name: $(wc -l < "$TMP/$name/host/ZENMAI.LOG") 行一致（$(head -1 "$TMP/$name/run.txt")）"
    else
        echo "NG  $name: 記録が違う"
        diff "$TMP/$name/host/ZENMAI.LOG" "$TMP/$name/qb/ZENMAI.LOG" | head -20
        fail=1
    fi
done
exit $fail
