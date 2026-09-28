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
