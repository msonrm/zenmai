/* PC-98 のグラフィック画面。規則は pc98_gfx.h。
 *
 * プレーンは B = A800:0・R = B000:0・G = B800:0・I = E000:0（各 80 バイト × 400 ライン）。
 * ★DOS/4GW は先頭 1MB を線形アドレスそのままで見せる（pc98_text.c と同じ）。
 * 設定の手順は画面の試作（pc98-mock/screen.asm）と同じ。
 */
#include <string.h>
#include "pc98_gfx.h"

enum { PLANE = 80 * GFX_H };

#ifdef PC98_HOST
static uint8_t host_plane[4][PLANE];
#define PLANE_AT(i) (host_plane[i])
#define OUTP(port, v) ((void)(port), (void)(v))
#else
#include <conio.h>
#include <i86.h>
static uint8_t *const plane_at[4] = {
    (uint8_t *)0xA8000, (uint8_t *)0xB0000, (uint8_t *)0xB8000, (uint8_t *)0xE0000,
};
#define PLANE_AT(i) (plane_at[i])
#define OUTP(port, v) outp((port), (v))
static void bios18(int ah, int ch)
{
    union REGS r;
    memset(&r, 0, sizeof r);
    r.h.ah = (unsigned char)ah;
    r.h.ch = (unsigned char)ch;
    int386(0x18, &r, &r);
}
#endif

void gfx_palette(int idx, int g, int r, int b)
{
    OUTP(0xA8, idx);
    OUTP(0xAA, g);
    OUTP(0xAC, r);
    OUTP(0xAE, b);
}

static void clear_all(void)
{
    for (int i = 0; i < 4; i++)
        memset(PLANE_AT(i), 0, PLANE);
}

void gfx_init(void)
{
#ifndef PC98_HOST
    bios18(0x42, 0xC0);                /* 640×400・カラー */
    bios18(0x40, 0);                   /* 表示を始める */
#endif
    OUTP(0x6A, 1);                     /* 16 色（アナログ）*/
    clear_all();
}

void gfx_fini(void)
{
    clear_all();
    OUTP(0x6A, 0);                     /* 8 色（デジタル）に戻す */
#ifndef PC98_HOST
    bios18(0x41, 0);                   /* 表示をやめる */
#endif
}

void gfx_rect(int x0, int y0, int x1, int y1, int c)
{
    const int b0 = x0 / 8, n = (x1 - x0) / 8;
    for (int i = 0; i < 4; i++) {
        const int v = (c >> i) & 1 ? 0xFF : 0x00;
        for (int y = y0; y < y1; y++)
            memset(PLANE_AT(i) + y * 80 + b0, v, (size_t)n);
    }
}

void gfx_fill(int x0, int y0, int x1, int y1, int c)
{
    for (int b = x0 / 8; b <= (x1 - 1) / 8; b++) {
        const int lo = x0 > b * 8 ? x0 - b * 8 : 0, hi = x1 < b * 8 + 8 ? x1 - b * 8 : 8;
        const uint8_t m = (uint8_t)((0xFF >> lo) & (0xFF << (8 - hi)));    /* 左端の画素が最上位ビット */
        for (int i = 0; i < 4; i++)
            for (int y = y0; y < y1; y++) {
                uint8_t *p = PLANE_AT(i) + y * 80 + b;
                if ((c >> i) & 1) *p |= m; else *p &= (uint8_t)~m;
            }
    }
}

void gfx_blit4(int x, int y, const uint8_t *px, int stride, int w, int h)
{
    for (int r = 0; r < h && y + r < GFX_H; r++) {
        const uint8_t *row = px + r * stride;
        for (int g = 0; g < w / 8; g++) {              /* 8 画素 = 4 バイトを 4 枚のプレーンの 1 バイトずつへ */
            uint8_t pl[4] = { 0, 0, 0, 0 };
            for (int j = 0; j < 8; j++) {
                const int idx = j & 1 ? row[g * 4 + j / 2] & 15 : row[g * 4 + j / 2] >> 4;
                for (int i = 0; i < 4; i++)
                    pl[i] |= (uint8_t)((idx >> i & 1) << (7 - j));
            }
            for (int i = 0; i < 4; i++)
                PLANE_AT(i)[(y + r) * 80 + x / 8 + g] = pl[i];
        }
    }
}

void gfx_glyph8(int x, int y, const uint8_t rows[8], int c)
{
    const int b = x / 8, sh = x % 8;
    for (int r = 0; r < 8; r++) {
        const int yy = y + r;
        if (yy < 0 || yy >= GFX_H || !rows[r]) continue;
        const unsigned m = (unsigned)rows[r] << 8 >> sh;    /* 16 ビットに広げて右へ寄せる */
        const uint8_t m0 = (uint8_t)(m >> 8), m1 = (uint8_t)m;
        for (int i = 0; i < 4; i++) {
            uint8_t *p = PLANE_AT(i) + yy * 80 + b;
            if ((c >> i) & 1) {
                p[0] |= m0;
                if (m1 && b + 1 < 80) p[1] |= m1;
            } else {
                p[0] &= (uint8_t)~m0;
                if (m1 && b + 1 < 80) p[1] &= (uint8_t)~m1;
            }
        }
    }
}
