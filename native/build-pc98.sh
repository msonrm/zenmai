#!/bin/sh
# Zenmai PC-98 版 → pc98-out/ZENMAI.EXE（Open Watcom + DOS/4GW）と、
# 同じ芯をホストで建てた pc98-out/zenmai-host（記録を突き合わせる相手）。
#
#   sh build-pc98.sh
#
# 要るもの:
#   - Open Watcom 1.9（WATCOM、既定 ~/development/openwatcom-1.9）と、
#     この機械で動く wcc386 / wlink（OW_BIN、既定 ~/development/ow-bin。開発機は aarch64）
#   - python3・cc
#
# 罠（suika3 の build-pc98.sh で踏んだもの）:
#   - WLINK_LNK が無いと wlink が `system dos4g` を知らず E3002: format not decided
#   - wstub.exe は cwd に要る。無いと DOS スタブが欠ける
set -e
cd "$(dirname "$0")"
WATCOM="${WATCOM:-$HOME/development/openwatcom-1.9}"
OW_BIN="${OW_BIN:-$HOME/development/ow-bin}"
export WATCOM INCLUDE="$WATCOM/h" WLINK_LNK="$WATCOM/binl/wlink.lnk"
export PATH="$OW_BIN:$WATCOM/binl:$WATCOM/binw:$PATH"
OUT=pc98-out
mkdir -p "$OUT"

# 字の表（UTF-16 → テキスト VRAM）。訳・語彙の字が 1 つでも引けなければ止まる
python3 pc98_jis.py
# かな入力の表（ローマ字は Mozc の表から。vendor/mozc/）
python3 gen_kana_input.py
# ふりがなの字形（美咲ゴシック）。★BDF は追跡していないので、あるときだけ焼き直す（vendor/misaki/）
if [ -f "${MISAKI_BDF:-../pc98-mock/misaki_gothic.bdf}" ]; then python3 gen_misaki.py; fi
# 起動画面の曲（Bach の謎カノン。先の声の書き起こしと反転の規則から）
python3 gen_canon.py

# ★story は C の配列にして焼き込む（PS1 / SDL 版と同じく、同梱していることを配布の形に頼らない）
python3 - ../vendor/zork1/zork1.z3 "$OUT/story_pc98.c" <<'EOF'
import sys
b = open(sys.argv[1], 'rb').read()
with open(sys.argv[2], 'w') as f:
    f.write('/* build-pc98.sh が zork1.z3 から生成 */\n#include <stdint.h>\n')
    f.write(f'const uint32_t zm_story_len = {len(b)};\nconst uint8_t zm_story[{len(b)}] = {{\n')
    for i in range(0, len(b), 20):
        f.write(','.join(str(x) for x in b[i:i + 20]) + ',\n')
    f.write('};\n')
EOF

SRC="main_pc98.c session.c render_pc98.c save_dos.c pc98_text.c pc98_gfx.c pc98_fm.c pc98_jis.c \
     kana_input.c kana_input_data.c jp_text.c ruby_data.c misaki_data.c \
     translate.c translate_data.c cmd.c cmd_data.c canon_data.c"

# ---- PC-98（DOS/4GW）----
CFLAGS="-q -za99 -bt=dos -ox -zp4 -fpi87 -i=. $PC98_CFLAGS"   # PC98_CFLAGS: 見本を焼き分けるとき（例: -dFM_PAIR=1）
OBJS=""
for s in $SRC "$OUT/story_pc98.c"; do
    o="$OUT/$(basename "${s%.c}").obj"
    wcc386 $CFLAGS -fo="$o" "$s"
    OBJS="$OBJS file $o"
done
cp "$WATCOM/binw/wstub.exe" "$OUT/" 2>/dev/null || true
( cd "$OUT" && wlink system dos4g option quiet option stack=65536 name ZENMAI.EXE \
      $(echo "$OBJS" | sed "s| $OUT/| |g") library clib3r )
cp "$WATCOM/binw/dos4gw.exe" "$OUT/DOS4GW.EXE"

# ---- ホスト（記録の突き合わせ用）----
cc -std=gnu11 -O1 -DPC98_HOST -I. -w $SRC "$OUT/story_pc98.c" -o "$OUT/zenmai-host"

echo "OK: $OUT/ZENMAI.EXE ($(du -h "$OUT/ZENMAI.EXE" | cut -f1)) / $OUT/zenmai-host"
