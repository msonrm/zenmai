#!/bin/sh
# ゲームに入るときの引き継ぎ —— **押されたままのボタンを「もう見た」ことにする**。
#
# ★2026-08-29 に実機（R36H / PortMaster）で見つかった穴:
#   言語を**面ボタン**で選ぶと、そのボタンが押されたままゲームに入るので、
#   入力の状態機械が `!prevVowelPressed` を成立させ、**選んだボタンに応じた字が
#   コマンド欄に入った状態でゲームが始まる**（✕ なら「お」）。
#
# ★**なぜ誰も踏まなかったか** —— 既存の台本が**全部 Start で言語を選んでいた**。
#   Start は字を出さないので、面ボタンで選ぶ経路だけが検査の外にあった。
#   （[[hechima-dual-path-hazard]] と同じ形: 片方の経路でしか通らない値がある）
#
# 見張り方: **clen（コマンド欄の文字数）が 0 であること**。
#
# 使い方: sh test-entry.sh
set -e
cd "$(dirname "$0")"
PY="${PY:-python3}"
ng=0

# ★毎回焼き直す（out-zork.elf と psexe を必ず同じビルドから取るため）
./build.sh >/dev/null 2>&1

ZERO="b'\\x00\\x00\\x00\\x00'"

# ★番地は**毎回いまの ELF から引き直す**。定数に置くと、焼き直したあとに
#   「古い番地 × 新しい psexe」を覗いてゴミを読む —— test-save.sh で踏んだのと
#   同じ罠で、しかも**ゴミはたいてい 0 なので緑に見える**（カナリアが死ぬ）。
clen_after() {   # clen_after <台本> <停止フィールド>
    addr=$(mipsel-linux-gnu-nm out-zork.elf | grep -i ' clen$' | cut -d' ' -f1)
    "$PY" sim.py zenmai-zork.psexe --script "$1" --polls --stop "$2" \
        --max 400000000 --dump "$addr,4" 2>/dev/null | sed -n 's/^DUMP: //p'
}

check() {   # check <題> <台本> <停止フィールド>
    R=$(clen_after "$2" "$3")
    if [ "$R" = "$ZERO" ]; then
        echo "✓ $1"
    else
        echo "✗ $1"
        echo "    コマンド欄に字が残っている: clen = $R"
        ng=$((ng + 1))
    fi
}

check "日本語を ✕ で選んでも、コマンド欄は空で始まる"    face_ja.script 400
check "ENGLISH を ○ で選んでも、コマンド欄は空で始まる" face_en.script 440

# ★カナリア: 検査そのものが生きているか。
#   引き継ぎを外した版を焼いて、**ちゃんと赤になる**ことを見る。
#   （無言を緑と取り違えないための、このリポジトリの作法）
sed 's/^    gp_sync_prev(&gm, &f);$/    (void)0;/' main.c > main_nocarry.c
if ! cmp -s main.c main_nocarry.c; then
    cp main.c main_carry.bak
    cp main_nocarry.c main.c
    ./build.sh >/dev/null 2>&1 || true
    R=$(clen_after face_ja.script 400)
    cp main_carry.bak main.c
    rm -f main_nocarry.c main_carry.bak
    ./build.sh >/dev/null 2>&1
    if [ "$R" = "$ZERO" ]; then
        echo "✗ ★カナリア: 引き継ぎを外しても緑のまま = 検査が死んでいる"
        ng=$((ng + 1))
    else
        echo "✓ ★カナリア: 引き継ぎを外すとちゃんと赤になる（clen = $R）"
    fi
else
    rm -f main_nocarry.c
    echo "✗ ★カナリア: 外す対象（gp_sync_prev）が見つからない"
    ng=$((ng + 1))
fi

if [ "$ng" = 0 ]; then
    echo
    echo "--- 3 件すべて通った ---"
else
    echo
    echo "--- ★$ng 件 食い違った ---"
    exit 1
fi
