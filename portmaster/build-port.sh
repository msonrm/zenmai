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
# ★★**glibc の版がそのまま動く範囲を決める**。ここで焼いたバイナリは
#   焼いた機械の glibc を要求する（`__libc_start_main` の版が上がるため）。
#   開発機は Debian trixie（2.41）だが、ArkOS / AmberELEC は **Ubuntu 20.04 = 2.31**。
#   ★だから既定では **podman の入れ物（Containerfile）に入って焼く** ——
#     入れ物は配る先そのもの（focal）なので、数字を合わせるのではなく現物で焼くことになる。
#   ★開発機の glibc でよければ `ZM_HOST_BUILD=1` を付ける（手元で試すだけのとき）。
#   ★焼いたあとは `--check` で要求版を確かめること（2.31 を超えていたら配れない）。
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
    # ★2.31 を超えるものが 1 つでもあれば ArkOS / AmberELEC で起動しない
    TOP=$(objdump -T "$BIN" | grep -o 'GLIBC_[0-9.]*' | sort -Vu | tail -1)
    if [ "$TOP" = "$(printf '%s\nGLIBC_2.31\n' "$TOP" | sort -Vu | tail -1)" ] \
       && [ "$TOP" != "GLIBC_2.31" ]; then
        echo "★$TOP を要求している —— 2.31 の入れ物で焼き直すこと" >&2
        exit 1
    fi
    echo "OK: 2.31 までしか要求していない"
    exit 0
fi

# 1. 本体を焼く
#
# ★配布は **FreeType 版**（GLYPH=glyph_ft.c）。焼いたビットマップ版は
#   「PS1 と画素一致するか」を確かめる検査用で、配る方ではない（native/test-sdl.sh）。
#   FreeType 版は字がフォントの被覆ぶんに増え、英字がプロポーショナルになり、
#   アンチエイリアスが乗る（12px のふりがなだけは 1bit で焼く）。
if [ "${ZM_HOST_BUILD:-}" = 1 ]; then
    ( cd ../native && GLYPH=glyph_ft.c sh build-sdl.sh "$HERE/zenmai/zenmai-zork.$ARCH" )
else
    command -v podman >/dev/null 2>&1 || {
        echo "podman が要る（glibc 2.31 の入れ物で焼くため）。" >&2
        echo "  手元の glibc でよければ: ZM_HOST_BUILD=1 sh build-port.sh" >&2
        exit 1
    }
    # ★入れ物が無ければ作る（初回だけ数分）
    podman image exists localhost/zenmai-build \
        || podman build -t zenmai-build -f "$HERE/Containerfile" "$HERE"
    # ★渡すのはリポジトリごと。build-sdl.sh は ../vendor/zork1/zork1.z3 を読むので、
    #   native/ だけ渡すと story が無い。
    podman run --rm -v "$(cd .. && pwd)":/src -w /src/native localhost/zenmai-build \
        sh -c "GLYPH=glyph_ft.c sh build-sdl.sh /src/portmaster/zenmai/zenmai-zork.$ARCH"
fi

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
# ★story ファイルは実行ファイルに焼き込んである（ZIL を MIT で公開した Microsoft /
#   Activision のもの）。**焼き込んでいるのに licenses/ に無かった** —— 中身は配って
#   いるのに出自が配布物の中に無い、という穴（2026-09-05 に PortMaster 申請の
#   要件を突き合わせて見つけた）。
cp ../vendor/zork1/LICENSE                 zenmai/licenses/zork1-MIT.txt
# ★どのファイルが何を覆っているか（と、**覆っていないもの**）の索引。
#   ★kh-dotfont は**この版では字を描いていない**（部分集合の収録字を決めただけ）ことと、
#   SDL2 / FreeType は**同梱していない**ことを、ここで名指しで書いておく ——
#   「使っていないフォントを名乗る」の逆で、黙って並べると誤解させる。
cp licenses-README.md                      zenmai/licenses/README.md
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
