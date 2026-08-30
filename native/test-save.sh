#!/bin/sh
# セーブ / 復帰の検証。sim.py にメモリーカードを模させて、往復を確かめる。
#
# ★実機・エミュレータが要らないのがこのリグの値打ちだが、模しているのは**論理**だけ。
#   /ACK の間合いや書き込み待ちは模していないので、ここが緑でも
#   「実機で速すぎて落ちる」型は捕まらない(GPU の FIFO と同じ穴)。実機は最後の砦。
#
# 使い方: sh test-save.sh
set -e
cd "$(dirname "$0")"

# ★検査どうし・検査とビルドの**並行実行を止める**。
#   このディレクトリの zenmai-zork.psexe と out-zork.elf は共有物なので、
#   走っている最中に別のビルドが入ると**足元が入れ替わり、シンボル表と中身が
#   食い違う**（2026-08-30 に踏んだ。ライセンス頁の検査が偽の赤を出した）。
#   ★今日 4 回踏んだ「古い成果物」の罠の並行実行版。
if ! mkdir .test-lock 2>/dev/null; then
    echo "★別の検査かビルドが走っています（.test-lock）。終わってからにしてください。" >&2
    echo "  （異常終了で残ったなら: rmdir native/.test-lock）" >&2
    exit 1
fi
trap 'rmdir .test-lock 2>/dev/null' EXIT INT TERM
ZM_IN_TEST=1
export ZM_IN_TEST
PY="${PY:-python3}"
TMP="${TMPDIR:-/tmp}"
ng=0

sym() { mipsel-linux-gnu-nm "$1" | grep -i " $2\$" | cut -d' ' -f1; }

check() {  # check <題> <実際> <期待>
    if [ "$2" = "$3" ]; then
        echo "✓ $1"
    else
        echo "✗ $1"
        echo "    実際: $2"
        echo "    期待: $3"
        ng=$((ng + 1))
    fi
}

echo "--- 1. カード層(card.c)が 3000 バイトを往復するか ---"
sh build-card-test.sh >/dev/null 2>&1
"$PY" gen_card.py "$TMP/zm-card1.mcd" >/dev/null
R=$("$PY" sim.py card-test.psexe --card "$TMP/zm-card1.mcd" \
    --dump "$(sym out-card.elf result),10" --max 60000000 2>/dev/null | sed -n 's/^DUMP: //p')
check "保存 1 / 長さ 3000(0bb8) / 一致 1 / 印 ZZZZ" \
    "$R" "b'\\x01\\x00\\x00\\x00\\xb8\\x0b\\x00\\x00\\x01\\x00\\x00\\x00ZZZZ'"

echo "--- 2. カードが BIOS の読める形になっているか ---"
S=$("$PY" gen_card.py "$TMP/zm-card1.mcd" --show | tr -d '\n')
case "$S" in
    *'"BIZENMAI-ZORK"'*'タイトル「ぜんまい」 アイコン あり'*) echo "✓ ディレクトリ・タイトル・アイコン" ;;
    *) echo "✗ ディレクトリ・タイトル・アイコン"; echo "    $S"; ng=$((ng + 1)) ;;
esac

echo "--- 3. 実ゲームで「ほぞんする」→ カードに書かれるか ---"
# ★**毎回焼き直す**（4 秒）。以前は `[ -f ... ] || ./build.sh` だったが、
#   これは **psexe が古いまま、シンボル表だけ新しい ELF から取る**事故を起こす ——
#   検査は「知らない番地」を覗いてゴミを読み、★**移植のせいで壊れたように見える**。
#   （2026-08-29 に実際に踏んだ。3 件が偽の赤になった）
#   ★出力の対（out-zork.elf と zenmai-zork.psexe）は**必ず同じビルドから**にする。
./build.sh >/dev/null 2>&1
STATUS=$(sym out-zork.elf statusbuf)
"$PY" gen_card.py "$TMP/zm-card2.mcd" >/dev/null
BEFORE=$("$PY" sim.py zenmai-zork.psexe --script save_move_ja.script --polls --stop 1700 \
    --max 400000000 --card "$TMP/zm-card2.mcd" --dump "$STATUS,30" 2>/dev/null | sed -n 's/^DUMP: //p')
check "保存した時点の部屋 = North of House(北へ 1 手)" \
    "$BEFORE" "b'North of House                    Score: 0/1\\x00   '"
D=$("$PY" gen_card.py "$TMP/zm-card2.mcd" --show | sed -n 's/.*データ長 \([0-9]*\) バイト.*/\1/p')
if [ -n "$D" ] && [ "$D" -gt 0 ] && [ "$D" -lt 7935 ]; then
    echo "✓ セーブデータ $D バイト(上限 7934)"
else
    echo "✗ セーブデータの長さが取れない / 収まらない: '$D'"; ng=$((ng + 1))
fi

echo "--- 4. 「さいかいする」で戻るか ---"
AFTER=$("$PY" sim.py zenmai-zork.psexe --script restore_ja.script --polls --stop 1200 \
    --max 400000000 --card "$TMP/zm-card2.mcd" --dump "$STATUS,30" 2>/dev/null | sed -n 's/^DUMP: //p')
check "復帰した先 = North of House(起動直後は West of House)" \
    "$AFTER" "b'North of House                    Score: 0/1\\x00   '"

echo "--- 5. ★カナリア: セーブが無ければ戻らないこと ---"
"$PY" gen_card.py "$TMP/zm-card3.mcd" >/dev/null
EMPTY=$("$PY" sim.py zenmai-zork.psexe --script restore_ja.script --polls --stop 1200 \
    --max 400000000 --card "$TMP/zm-card3.mcd" --dump "$STATUS,30" 2>/dev/null | sed -n 's/^DUMP: //p')
check "空のカードでは West of House のまま" \
    "$EMPTY" "b'West of House                     Score: 0/0\\x00   '"

echo
if [ "$ng" = 0 ]; then
    echo "--- 5 件すべて通った ---"
else
    echo "--- ★$ng 件 食い違った ---"
    exit 1
fi
