#!/bin/sh
# 「ボタンの並び」（SDL 版だけ）の検証。
#
# ★★何を確かめているか: **SDL の A/B/X/Y は「機体に印刷された札の名前」であって
#   位置ではない**ので、位置は本人に押してもらうしかない。その受け答えが正しいか。
# ★見張り方は**書かれた設定ファイルそのもの**。画面ではなく結論を見る
#   （画面は test-sdl.sh の画素一致とは別物 —— この画面は PS1 版に存在しない）。
# ★数字は**位置**（右 下 上 左）に来る**素の並びでの通し番号**（0=○ 1=✕ 2=△ 3=□）。
set -e
cd "$(dirname "$0")"

if ! mkdir .test-lock 2>/dev/null; then
    echo "★別の検査かビルドが走っています（.test-lock）。終わってからにしてください。" >&2
    echo "  （異常終了で残ったなら: rmdir native/.test-lock）" >&2
    exit 1
fi
trap 'rmdir .test-lock 2>/dev/null' EXIT INT TERM
ZM_IN_TEST=1
export ZM_IN_TEST
TMP="${TMPDIR:-/tmp}"
ng=0

# ★配る方（FreeType 版）で焼く。★毎回焼き直す（古い成果物の罠）
GLYPH=glyph_ft.c sh build-sdl.sh "$TMP/zm-face" >/dev/null 2>&1

# 台本を組んで走らせ、書かれた設定を 1 行で返す
probe() {   # probe <停止> <台本の中身…>
    stop="$1"; shift
    rm -f "$TMP/zm-face.conf" "$TMP/zm-face.sav" "$TMP/zm-face.script"
    printf '60-90:START\n' > "$TMP/zm-face.script"     # 言語は既定（ENGLISH）
    for ln in "$@"; do printf '%s\n' "$ln" >> "$TMP/zm-face.script"; done
    ZENMAI_SCRIPT="$TMP/zm-face.script" ZENMAI_STOP="$stop" ZENMAI_RAW="$TMP/zm-face.raw" \
        ZENMAI_CONF="$TMP/zm-face.conf" ZENMAI_SAVE="$TMP/zm-face.sav" \
        "$TMP/zm-face" >/dev/null 2>&1 || true
    if [ -f "$TMP/zm-face.conf" ]; then tr '\n' ' ' < "$TMP/zm-face.conf" | sed 's/ *$//'
    else echo "（書かれていない）"; fi
}

check() {
    if [ "$2" = "$3" ]; then echo "✓ $1"
    else echo "✗ $1"; echo "    実際: $2"; echo "    期待: $3"; ng=$((ng + 1)); fi
}

# ★1 回で決まる 2 系統。★台本のボタン名は BTN_* に直接入るので、
#   「右を押したら何が返ったか」をそのまま作れる。
check "★任天堂式（右で ○ が返る）= 1 回で決まる" \
      "$(probe 400 '200-230:CIR')" "zenmai-conf 1 face 0 1 2 3"
check "★Xbox 式（右で ✕ が返る）= 1 回で決まる（A↔B・X↔Y）" \
      "$(probe 400 '200-230:X')"   "zenmai-conf 1 face 1 0 3 2"

# ★★どちらの系統でもない機体では**決めつけない**。ここが仮定を剥がしている所。
check "★どちらでもない（右で △）= まだ決めない" \
      "$(probe 400 '200-230:TRI')" "（書かれていない）"
check "★そのまま 4 回訊けば決まる" \
      "$(probe 700 '200-230:TRI' '300-330:CIR' '400-430:X' '500-530:SQ')" \
      "zenmai-conf 1 face 2 0 1 3"

# ★同じボタンを 2 度受けない（受けると押しても何も出ないボタンが生まれる）
check "★同じボタンを 2 度押しても進まない" \
      "$(probe 700 '200-230:TRI' '300-330:TRI' '400-430:TRI')" "（書かれていない）"

# ★★カナリア: 覚えた設定があるときは**訊かない**。
#   （訊いてしまうと、毎回この画面を通ることになる ＝ 上の 5 件が全部無意味になる）
rm -f "$TMP/zm-face.conf" "$TMP/zm-face.sav"
printf 'zenmai-conf 1\nface 1 0 3 2\n' > "$TMP/zm-face.conf"
printf '60-90:START\n200-230:CIR\n' > "$TMP/zm-face.script"
ZENMAI_SCRIPT="$TMP/zm-face.script" ZENMAI_STOP=400 ZENMAI_RAW="$TMP/zm-face.raw" \
    ZENMAI_CONF="$TMP/zm-face.conf" ZENMAI_SAVE="$TMP/zm-face.sav" \
    "$TMP/zm-face" >/dev/null 2>&1 || true
check "★カナリア: 覚えていれば訊かない（設定が書き換わらない）" \
      "$(tr '\n' ' ' < "$TMP/zm-face.conf" | sed 's/ *$//')" "zenmai-conf 1 face 1 0 3 2"

# ★★カナリア: 壊れた設定は**受けない**（＝訊き直す）。
#   半端な並びを入れると「押しても何も出ないボタン」が生まれ、無音なので緑に見える。
rm -f "$TMP/zm-face.sav"
printf 'zenmai-conf 1\nface 0 0 2 3\n' > "$TMP/zm-face.conf"   # 0 が 2 つ
printf '60-90:START\n200-230:CIR\n' > "$TMP/zm-face.script"
ZENMAI_SCRIPT="$TMP/zm-face.script" ZENMAI_STOP=400 ZENMAI_RAW="$TMP/zm-face.raw" \
    ZENMAI_CONF="$TMP/zm-face.conf" ZENMAI_SAVE="$TMP/zm-face.sav" \
    "$TMP/zm-face" >/dev/null 2>&1 || true
check "★カナリア: 壊れた設定は訊き直して上書きする" \
      "$(tr '\n' ' ' < "$TMP/zm-face.conf" | sed 's/ *$//')" "zenmai-conf 1 face 0 1 2 3"

# ★★メニューからも同じ画面に入れること ＝ **SDL のメニューの並び**の見張り。
#   上から ひらがな入力方法 / システムコマンド / ボタン設定 / ライセンス / やめる なので
#   ↓↓ が「ボタン設定」。★並びが変わればここが赤くなる
#   （test-sdl.sh は項目の数がビルドで違うのでメニューを比べられない。その穴をここで塞ぐ）。
check "★メニューの ↓↓ が「ボタン設定」（並びの見張り）" \
      "$(probe 1100 '200-230:CIR' '500-530:START' '600-630:DOWN' '680-710:DOWN' \
                    '760-790:CIR' '860-890:X')" \
      "zenmai-conf 1 face 1 0 3 2"

rm -f "$TMP/zm-face" "$TMP/zm-face.conf" "$TMP/zm-face.sav" "$TMP/zm-face.script" "$TMP/zm-face.raw"
echo
if [ "$ng" = 0 ]; then echo "--- 8 件すべて通った ---"; else echo "--- ★$ng 件 食い違った ---"; exit 1; fi
