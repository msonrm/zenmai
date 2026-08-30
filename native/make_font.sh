#!/bin/sh
# 同梱するフォント（zenmai.otf）を作る。
#
# ★**なぜ部分集合にするか** —— Noto Sans CJK は 1 面で 15.7MB あり、そのうち
#   実際に出る可能性があるのは日本語の範囲だけ。ポート全体が 350KB だったので、
#   丸ごと積むと**フォントが本体の 45 倍**になる。
#
# ★**なぜシステムのフォントに頼らないか** —— R36H（dArkOSen）には Noto CJK が
#   入っていたが、★**PortMaster の全機種に入っているとは限らない**。
#   CFW によっては CJK フォントを 1 本も持たない。同梱すれば機種に依存しない。
#
# ★**GSUB/GPOS は落とさない** —— 落とせば 3.89MB → 1.75MB になるが、
#   落とせるのは CJK とハングルだけで、**デーヴァナーガリーは落とせない**
#   （結合子音と母音記号の並べ替えがそこに入っていて、HarfBuzz が読む）。
#   言語ごとに落とす表を変えると、実装と手順が言語の数だけ分岐する。
#   ★容量より実装の単純さを採る、という判断（2026-08-30）。
#
# 収録する字は **KH ドットフォントの cmap（7,623 字）と `glyphs.h` に焼いた字の和**。
# ★これは「PS1 版で出せた字は SDL 版でも必ず出せる」を保証するため ——
#   焼いた版と FreeType 版で**出せる字が食い違わない**。
#
# ★**glyphs.h を足すのは後付けではなく必須**。KH の cmap だけで濾していたら
#   `——`（U+2014）が落ちた —— 焼いた版には入っている（`{0x2014, 24}`）のに
#   同梱フォントには無く、ライセンス頁の「Zenmai —— このソフト」が
#   **黙って空白で描かれていた**（実機の指摘・2026-08-30）。
#   `glyphs.h` は「実際に出す字」そのものなので、これを足せば取りこぼしが無い。
#   見張りは `test-glyph.sh`（焼いた字が全部 FreeType 版でも出るか）。
#
#   sh make_font.sh <KH フォントを展開した dir> [出力先]
#
# 前提: fonttools（pip install fonttools）、Noto Sans CJK。
set -e
cd "$(dirname "$0")"
KH="${1:?KH ドットフォントを展開した dir を指定してください（KH-Dot-Hibiya-24.ttf がある場所）}"
OUT="${2:-zenmai.otf}"
PY="${PY:-python3}"

NOTO="${NOTO:-/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc}"
[ -f "$NOTO" ] || { echo "Noto Sans CJK が見つからない: $NOTO（NOTO= で指定できます）" >&2; exit 1; }
[ -f "$KH/KH-Dot-Hibiya-24.ttf" ] || { echo "KH-Dot-Hibiya-24.ttf が無い: $KH" >&2; exit 1; }

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

"$PY" - "$NOTO" "$KH/KH-Dot-Hibiya-24.ttf" "$TMP" glyphs.h <<'PYEOF'
import re
import sys
from fontTools.ttLib import TTCollection, TTFont
noto, kh, tmp, glyphs = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
# .ttc の先頭（Noto Sans CJK JP）を取り出す。★どの面も cmap は同じ 44,810 字で、
#   違うのは共有字形の既定の選び方だけ。日本語の字形が既定の JP を使う。
TTCollection(noto).fonts[0].save(f"{tmp}/base.otf")
kh_codes = set(TTFont(kh).getBestCmap().keys())
# ★焼いた版が出せる字（glyphs.h）は必ず入れる。KH の cmap だけだと取りこぼす
#   —— 実際に U+2014 を取りこぼしていた。
baked = set(int(m, 16) for m in
            re.findall(r'\{0x([0-9A-Fa-f]{4}),', open(glyphs, encoding='utf-8').read()))
codes = sorted(kh_codes | baked)
open(f"{tmp}/set.txt", "w").write(",".join(f"U+{c:04X}" for c in codes))
print(f"収録する字: {len(codes):,}"
      f"（KH の cmap {len(kh_codes):,} + 焼いた字のうち外の {len(baked - kh_codes)}）")
PYEOF

# ★name の 13/14（ライセンス本文と URL）を**明示的に残す**。pyftsubset の既定は
#   これらを捨てるので、放っておくと**フォントから OFL の表記が消える**
#   （glyphs.h が OFL だった件と同じ形 —— 抽出した資産は元のライセンスを引きずる）。
pyftsubset "$TMP/base.otf" --unicodes-file="$TMP/set.txt" --output-file="$OUT" \
    --name-IDs+=0,7,13,14
ls -lh "$OUT" | awk '{printf "OK: %s (%s)\n", $9, $5}'

# ★同梱物の出自を配布物の中に持たせるため、ライセンスも一緒に出す
"$PY" - "$OUT" <<'PYEOF'
import sys
from fontTools.ttLib import TTFont
f = TTFont(sys.argv[1])
n = f["name"]
for nid, label in ((0, "著作権"), (3, "識別子"), (13, "ライセンス"), (14, "URL")):
    v = n.getDebugName(nid)
    if v:
        print(f"  {label}: {v[:110]}")
PYEOF
