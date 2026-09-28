/* PC-98 のテキスト画面（漢字 ROM）に字を置く層。
 *
 * ★1 行を **24 ラスタ**にする（字 16 ラスタの上に 8 ラスタの帯 = ふりがなの置き場）。
 *   640×400 に 16 行と 16 ラスタ入る（17 行目は上半分だけ見える）。設定は画面の試作（pc98-mock/screen.asm）と同じ。
 * ★反転属性の行は、字の無いラスタごと 24 ラスタ全部が塗られる（試作で確かめた）。
 * ★ホストでも同じものを配列に向けてビルドする（PC98_HOST）—— 本体の芯を PC-98 と
 *   開発機で同じに動かし、記録を突き合わせるため。
 */
#ifndef PC98_TEXT_H
#define PC98_TEXT_H
#include <stdint.h>

enum { TXT_COLS = 80, TXT_ROWS = 17, TXT_RASTERS = 24 };

/* 属性: bit0 = 表示 / bit2 = 反転 / bit5-7 = 色（GRB） */
enum {
    TA_WHITE = 0xE1, TA_YELLOW = 0xC1, TA_CYAN = 0xA1, TA_GREEN = 0x81,
    TA_REV = 0x04,
};

void txt_init(void);                   /* 画面を消し、カーソルを隠し、1 行 24 ラスタにする */
void txt_fini(void);                   /* 25 行に戻して消し、カーソルを戻す（DOS へ返る前） */
int  txt_cells(uint16_t u);            /* 字が占める桁（ASCII = 1 / 全角 = 2） */
/* 1 字置く。戻り値 = 占めた桁。★全角が右端をはみ出すなら置かずに 0 */
int  txt_put(int row, int col, uint16_t u, uint8_t attr);
void txt_clear(int row0, int row1, uint8_t attr);    /* [row0, row1] を空白で */
void txt_vsync(void);                  /* 次の垂直帰線まで待つ（ホストでは何もしない） */

#endif
