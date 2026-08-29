# 使用フォントの出どころ

`native/glyphs.h` の字形は **KH ドットフォント**から採ったもの。

| | |
|---|---|
| 本文 24×24 / 12×24 | `KH-Dot-Hibiya-24.ttf`（日比谷24・明朝体） |
| ふりがな 12×12 | `KH-Dot-Kagurazaka-12.ttf`（神楽坂12・明朝体） |
| 版 | `khdotfont-20150527`（http://jikasei.me/font/kh-dotfont/） |
| 配布物 | `khdotfont-20150527.zip` — md5 `02a87bf08fae579bacfe27cdb1371b1c`（11MB）<br>ミラー: `https://ftp.iij.ad.jp/pub/osdn.jp/users/8/8546/khdotfont-20150527.zip` |
| 被覆 | 日比谷24 = cmap **7,623 字** / 埋め込み 24×24 ビットマップ **8,048 グリフ**<br>神楽坂12 = cmap 7,199 字 / 12×12 **7,592 グリフ**（JIS X 0208 の 6,879 字を含む） |
| 著作権 | `Copyright (c) Keitarou Hiraki, Font Silo. 1990-2015`（フォント自身が名乗る文字列） |
| ライセンス | **SIL Open Font License 1.1**（全文 = このディレクトリの `LICENSE`） |

字形は平木敬太郎氏がデザインしたもので、自家製フォント工房が TrueType 化して
SIL OFL 1.1 で頒布している。**予約フォント名（Reserved Font Name）の宣言は無い。**

## ★`glyphs.h` は OFL 1.1 の下にある

**フォント本体（.ttf）はこのリポジトリに同梱していない**が、`glyphs.h` は
**フォントに埋め込まれた 24px ビットマップ（EBLC/EBDT）とバイト単位で同一**
（2026-08-25 に `fontTools` で全数照合し、1,388/1,388 一致）。つまり中身は
font software の抽出であって「レンダリング結果」ではない。

だから OFL の条件がそのまま掛かる:

- **条件 2** —— 複製物には上記の著作権表示とライセンス本文を含めること。
  → このディレクトリの `LICENSE` と、`glyphs.h` 冒頭のヘッダ、
  そして **`.psexe` のライセンス頁に焼き込んだ全文**（`native/gen_ui.py`）
- **条件 5** —— font software は一部であれ全体であれ OFL の下で頒布すること。
  → ★**`glyphs.h` だけはリポジトリの MIT ではなく OFL 1.1**（README のライセンス節に明記）
- 条件 3（予約フォント名）は宣言が無いので該当しない

★**`.psexe` は 1 本で配るので外部ファイルが置けない。** MIT も zlib も OFL も
「複製物にライセンス本文を含める」ことを条件にしているから、全文を焼き込むのが唯一の道になる。

## 焼き直し方

フォントは同梱していないので、`glyphs.h` を作り直すには上記の版を落としてくること。

```sh
cd native && python3 gen_ui.py && python3 gen_data.py <フォントを展開した dir>
python3 gen_ui.py            # ★2 度回す（1 度目が ui_chars.txt を書き、2 度目で照合が通る）
```

★**焼き直しは再現する** —— 頒布元から取り直して回しても、既存の字は 1 字も動かない。
2026-08-30 に上記 md5 の配布物から実際に焼き直し、`glyphs.h` / `content.h` /
`content_data.c` の 3 本ともバイト一致することを確かめた。
