/* PC-98 の .MAG の読み込み。規則は pc98_mag.h。 */
#include "pc98_mag.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { FILE_MAX = 65536, DIM_MAX = 1024 };

/* 近傍コピーの元: 現在位置 - (行 × 1 行のバイト数 + 語 × 2)。0 番は新しい語を読む */
static const signed char copy_dy[16] = { 0, 0, 0, 0, 1, 1, 2, 2, 2, 4, 4, 4, 8, 8, 8, 16 };
static const signed char copy_dx[16] = { 0, 1, 2, 4, 0, 1, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0 };

static unsigned rd16(const uint8_t *b) { return (unsigned)(b[0] | b[1] << 8); }
static unsigned long rd32(const uint8_t *b) { return (unsigned long)rd16(b) | (unsigned long)rd16(b + 2) << 16; }

int mag_load(const char *path, Mag *m)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    uint8_t *buf = (uint8_t *)malloc(FILE_MAX);
    if (!buf) { fclose(f); return 0; }
    const long len = (long)fread(buf, 1, FILE_MAX, f);
    fclose(f);
    int ok = 0;
    uint8_t *px = 0;
    if (len < 8 + 1 + 32 || memcmp(buf, "MAKI02  ", 8)) goto done;
    long p = 8;
    while (p < len && buf[p] != 0x1A) p++;
    const long H = p + 1;                         /* ヘッダ先頭（オフセットはここから） */
    if (H + 32 > len) goto done;
    const uint8_t *h = buf + H;
    if (h[3] & 0x80) goto done;                   /* 256 色は読まない */
    const int x0 = (int)rd16(h + 4), y0 = (int)rd16(h + 6), x1 = (int)rd16(h + 8), y1 = (int)rd16(h + 10);
    const long offA = (long)rd32(h + 12), offB = (long)rd32(h + 16), offP = (long)rd32(h + 24);
    const int units = (x1 >> 3) - (x0 >> 3) + 1, lines = y1 - y0 + 1;
    if (units <= 0 || lines <= 0 || units * 8 > DIM_MAX || lines > DIM_MAX) goto done;
    const int bw = units * 4;                     /* 1 行のバイト数 */
    px = (uint8_t *)calloc((size_t)bw * (size_t)lines, 1);
    if (!px) goto done;

    memset(m->pal, 0, sizeof m->pal);
    for (int i = 0; i < 16 && 32 + (i + 1) * 3 <= offA; i++)
        for (int k = 0; k < 3; k++)
            m->pal[i][k] = (uint8_t)(H + 32 + i * 3 + k < len ? buf[H + 32 + i * 3 + k] >> 4 : 0);

    long ac = H + offA, bc = H + offB, pc = H + offP;
    int abits = 0, abyte = 0;
    uint8_t *flag = (uint8_t *)calloc((size_t)units, 1);
    if (!flag) { free(px); px = 0; goto done; }
#define AT(i) ((i) >= 0 && (i) < len ? buf[i] : 0)
    for (int y = 0; y < lines; y++) {
        for (int c = 0; c < units; c++) {
            if (abits == 0) { abyte = AT(ac); ac++; abits = 8; }
            const int bit = abyte >> 7 & 1;
            abyte = abyte << 1 & 0xFF;
            abits--;
            if (bit) { flag[c] ^= (uint8_t)AT(bc); bc++; }
        }
        for (int c = 0; c < units; c++) {
            const int pos = y * bw + c * 4;
            for (int half = 0; half < 2; half++) {
                const int n = half == 0 ? flag[c] >> 4 : flag[c] & 15, dst = pos + half * 2;
                if (n == 0) {
                    px[dst] = (uint8_t)AT(pc); pc++;
                    px[dst + 1] = (uint8_t)AT(pc); pc++;
                } else {
                    const int s = dst - (copy_dy[n] * bw + copy_dx[n] * 2);
                    if (s >= 0) { px[dst] = px[s]; px[dst + 1] = px[s + 1]; }
                }
            }
        }
    }
#undef AT
    free(flag);
    m->w = units * 8;
    m->h = lines;
    m->px = px;
    ok = 1;
done:
    free(buf);
    if (!ok) free(px);
    return ok;
}

void mag_free(Mag *m)
{
    free(m->px);
    m->px = 0;
}
