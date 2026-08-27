#!/bin/sh
# CD イメージ → POPS の仮想 CD-ROM(.VCD)。PS2(FreeMcBoot)の POPStarter 用。
#   前提: cue2pops(https://github.com/makefu/cue2pops-linux)を PATH か $CUE2POPS に
#   前提: build-iso.sh で zenmai-zork.cue / .bin を焼いてあること
#
# ★POPS 本体(POPS_IOX.PAK / IOPRP252.IMG)は Sony 著作物なので同梱しない。
#   自分の PS2 の DVD Player から吸うこと。手順は docs/ps1-ps2-pops.md
set -e
cd "$(dirname "$0")"
: "${CUE2POPS:=cue2pops}"
[ -f zenmai-zork.cue ] || ./build-iso.sh

# ★名前は ZENMAI.VCD ——「.VCD は大文字」(POPS は case sensitive)、かつ短く
#   (POPStarter は ELF のパスが長いと切り詰める)。ここを外すと POPStarter が
#   VCD を開けず「No Disk!」で FMCB へ戻る。
"$CUE2POPS" zenmai-zork.cue ZENMAI.VCD
ls -l ZENMAI.VCD
