/* Zenmai プラットフォーム境界。
 *
 * ★**この 9 本だけが機械に触る。** 描画の芯（render.c）・組版・グリフ・履歴・窓・
 *   入力の状態機械（input.c）・訳（translate.c）は、どれもここより上にあり無改造で運べる。
 *
 * 実装は 1 つだけリンクする:
 *   plat_ps1.c … PS1 実機（GPU レジスタ + JOY シリアル）
 *   plat_sdl.c … SDL2（PortMaster / Linux デスクトップ）
 *
 * ★**画面は 640×480 の 15bit(RGB555) バッファ**という 1 つの形しか使わない。
 *   PS1 ではそれが VRAM の書式そのもので、SDL では SDL_PIXELFORMAT_RGB555 の
 *   ストリーミングテクスチャになる —— どちらでも「転送するだけ」で済む。
 *
 * ★セーブ（card.h）も同じ意味で境界だが、既に別ファイルに分かれているので
 *   ここには入れない（PS1 = card.c / SDL = save_file.c）。
 */
#ifndef PLAT_H
#define PLAT_H
#include <stdint.h>

enum { W = 640, H = 480 };

/* ---- パッドのビット（PS1 の並びを契約として使う。SDL 側はここへ写す）---- */
#define BTN_SELECT (1 << 0)
#define BTN_L3     (1 << 1)
#define BTN_R3     (1 << 2)
#define BTN_START  (1 << 3)
#define BTN_UP     (1 << 4)
#define BTN_RIGHT  (1 << 5)
#define BTN_DOWN   (1 << 6)
#define BTN_LEFT   (1 << 7)
#define BTN_L2     (1 << 8)
#define BTN_R2     (1 << 9)
#define BTN_L1     (1 << 10)
#define BTN_R1     (1 << 11)
#define BTN_TRI    (1 << 12)
#define BTN_CIR    (1 << 13)
#define BTN_X      (1 << 14)
#define BTN_SQ     (1 << 15)

/* ---- 画面 ---- */
void gpu_init(void);                   /* 640×480・15bpp の表示を用意する（表示はまだ出さない） */
void gpu_display_on(void);             /* 表示オン。最初の画面を組み終えてから呼ぶ */
void gp0_fill(int x, int y, int w, int h, uint32_t rgb24);
void gp0_upload(int x, int y, int w, int h, const uint16_t *src);
void gp0_copy(int sx, int sy, int dx, int dy, int w, int h);   /* 画面内コピー（スクロール） */
void wait_fields(int n);               /* n フィールド（1/60 秒）待つ。★SDL では提示も兼ねる */

/* ---- パッド ---- */
void pad_try_analog(void);             /* アナログ有効化（SDL では何もしない） */
int pad_read(void);                    /* ボタンのビット和。-1 = 繋がっていない */
int pad_read_ex(uint8_t axes[4]);      /* + スティック 4 軸（0x80 が中立） */

#endif
