# ローマ字 → ひらがなの表の出どころ

PC-98 版のキーボード入力（`native/kana_input.c`）が使うローマ字の表は、**Mozc の表をそのまま**採ったもの。
★自前で書き起こさないのは、**打つ人の指が覚えている規則**（`nn` → ん・`kk` → っk・`xtu` → っ など）に
合わせるため —— 規則を変えると、打ち慣れた人ほど打ち間違える。

| | |
|---|---|
| 元のファイル | `src/data/preedit/romanji-hiragana.tsv`（`input` `output` `pending` の 3 列・323 行） |
| 取ってきた版 | https://github.com/google/mozc の `9b29c7d`（2026-07-06、この表を最後に変えたコミット） |
| 著作権 | `Copyright 2010-2018, Google Inc.` |
| ライセンス | **BSD 3-Clause**（全文 = このディレクトリの `LICENSE`） |

★**`kana_input_data.c` はこの表から生成したもの**（`gen_kana_input.py`）なので、BSD 3-Clause の下にある。
PC-98 版を配るときは、配布物の文書に上の著作権表示とライセンス全文を入れる（ライセンスの 2 項目め）。
