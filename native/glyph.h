/* Zenmai 字の境界。
 *
 * ★**この 5 本だけが「字がどう出るか」を知っている。** 組版・折返し・履歴・表示窓
 *   （render.c）は、幅を訊いて描けと言うだけで、字形の出どころを知らない。
 *
 * 実装は 1 つだけリンクする:
 *   glyph_baked.c … 焼いたビットマップ（glyphs.h）。PS1 と、★PS1 との画素一致を
 *                   確かめる SDL ビルド（test-sdl.sh）が使う
 *   glyph_ft.c    … FreeType。配布用の SDL ビルドが使う。字が増える（4 言語）・
 *                   プロポーショナルになる・拡縮が効く
 *
 * ★これは plat.h と同じ形の 2 本目の境界。plat.h が「機械」を切り出したのに対し、
 *   こちらは「字」を切り出す。どちらも上の層は無改造で載る。
 *
 * ★**背景色は渡さない。** アンチエイリアスは**描画先の画素に混ぜる**ことで解決する
 *   ので、本文・ルビ帯・オプションの藍板のどこに描いても正しく乗る。
 *   （引数に背景色を足すと、呼ぶ側が「いまどの地色の上か」を知る責任を負ってしまう）
 */
#ifndef GLYPH_H
#define GLYPH_H
#include <stdint.h>
#include "plat.h"

void glyph_init(void);                 /* 焼いた版は何もしない。FreeType 版は面を開く */
void glyph_clip(int y0, int y1);       /* 描く行の範囲 [y0,y1)。外は捨てる */
int  glyph_w(uint16_t code);           /* 送り幅（px） */
/* ★rows = 描画先の**行数**。省略できない引数にしてある ——
   焼いたビットマップは必ず 24 行に収まるので要らなかったが、FreeType の
   descender（g / j / p / 読点）は**ベースラインより下**へ出るので、
   24 行の器（コマンド欄 strip[24] / 状態行 sbar[24]）に描くと
   ★**.bss の隣の変数を踏み潰す**。2026-08-30 に実機で発覚した——
   症状は「打った覚えのない字がコマンドに混じる」「フリーズ」「強制終了」で、
   メモリ破壊なので出方がランダムだった。
   ★build_strip には横方向の門（「draw24 は範囲を見ない」）が既にあったが、
   縦方向には無かった。**器の高さを渡さないと描けない形**にして塞ぐ。 */
void draw24(uint16_t (*buf)[W], int rows, int x, int y, uint16_t code, uint16_t color);
void draw12(uint16_t (*buf)[W], int rows, int x, int y, uint16_t code, uint16_t color);

#endif
