#!/bin/sh
# PMD の MML コンパイラ MC.EXE と、常駐ドライバ PMD86.COM / PMD.COM を、KAJA（梶原正裕）氏が 2019 に自由公開した
# ソースから建てる → pc98-music/.mc/{MC.EXE,PMD86.COM,PMD.COM}
#
#   sh build-mc.sh
#
# ★MC.EXE は曲を作り直すときだけ要る（`.M` は pc98-music/ に置いてあるので、本体を建てるだけなら要らない）。
#   PMD86.COM / PMD.COM は配る書庫に入れる（pc98-music/ に置いてあるものと byte 一致することを README の SHA-256 で確かめる）。
# ★ソースは commit に pin した（QuuBee の tools/pmd_build/build_pmd.sh と同じ版）。1997 配布のバイナリは使わない
#   （「無断の改変・営利使用を禁ず」の別ライセンス・出どころ = QuuBee の CREDITS.md「PMD」）。
# ★UASM（MASM 互換）で建てる。OPTASM 用のソースを通すための機械的な補正だけを当てる:
#   (a) DOS の EOF 等の制御文字を除く (b) 負変位 -N[reg] → [reg-N] (c) 文字列 equate を <...> に
#   (d) include の大小文字違いに小文字の写しを置く。★出力は `-mz`（DOS の EXE）
# 前提: gh（認証済）・gcc・make・tar。
set -e
cd "$(dirname "$0")"
OUT="$PWD/pc98-music/.mc"
[ -f "$OUT/PMD.COM" ] && { echo "$OUT に建て済み（消せば建て直す）"; exit 0; }
UASM_REF="bffb18461dd541479064990c3b2750ab50ae23e2"
PMD_REF="c620dc95c5e47970e7839cb5f0b7b9ab742d4f46"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/mc_build.XXXXXX")"
mkdir -p "$OUT"
unset UASM       # ★UASM は環境変数を「既定のオプション」として読むので、パスを入れた変数があると壊れる
gh api "repos/Terraspace/UASM/tarball/$UASM_REF" > "$WORK/uasm.tgz"
mkdir "$WORK/uasm" && tar xzf "$WORK/uasm.tgz" -C "$WORK/uasm" --strip-components=1
( cd "$WORK/uasm"
  printf '%s\n' '#ifndef UASM_DIRECT_H_SHIM' '#define UASM_DIRECT_H_SHIM' '#include <unistd.h>' \
      '#ifndef _MAX_PATH' '#define _MAX_PATH 4096' '#endif' '#define _getcwd getcwd' '#define _chdir chdir' '#endif' > H/direct.h
  grep -q '_pgmptr = ' dbgcv.c || sed -i '1i char *_pgmptr = "";' dbgcv.c
  sed -i 's/(unsigned short)(s - cv.ps - 2)/(unsigned short)((char*)s - (char*)cv.ps - 2)/' dbgcv.c
  sed -i 's/cv.section->length += (s - start)/cv.section->length += ((char*)s - (char*)start)/' dbgcv.c
  make -f Makefile_Linux CC=gcc \
      extra_c_flags="-DNDEBUG -O2 -funsigned-char -w -fcommon -Wno-error=implicit-function-declaration -Wno-error=implicit-int -Wno-error=incompatible-pointer-types -Wno-error=int-conversion" \
      -j"$(nproc)" >/dev/null )
gh api "repos/d2lmirrors/pmd/tarball/$PMD_REF" > "$WORK/pmd.tgz"
mkdir "$WORK/src" && tar xzf "$WORK/pmd.tgz" -C "$WORK/src" --strip-components=1
( cd "$WORK/src/mc"
  for f in MC.ASM *.INC; do
      tr -d '\032\034' < "$f" > "$f.t" && mv "$f.t" "$f"
      sed -E -i 's/-([0-9]+)\[([A-Za-z]+)\]/[\2-\1]/g' "$f"
  done
  sed -E -i 's/^(ver[[:space:]]+equ[[:space:]]+)"4\.8s"/\1<"4.8s">/; s/^(date[[:space:]]+equ[[:space:]]+)"2020\/01\/22"/\1<"2020\/01\/22">/' MC.ASM
  for f in *.INC; do lb=$(echo "$f" | tr A-Z a-z); [ "$f" != "$lb" ] && cp -f "$f" "$lb"; done
  "$WORK/uasm/GccUnixR/uasm" -mz -Zm -Fo=MC.EXE MC.ASM >/dev/null
  cp MC.EXE "$OUT/MC.EXE" )
# ---- 常駐ドライバ（QuuBee の tools/pmd_build/build_pmd.sh と同じ補正）----
( cd "$WORK/src/pmd"
  for f in *.ASM *.INC; do
      tr -d '\032\034' < "$f" > "$f.t" && mv "$f.t" "$f"
      sed -E -i 's/-([0-9]+)\[([A-Za-z]+)\]/[\2-\1]/g' "$f"
  done
  sed -E -i 's/^(ver[[:space:]]+equ[[:space:]]+)"4\.8s"/\1<"4.8s">/' PMD.ASM
  sed -E -i 's/^(_myname[[:space:]]+equ[[:space:]]+)"PMD86   COM"/\1<"PMD86   COM">/' PMD86.ASM
  sed -E -i 's/^(resmes[[:space:]]+equ[[:space:]]+)"PMD86 ver\.",ver/\1<"PMD86 ver.",ver>/' PMD86.ASM
  sed -E -i 's/^(_optnam[[:space:]]+equ[[:space:]]+)"\(86PCM\)"/\1<"(86PCM)">/' PMD86.ASM
  perl -0pi -e 's/\tloop\tdin0/\tdec\tcx\n\tjnz\tdin0/' PMD.ASM      # 短いジャンプの範囲外になるので置き換える
  for f in *.ASM *.INC; do lb=$(echo "$f" | tr A-Z a-z); [ "$f" != "$lb" ] && cp -f "$f" "$lb"; done
  "$WORK/uasm/GccUnixR/uasm" -bin -Zm -Fo=PMD86.COM PMD86.ASM >/dev/null
  "$WORK/uasm/GccUnixR/uasm" -bin -Zm -Fo=PMD.COM PMD.ASM >/dev/null
  cp PMD86.COM PMD.COM "$OUT/" )
rm -rf "$WORK"
echo "できた: $OUT/{MC.EXE,PMD86.COM,PMD.COM}"
sha256sum "$OUT"/PMD86.COM "$OUT"/PMD.COM
