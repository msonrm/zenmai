/* PC-98 のテキスト画面。規則は pc98_text.h。
 *
 * テキスト VRAM は A000:0000（字・1 桁 2 バイト）と A200:0000（属性）。
 * ★全角は 2 桁を使い、左に「左半分の語」、右に同じ語の bit7 を立てたものを置く
 *   （語の作り方の正典 = pc98_jis.py）。
 * ★DOS/4GW は先頭 1MB を線形アドレスそのままで見せるので、0xA0000 をそのまま指せる。
 */
#include <string.h>
#include "pc98_text.h"
#include "pc98_jis.h"

#ifdef PC98_HOST
static uint16_t host_code[25 * TXT_COLS], host_attr[25 * TXT_COLS];   /* VRAM は 25 行ぶん */
#define VCODE host_code
#define VATTR host_attr
#else
#include <i86.h>
#define VCODE ((volatile uint16_t *)0xA0000)
#define VATTR ((volatile uint16_t *)0xA2000)
#include <conio.h>
static void bios18(int ah, int al, int dx)
{
    union REGS r;
    memset(&r, 0, sizeof r);
    r.h.ah = (unsigned char)ah;
    r.h.al = (unsigned char)al;
    r.w.dx = (unsigned short)dx;
    int386(0x18, &r, &r);
}

/* 1 行 24 ラスタ・字の上に 8 ラスタ（試作 pc98-mock/screen.asm と同じ）。
   ★CRTC の PL = 18h は「字の上に 8 ラスタ」（−8）。実機でここまで同じに出るかは未確認 */
static void rows24(void)
{
    outp(0x70, 0x18);                  /* PL */
    outp(0x72, 0x0F);                  /* BL: 字の最後のラスタ = 15 */
    outp(0x74, 0x10);                  /* CL: 字は 16 ラスタ */
    outp(0x76, 0x00);                  /* SSL */
    while (!(inp(0x60) & 4)) { }       /* GDC の FIFO が空くのを待つ */
    outp(0x62, 0x4B);                  /* CSRFORM */
    outp(0x60, 0x17);                  /* LR = 17h（24 ラスタ）・カーソルは出さない */
    outp(0x60, 0x00);
    outp(0x60, 0xBB);
}
#endif

static uint16_t geta;                  /* 引けない字の代わり（〓） */

static int jis_word(uint16_t u)
{
    int lo = 0, hi = PC98_JIS_N - 1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        if (pc98_jis_u[mid] == u) return pc98_jis_v[mid];
        if (pc98_jis_u[mid] < u) lo = mid + 1; else hi = mid - 1;
    }
    return -1;
}

void txt_init(void)
{
    geta = (uint16_t)jis_word(0x3013);
    txt_clear(0, 24, TA_WHITE);        /* ★VRAM は 25 行ぶん消す（DOS に返ったあとも残らないように） */
#ifndef PC98_HOST
    bios18(0x12, 0, 0);                /* カーソルを隠す */
    rows24();
#endif
}

void txt_fini(void)
{
#ifndef PC98_HOST
    bios18(0x0A, 0x00, 0);             /* 80 桁 × 25 行に戻す */
#endif
    txt_clear(0, 24, TA_WHITE);
#ifndef PC98_HOST
    bios18(0x13, 0, 0);                /* カーソルを左上へ戻して出す */
    bios18(0x11, 0, 0);
#endif
}

int txt_cells(uint16_t u)
{
    return u < 0x80 ? 1 : 2;
}

int txt_put(int row, int col, uint16_t u, uint8_t attr)
{
    const int p = row * TXT_COLS + col;
    if (u < 0x80) {
        VCODE[p] = u < 0x20 ? 0x20 : u;
        VATTR[p] = attr;
        return 1;
    }
    if (col + 1 >= TXT_COLS)
        return 0;
    int w = jis_word(u);
    if (w < 0) w = geta;
    VCODE[p] = (uint16_t)w;
    VCODE[p + 1] = (uint16_t)(w | 0x80);
    VATTR[p] = VATTR[p + 1] = attr;
    return 2;
}

void txt_clear(int row0, int row1, uint8_t attr)
{
    for (int p = row0 * TXT_COLS; p < (row1 + 1) * TXT_COLS; p++) {
        VCODE[p] = 0x20;
        VATTR[p] = attr;
    }
}

void txt_vsync(void)
{
#ifndef PC98_HOST
    while (inp(0x60) & 0x20) { }       /* GDC の状態: bit5 = 垂直帰線中 */
    while (!(inp(0x60) & 0x20)) { }
#endif
}
