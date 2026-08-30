#!/bin/sh
# ★**PS1 版と SDL 版が同じ画面を出すか**を画素で突き合わせる。
#
# 同じ台本（f0-f1:BUTTONS）を両方に食べさせ、640×480 の 15bit をそのまま比べる。
#   PS1  : sim.py が GP0 のコマンド列から VRAM を組み立てる
#   SDL  : plat_sdl.c の fb をそのまま書き出す（ZENMAI_RAW）
#
# ★これが緑なら、切り出し（plat.h）で**振る舞いが変わっていない**と言える ——
#   移植の検査を目視に頼らないための台。逆に赤が出たら、差分は必ず
#   plat_sdl.c の 9 本のどれかにある（上の層は共有しているので他に置き場が無い）。
#
# ★ただし**模しているのは論理だけ**。GPU の FIFO・パッドの間合い・実機の色は
#   ここでは見えない（test-save.sh の注記と同じ）。実機は最後の砦。
#
# 使い方: sh test-sdl.sh
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

# ★**毎回焼き直す**（4 秒）。以前は `[ -f ... ] || ./build.sh` だったが、
#   これは **psexe が古いまま、シンボル表だけ新しい ELF から取る**事故を起こす ——
#   検査は「知らない番地」を覗いてゴミを読み、★**移植のせいで壊れたように見える**。
#   （2026-08-29 に実際に踏んだ。3 件が偽の赤になった）
#   ★出力の対（out-zork.elf と zenmai-zork.psexe）は**必ず同じビルドから**にする。
./build.sh >/dev/null 2>&1
sh build-sdl.sh >/dev/null 2>&1          # ★同じ理由で毎回焼き直す

# 突き合わせる場面。★**日本語・英語・オプション画面**を通す
#   （入力の状態機械 / T9 / 重ね描き、と性格の違う 3 つを踏むため）。
run() {   # run <題> <台本> <停止フィールド>
    "$PY" sim.py zenmai-zork.psexe --script "$2" --polls --stop "$3" \
        --max 900000000 --png "$TMP/zm-ps1.png" >/dev/null 2>&1
    # ★SDL_VIDEODRIVER は指定しない —— どの headless ドライバがあるかは機械ごとに
    #   違う（R36H には dummy が無い）ので、plat_sdl.c の init_video に探させる。
    ZENMAI_SCRIPT="$2" ZENMAI_STOP="$3" ZENMAI_RAW="$TMP/zm-sdl.raw" \
        ./zenmai-zork >/dev/null 2>&1 || true
    if "$PY" cmp_frame.py "$TMP/zm-ps1.png" "$TMP/zm-sdl.raw"; then
        echo "✓ $1"
    else
        echo "✗ $1"
        ng=$((ng + 1))
    fi
}

run "日本語: 打つ → 確定 → 履歴を遡る" dual_ja.script 4400
run "英語: T9 で打つ → L3 で確定"       dual_en.script 1900
run "オプション画面を開く"              opt_open.script 800

# ★カナリア: 違う台本どうしを比べれば**必ず赤になる**こと。
#   突き合わせが素通りしていないか（両方とも真っ黒、読み違い、等）をここで潰す。
"$PY" sim.py zenmai-zork.psexe --script dual_en.script --polls --stop 1900 \
    --max 900000000 --png "$TMP/zm-ps1-en.png" >/dev/null 2>&1
if "$PY" cmp_frame.py "$TMP/zm-ps1-en.png" "$TMP/zm-sdl.raw" >/dev/null 2>&1; then
    echo "✗ ★カナリア: 別の台本どうしが一致してしまう = 突き合わせが死んでいる"
    ng=$((ng + 1))
else
    echo "✓ ★カナリア: 別の台本どうしはちゃんと食い違う"
fi

if [ "$ng" = 0 ]; then
    echo
    echo "--- PS1 と SDL は同じ画面を出している ---"
else
    echo
    echo "--- ★$ng 件 食い違った ---"
    exit 1
fi
