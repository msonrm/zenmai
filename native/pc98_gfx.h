/* PC-98 のグラフィック画面（640×400・4096 色から 16 色）—— 装飾の地とふりがなを描く層。
 *
 * ★本文の字はテキスト画面（pc98_text.h）で、グラフィックはその下に敷く。
 *   テキストの字の無いところからグラフィックが透けて見える。
 * ★色の番号 = 4 枚のプレーンのビット（bit0 = B・bit1 = R・bit2 = G・bit3 = I）。
 * ★PC98_HOST ではホストの配列に向けて建つ（pc98_text.h と同じ）。
 */
#ifndef PC98_GFX_H
#define PC98_GFX_H
#include <stdint.h>

enum { GFX_W = 640, GFX_H = 400 };

void gfx_init(void);                   /* 640×400・16 色にして消し、表示する */
void gfx_fini(void);                   /* 消して表示をやめる（DOS へ返る前） */
void gfx_palette(int idx, int g, int r, int b);      /* 各 0〜15 */
/* 矩形 [x0,x1)×[y0,y1) を色 c で塗る。★x0 と x1 は 8 の倍数 */
void gfx_rect(int x0, int y0, int x1, int y1, int c);
/* 矩形 [x0,x1)×[y0,y1) を色 c で塗る。x は画素単位で任意（キャレットのような細い線用） */
void gfx_fill(int x0, int y0, int x1, int y1, int c);
/* 4bpp を詰めた画素（1 行 stride バイト・上位ニブルが左）を (x, y) へ。★x と w は 8 の倍数。色の番号はそのままパレットの番号 */
void gfx_blit4(int x, int y, const uint8_t *px, int stride, int w, int h);
/* 8×8 の字形を (x, y) に色 c で置く（x は画素単位で任意）。字形の 0 のところは触らない */
void gfx_glyph8(int x, int y, const uint8_t rows[8], int c);

#endif
