/* PC-98 の .MAG（MAKI02）画像の読み込み（16 色・縁の絵柄用）。
 *
 * ★QuuBee の web/player/magimage.js（デコーダ）と同じ読み方。256 色・200 ライン（縦 2 倍）は読まない。
 * ★パレットは G, R, B の順で、各 8 ビットの上位 4 ビットを使う（0xFF も 0xF0 も 15）。
 */
#ifndef PC98_MAG_H
#define PC98_MAG_H
#include <stdint.h>

typedef struct {
    int w, h;                          /* 画素（w は 8 の倍数） */
    uint8_t *px;                       /* 4bpp を詰めた行 × h（1 行 w/2 バイト・上位ニブルが左の画素） */
    uint8_t pal[16][3];                /* G, R, B（各 0〜15） */
} Mag;

int  mag_load(const char *path, Mag *m);   /* 1 = 読めた。読めなければ何も確保しない */
void mag_free(Mag *m);

#endif
