# PS2（FreeMcBoot）で動かす —— POPStarter 経由

★**未検証**。手前の材料（PS1 の CD イメージと POPS の VCD）はこちらで焼けたが、
**起動するかどうかは実機でしか分からない**。`sim.py` は PS1 の R3000 を手書きしたもので、
PS2 も POPS も模せない。この文書は「試す順序」と「動かなかったときの切り分け」を置く。

## 1. なぜ `.psexe` のままでは駄目か

FreeMcBoot が起動するのは **PS2 の ELF** で、PS1 の PS-EXE ではない。
PS2 で PS1 のソフトを動かす道は 2 本ある:

| 道 | 中身 | CD-R / 自作ソフト |
|---|---|---|
| **本物の PS1 互換モード** | IOP を PS1 として動かす（ハードウェア） | ディスクのプロテクトを見るので **modchip が要る**。FMCB では開かない |
| **POPS**（POPStarter） | PS2 上の**ソフトウェア PS1 エミュレータ**。物理ディスクではなく `.VCD` を読む | ★ディスク由来のプロテクトは効きようがない。**FMCB で開くのはこちら** |

なので **PS1 のディスクイメージに包んで、VCD に変換する**。

## 2. 作り方

```bash
cd ps1
./build-zork.sh                       # .psexe（既存）
MKPSXISO=…/mkpsxiso ./build-iso.sh    # → zenmai-zork.bin / .cue
python3 verify_iso.py zenmai-zork.bin zenmai-zork.psexe   # 中身の照合
CUE2POPS=…/cue2pops ./build-vcd.sh    # → zenmai-zork.vcd
```

道具は 2 つとも野良ビルド（Debian のパッケージには無い）:

- [mkpsxiso](https://github.com/Lameguy64/mkpsxiso) —— `git submodule update --init` を忘れると
  tinyxml2 が空で configure が止まる
- [cue2pops-linux](https://github.com/makefu/cue2pops-linux) —— `make` だけ

`verify_iso.py` は **ISO9660 を自前で辿って**、`SYSTEM.CNF` の `BOOT` 行が指す EXE が実在し、
元の `.psexe` と**バイト単位で同じ**かを見る。★**焼けたことと、正しく焼けたことは別**。

```
system='PLAYSTATION' volume='ZENMAI' root=LBA 22 / 2048 B
収録: SYSTEM.CNF(63 B), ZENMAI.EXE(694272 B)
BOOT → ZENMAI.EXE
PS-EXE: PC=0x80010000 SP=0x801FFF00 / 694272 B
✓ 焼いた EXE は元の .psexe とバイト単位で同一
ライセンス領域: 空(未改造の実機では起動しない)
```

## 3. 権利 —— 3 つとも「自分の機械から吸う」側

配布物に入れられないものが 3 つある。**どれも Sony の著作物**:

| 何 | どこから | 無いとどうなる |
|---|---|---|
| PS1 のライセンスデータ（`LICENSEA.DAT` 等） | 自分の PS1 ソフト | **未改造の実機では起動しない**（エミュレータ・OpenBIOS・POPS は不要のはず） |
| `POPS_IOX.PAK` | 自分の PS2 の DVD Player | POPStarter が動かない |
| `IOPRP252.IMG` | 同上 | 同上 |

`iso.xml` に `<license file="…"/>` を足せば、手元で焼くぶんにはライセンスデータを入れられる。
**配布物には入れない**（`ps1-mock/iso.xml` から引き継いだ方針）。

## 4. 置き方（USB の例）

★**2026-08-27 の実機（FMCB）で「No Disk!」**。POPStarter は起動しているのに VCD を開けない、
という形だった。**下の 3 つはそのとき分かった作法**で、最初の版はどれも外していた。

```
USB:/POPS/POPS_IOX.PAK      ← 自分の PS2 の DVD Player から吸う
USB:/POPS/IOPRP252.IMG      ← 同上
USB:/POPS/PFS_WRAP.BIN      ← 同上（構成によっては POPS.PAK / POPS.ELF / PATCH_5.BIN も）
USB:/POPS/ZENMAI.VCD        ← 焼いた VCD
USB:/XX.ZENMAI.ELF          ← POPSTARTER.ELF をリネーム（USB では XX. を前に付ける流儀）
```

- ★**`.VCD` は大文字**。POPS は**大小を区別する**ので、`.vcd` だと見つけられない
- ★**名前は短く**。POPStarter は ELF のパスが長いと**切り詰める**ので、VCD が引けなくなる
- ★**`POPS/` はドライブのルート**に置く
- USB は FAT32（exFAT では `__common/` を作らない）

構成（USB / HDD / SMB、POPStarter の版、OPL 経由かどうか）で作法が変わる。
★**この文書の配置は「一例」で、正典は使っている POPStarter の付属文書**。

## 5. 確かめる順序

**先に効くものから**。手前で落ちたら、その先は見なくていい。

0. ★**手持ちの PS1 ソフトを同じ手順で VCD にして起動する** —— これが最初。
   動けば環境（POPS の配置・ELF 名）は正しく、**問題は zenmai 側**だと確定する。
   動かなければ環境側。**変数を 1 つにするために、まずこれを通す**
1. **起動するか** —— 黒画面 →「No Disk!」なら VCD が開けていない（名前・大小・パスの長さ）。
   黒画面のまま止まるなら **ライセンスデータ**か POPS の互換性
2. **画面が出るか** —— zenmai は `GP1 = 0x08000027`（**640×480i** NTSC 15bpp）。
   PS1 では珍しい高解像度インターレースなので、化けたら `cue2pops … vmode` を試す
3. **パッドが効くか** —— 効けば SIO（JOY ポート）が模されている証拠。次の見込みが立つ
4. **セーブができるか** —— ★`card.c` は BIOS を経由せず **JOY ポート直叩き**（装置 81h）。
   POPS が低レベルの MC プロトコルをどこまで模すかは分からない

## 6. ★ライセンスデータについての訂正

最初の版で「POPS は物理ディスクではなく VCD を読むので、ディスク由来のプロテクトは
効きようがない**はず**」と書いた。**これは怪しい**。POPS は PS1 の BIOS を模しているので、
**ライセンス領域（先頭 16 セクタ）を見ている可能性がある** —— エミュレータ（DuckStation 等）が
見ないのは、そこを意図的に飛ばしているからで、POPS が同じとは限らない。

手元で焼くぶんには入れられる。**自分の PS1 ソフトから吸う**:

```bash
dumpsxiso -x out/ -s out/game.xml /path/to/your-ps1-game.cue   # licence.dat が出る
LICENSE=out/licence.dat ./build-iso.sh                          # 焼き込む
./build-vcd.sh
```

★**配布物には入れない**（Sony の著作物）。`verify_iso.py` は入ったかどうかを見る:

```
ライセンス領域: 空(未改造の実機では起動しない)     ← 入っていない
```

## 7. 動かなかったときの代案

- **PS1 実機 + 改造機**（同じ `.bin/.cue` を CD-R に焼く）。
  実装ノートの残件「実機 CRT（段階 6）」と同じ道になる
- **PS2 ネイティブ ELF への移植** —— GPU が GS に変わるので `render.c` の書き直し。
  別物の仕事で、ここまでの `.psexe` は使えない
