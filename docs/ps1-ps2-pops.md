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

## 4. 置き方（POPStarter・USB の例）

```
USB:/POPS/POPS_IOX.PAK          ← 自分の PS2 から吸う
USB:/POPS/IOPRP252.IMG          ← 同上
USB:/POPS/ZENMAI.VCD            ← 作った VCD（名前は自由）
USB:/ZENMAI.ELF                 ← POPSTARTER.ELF を VCD と同じ名前にリネーム
```

FMCB のメニューから `ZENMAI.ELF` を選ぶ。HDD / SMB 構成なら置き場所が変わる（OPL 経由でも動く）。
★**ELF と VCD の名前を揃える**のが POPStarter の作法。

## 5. 確かめる順序

**先に効くものから**。手前で落ちたら、その先は見なくていい。

1. **起動するか** —— 黒画面で止まるなら、ライセンスデータか POPS の互換性
2. **画面が出るか** —— zenmai は `GP1 = 0x08000027`（**640×480i** NTSC 15bpp）。
   PS1 では珍しい高解像度インターレースなので、POPS が化ける可能性がある。
   乱れたら `cue2pops … vmode`（画面位置と NTSC を当てるオプション）を試す
3. **パッドが効くか** —— 効けば SIO（JOY ポート）が模されている証拠。次が通る見込みが立つ
4. **セーブができるか** —— ★**ここがいちばん怪しい**。`card.c` は BIOS を経由せず
   **JOY ポートを直叩き**している（装置 81h・Read/Write Frame を psx-spx から写した）。
   POPS が低レベルの MC プロトコルをどこまで模すかは分からない。
   パッドと同じポートなので、3 が通れば見込みはある

## 6. 動かなかったときの代案

- **PS1 実機 + 改造機**（`iso.xml` に自分のライセンスデータを足して CD-R に焼く）。
  実装ノートの残件「実機 CRT（段階 6）」と同じ道になる
- **PS2 ネイティブ ELF への移植** —— GPU が GS に変わるので `render.c` の書き直し。
  別物の仕事で、ここまでの `.psexe` は使えない
