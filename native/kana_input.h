/* キーボードのかな入力 —— 打鍵 1 つずつを受けて、入力欄の字を組み立てる（PC-98 版）。
 *
 * ★3 通りの打ち方を受ける:
 *   ローマ字 … 規則は Mozc の表そのまま（vendor/mozc/）。`nn` → ん・`kk` → っ + k・`n` + 子音 → ん
 *   カナキー … 半角カナ（JIS X 0201 の 0xA1〜0xDF）をひらがなにする。ﾞ ﾟ は前の字にかぶせる
 *   英字     … english = 1（PC-98 では CAPS）なら表を通さずそのまま。大文字も同じ扱い ——
 *              ★cmd_run は**全部 ASCII なら英語のコマンド**として VM に渡すので、英語でも遊べる
 *
 * ★画面を知らない（純粋な核）。入力欄に見せるのは buf[0..n) の後ろに pend（組み立て途中のローマ字）。
 *   ホストの検査 = test_kana_input.c。PC-98 のキーボードと台本の `#!keys` はどちらもここを通る。
 */
#ifndef KANA_INPUT_H
#define KANA_INPUT_H
#include <stdint.h>

enum { KI_MAX = 36 };                  /* 入力欄の字数（main.c の CMD_MAX と同じ） */

typedef struct {
    uint16_t buf[KI_MAX];              /* 確定した字（かな・ASCII） */
    int n;
    char pend[8];                      /* 組み立て途中のローマ字 */
    int np;
} KiLine;

void ki_clear(KiLine *l);
/* 打鍵 1 つ。c = getch の値（ASCII / 半角カナ 0xA1〜0xDF）。english = 1 なら表を通さない */
void ki_key(KiLine *l, int c, int english);
void ki_backspace(KiLine *l);          /* 組み立て途中があればその 1 字、無ければ確定した 1 字 */
void ki_commit(KiLine *l);             /* 組み立て途中を出し切る（Enter の前）。`n` → ん */

#endif
