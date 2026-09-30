/* PC-98 の曲（PMD に頼む）。規則は pc98_music.h。 */
#include "pc98_music.h"

#ifdef PC98_HOST
void music_start(const char *file) { (void)file; }
void music_stop(void) { }
#else
#include <stdio.h>
#include <string.h>
#include <i86.h>
#include <dos.h>

enum { PMD_START = 0x00, PMD_STOP = 0x01, PMD_FADE = 0x02, PMD_MUSDAT = 0x06, PMD_SIZE = 0x22 };
enum { FADE_SPEED = 12 };               /* 1 = いちばん遅い */

static unsigned pmd_seg;               /* 常駐している PMD のセグメント（0 = いない） */
static int playing;

static unsigned char *lin(unsigned seg, unsigned off) { return (unsigned char *)((seg << 4) + off); }

static union REGS pmd(int ah, int al)
{
    union REGS r;
    memset(&r, 0, sizeof r);
    r.h.ah = (unsigned char)ah;
    r.h.al = (unsigned char)al;
    int386(0x60, &r, &r);
    return r;
}

/* INT 60h のベクタ（実モード）が PMD を指しているか。DPMI 0200h で実モードのベクタを取り、+2 の "PMD" を見る */
static unsigned find_pmd(void)
{
    union REGS r;
    memset(&r, 0, sizeof r);
    r.w.ax = 0x0200;
    r.h.bl = 0x60;
    int386(0x31, &r, &r);
    if (r.x.cflag || (!r.w.cx && !r.w.dx)) return 0;
    const unsigned char *p = lin(r.w.cx, r.w.dx);
    return p[2] == 'P' && p[3] == 'M' && p[4] == 'D' ? r.w.cx : 0;
}

void music_start(const char *file)
{
    pmd_seg = find_pmd();
    if (!pmd_seg) return;
    FILE *f = fopen(file, "rb");
    if (!f) return;
    pmd(PMD_STOP, 0);
    const union REGS sz = pmd(PMD_SIZE, 0);            /* AL = 曲の枠（KB） */
    const union REGS at = pmd(PMD_MUSDAT, 0);          /* DX = 曲を置く場所（★DS は使わない） */
    const size_t cap = (size_t)sz.h.al * 1024 - 1;
    const size_t n = fread(lin(pmd_seg, at.w.dx), 1, cap + 1, f);
    fclose(f);
    if (!n || n > cap) return;                         /* 枠に入らない曲は鳴らさない */
    pmd(PMD_START, 0);
    playing = 1;
}

void music_stop(void)
{
    if (!playing) return;
    playing = 0;
    pmd(PMD_FADE, FADE_SPEED);
}
#endif
