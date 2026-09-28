# ふりがなの字形の出どころ（PC-98 版）

`native/misaki_data.c` の字形は **美咲ゴシック**から採ったもの（`gen_misaki.py` が BDF から焼く）。

| | |
|---|---|
| 配布元 | https://littlelimit.net/misaki.htm |
| 版 | `misaki_bdf_2021-05-05.zip` の `misaki_gothic.bdf` |
| 字形 | 7×7（8×8 の枠に 1px の字間を含む）—— 本文 16px の「半分マイナス 1px」 |
| 入れた字 | ひらがな・カタカナの全部と、ふりがなに現れる字（ー など）＝ 170 字 |
| 著作権 | `Copyright(C) 2002-2021 Num Kadoma`（`misaki.txt`） |
| ライセンス | 「改変の有無に関わらず、また商業的な利用であっても、自由にご利用、複製、再配布することができますが、全て無保証」（`misaki.txt`） |

★**BDF はこのリポジトリに同梱していない**（`pc98-mock/misaki_gothic.bdf` に置くか `MISAKI_BDF` で指す）。
生成した `misaki_data.c` を追跡しているので、BDF が無くても本体は建つ。
★ライセンスに表示の義務は書かれていないが、配布物の文書には出どころと上の文言を残す
（KH ドットフォントの `vendor/kh-dotfont/` と同じ扱い）。
