#!/bin/sh
# PortMaster へ出す zip を組む。
#
#   sh portmaster/build-port.sh              → portmaster/zenmai.zip
#
# 中身は packaging.html の形:
#   zenmai.zip
#   ├── Zenmai.sh          起動スクリプト
#   ├── port.json          PortMaster の目録
#   ├── gameinfo.xml       ランチャの一覧に出る情報
#   ├── README.md          謝辞・操作表・ビルド手順
#   ├── screenshot.png     遊んでいる画面（4:3・640×480 以上）
#   └── zenmai/            GAMEDIR
#       ├── zenmai-zork.aarch64
#       └── licenses/
#
# ★**glibc の版がそのまま動く範囲を決める**。ここで焼いたバイナリは
#   焼いた機械の glibc を要求する（`__libc_start_main` の版が上がるため）。
#     - dArkOS / dArkOSen 系（Debian trixie）… 開発機と同じなのでそのまま動く
#     - ArkOS / AmberELEC（Ubuntu 20.04 = glibc 2.31）… **動かない**。
#       出すなら古い glibc の入れ物で焼き直すこと（`--check` で今の要求版が出る）
set -e
cd "$(dirname "$0")"
HERE=$(pwd)
ARCH="${ARCH:-$(uname -m)}"
OUT="${1:-$HERE/zenmai.zip}"

if [ "$1" = "--check" ] || [ "$2" = "--check" ]; then
    BIN="$HERE/zenmai/zenmai-zork.$ARCH"
    [ -f "$BIN" ] || { echo "先に組んでから: sh build-port.sh" >&2; exit 1; }
    echo "要求する glibc:"
    objdump -T "$BIN" | grep -o 'GLIBC_[0-9.]*' | sort -Vu | sed 's/^/  /'
    exit 0
fi

# 1. 本体を焼く
#
# ★配布は **FreeType 版**（GLYPH=glyph_ft.c）。焼いたビットマップ版は
#   「PS1 と画素一致するか」を確かめる検査用で、配る方ではない（native/test-sdl.sh）。
#   FreeType 版は字がフォントの被覆ぶんに増え、英字がプロポーショナルになり、
#   アンチエイリアスが乗る（12px のふりがなだけは 1bit で焼く）。
( cd ../native && GLYPH=glyph_ft.c sh build-sdl.sh "$HERE/zenmai/zenmai-zork.$ARCH" )

# 1b. 同梱フォント。★**システムのフォントに頼らない** —— R36H には Noto CJK が
#     入っていたが、PortMaster の全機種にあるとは限らない（CFW によっては
#     CJK フォントを 1 本も持たない）。実行ファイルの隣に置けば glyph_ft.c が拾う。
if [ ! -f ../native/zenmai.otf ]; then
    echo "同梱フォントが無い。先に作ってください:" >&2
    echo "  cd ../native && sh make_font.sh <KH ドットフォントを展開した dir>" >&2
    exit 1
fi
cp ../native/zenmai.otf zenmai/zenmai.otf

# 2. ライセンス（★同梱物の出自を配布物の中に持たせる。README から辿れるだけでは足りない）
mkdir -p zenmai/licenses
cp ../LICENSE                              zenmai/licenses/zenmai-MIT.txt
cp ../native/vendor/LICENSE.txt            zenmai/licenses/mojozork-zlib.txt
cp ../native/vendor/kh-dotfont/LICENSE     zenmai/licenses/kh-dotfont-OFL.txt
cp ../native/vendor/noto-cjk/LICENSE      zenmai/licenses/noto-cjk-OFL.txt
cp ../native/vendor/VENDOR.md              zenmai/licenses/VENDOR.md

# 3. スクリーンショット（gameinfo.xml が zenmai/ の中を指しているので複製する）
#
# ★**実機で撮る必要はない**。PS1 版と SDL 版が画素一致し、その一致が R36H の実機でも
#   確かめてあるので、headless で出した画面が実機の画面そのものになる:
#     cd ../native && GLYPH=glyph_ft.c sh build-sdl.sh zenmai-zork-ft
#     ZENMAI_SCRIPT=dual_ja.script ZENMAI_STOP=1620 ZENMAI_RAW=/tmp/s.raw ./zenmai-zork-ft
#     python3 raw2png.py /tmp/s.raw ../portmaster/screenshot.png
#   ★**配る方（FreeType 版）で撮る**。焼いたビットマップ版は字形も割り付けも違うので、
#   ★スクショだけ別のビルドで撮ると、**遊んだ画面と店先の画面が違う**ことになる。
#   ★stop の値で場面が決まる。1620 は「本文の上端が切れていない」点を選んである
#   （途中の値だと行が半分だけ見えた状態で写る）。
# ★先に消してから複製する。残しておくと、スクショを差し替え忘れた（あるいは消した）
#   ときに**前の版が黙って zip に入り続ける** —— このセッションで 3 回踏んだ
#   「古い成果物が残る」型（test-save.sh のシンボル / test-entry.sh の番地）。
# ★★**それでも `portmaster/screenshot.png` 自身は古くなる。** 2026-08-30 に、
#   字の大きさを 22px に固定した回（罫線が破線になった件）より**前**に撮ったものが
#   残っているのを見つけた —— 英語のバナーの折り返しが 1 行ずれていた。
#   ★**画面を変える直しをしたら、スクショも撮り直す**（この節の手順で 10 秒）。
rm -f zenmai/screenshot.png
if [ -f screenshot.png ]; then cp screenshot.png zenmai/screenshot.png; fi

# 4. zip に詰める
rm -f "$OUT"
FILES="Zenmai.sh port.json gameinfo.xml README.md zenmai"
if [ -f screenshot.png ]; then FILES="$FILES screenshot.png"; fi
# shellcheck disable=SC2086
zip -r -q "$OUT" $FILES -x '*/log.txt' '*/conf/*' '*/zenmai.sav'

echo "OK: $OUT ($(du -h "$OUT" | cut -f1))"
[ -f screenshot.png ] || echo "★ screenshot.png がまだ無い（PortMaster の PR には要る）"
