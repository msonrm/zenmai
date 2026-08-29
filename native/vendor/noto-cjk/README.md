# 同梱フォントの出どころ（Noto Sans CJK）

`native/zenmai.otf`（PortMaster 版が同梱する 3.9MB のフォント）は
**Noto Sans CJK JP** の部分集合。

| | |
|---|---|
| 元 | `NotoSansCJK-Regular.ttc` の 1 面目（Noto Sans CJK JP）/ Version 2.004 |
| 著作権 | `© 2014-2021 Adobe (http://www.adobe.com/).`（name[0]） |
| 商標 | `Noto is a trademark of Google Inc.`（name[7]） |
| ライセンス | **SIL Open Font License 1.1**（name[13] / 全文 = このディレクトリの `LICENSE`） |
| 収録 | **7,623 字** —— ★KH ドットフォントの cmap と**同一の集合** |
| 作り方 | `native/make_font.sh <KH フォントの dir>` |

## ★なぜ KH と同じ集合にするか

**「PS1 版で出せた字は SDL 版でも必ず出せる」を保証するため。** 焼いた版
（`glyphs.h`）と FreeType 版で**出せる字が食い違わない**ようにしてある。
片方でしか出ない字があると、同じ台本で片方だけ豆腐（□）になる ——
[[hechima-dual-path-hazard]] と同じ形の事故になる。

（Higgins は**かな漢字変換で無限の漢字が出る**ので、この縛りを外して
フォント全体を積むことになる。そこが Zenmai と Higgins の分かれ目。）

## ★予約フォント名（Reserved Font Name）は無い

name[13] の全文を確認したところ、`Reserved Font Name` の語は**現れない**。
Source Han Sans 由来の `Source` の予約名は Adobe の宣言で、Noto として
再頒布されているこのフォントには掛かっていない。
★だから **名前を変えずに部分集合を配れる**（`glyphs.h` と KH ドットフォントの関係と同じ）。

## ★OFL の条件をどう満たしているか

- **条件 2**（複製物に著作権表示とライセンスを含める）
  → (1) フォント自身の `name` に 0 / 7 / 13 / 14 を**明示的に残してある**
    （`pyftsubset` の既定はこれらを捨てる。`make_font.sh` の `--name-IDs+=` 参照）
  → (2) ポートの `zenmai/licenses/` に全文を同梱する
- **条件 3**（予約フォント名）→ 宣言が無いので該当しない
- **条件 5**（font software は OFL の下で頒布）
  → ★**`zenmai.otf` はリポジトリの MIT ではなく OFL 1.1**
