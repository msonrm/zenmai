/* PC-98 のテキスト画面（漢字 ROM）に字を置く層。
 *
 * ★段 1（素の Zork）は標準の 80 桁 × 25 行で組む。24 ラスタ × 16 行の画面（計画書の B 案）は
 *   組版を移す段で入れる。
 * ★ホストでも同じものを配列に向けてビルドする（PC98_HOST）—— 本体の芯を PC-98 と
 *   開発機で同じに動かし、記録を突き合わせるため。
 */
#ifndef PC98_TEXT_H
#define PC98_TEXT_H
#include <stdint.h>

enum { TXT_COLS = 80, TXT_ROWS = 25 };

/* 属性: bit0 = 表示 / bit2 = 反転 / bit5-7 = 色（GRB） */
enum {
    TA_WHITE = 0xE1, TA_YELLOW = 0xC1, TA_CYAN = 0xA1, TA_GREEN = 0x81,
    TA_REV = 0x04,
};

void txt_init(void);                   /* 画面を消してカーソルを隠す */
void txt_fini(void);                   /* 画面を消してカーソルを戻す（DOS へ返る前） */
int  txt_cells(uint16_t u);            /* 字が占める桁（ASCII = 1 / 全角 = 2） */
/* 1 字置く。戻り値 = 占めた桁。★全角が右端をはみ出すなら置かずに 0 */
int  txt_put(int row, int col, uint16_t u, uint8_t attr);
void txt_clear(int row0, int row1, uint8_t attr);    /* [row0, row1] を空白で */
void txt_scroll(int row0, int row1, uint8_t attr);   /* [row0, row1] を 1 行上へ。空いた行は空白 */
void txt_cursor(int row, int col);     /* カーソルを出す位置。row < 0 で隠す */

#endif
