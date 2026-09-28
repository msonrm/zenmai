/* キーボードのかな入力。規則は kana_input.h、表は kana_input_data.c（Mozc の表から生成）。
 *
 * ローマ字の組み立ては Mozc の表の読み方と同じ:
 *   組み立て途中 + 打った字 が
 *     - 表のどれかの**頭**になっている → まだ待つ（`n` は `na` の頭なので待つ）
 *     - 表のどれかと**一致**し、それより長い行の頭ではない → 出す。その行の「残す列」を次の組み立てに
 *     - どちらでもない → 組み立て途中をまず片づけ（一致すれば出す、しなければ頭の 1 字を ASCII のまま）、
 *       打った字でやり直す（`nk` → ん + k）
 */
#include <string.h>
#include "kana_input.h"
#include "kana_input_data.h"

static const KiRomaji *exact(const char *s, int n)
{
    for (int i = 0; i < KI_ROMAJI_N; i++)
        if ((int)strlen(ki_romaji[i].in) == n && !memcmp(ki_romaji[i].in, s, (size_t)n))
            return &ki_romaji[i];
    return 0;
}

static int is_head(const char *s, int n)   /* s[0..n) より長い行の頭か */
{
    for (int i = 0; i < KI_ROMAJI_N; i++)
        if ((int)strlen(ki_romaji[i].in) > n && !memcmp(ki_romaji[i].in, s, (size_t)n))
            return 1;
    return 0;
}

static void put(KiLine *l, uint16_t c)
{
    if (c && l->n < KI_MAX)
        l->buf[l->n++] = c;
}

static void put_out(KiLine *l, const KiRomaji *e)
{
    for (int k = 0; k < 3 && e->out[k]; k++)
        put(l, e->out[k]);
    l->np = (int)strlen(e->pend);
    memcpy(l->pend, e->pend, (size_t)l->np);
}

/* 組み立て途中を 1 段だけ片づける。★一致すれば出して「残す列」を残す（`ww` → っ + w）ので、
   出し切るには空になるまで呼ぶ */
static void settle_once(KiLine *l)
{
    const KiRomaji *e = exact(l->pend, l->np);
    if (e) {
        put_out(l, e);
        return;
    }
    put(l, (uint16_t)(unsigned char)l->pend[0]);
    memmove(l->pend, l->pend + 1, (size_t)--l->np);
}

static void romaji(KiLine *l, char c)
{
    for (int guard = 0; guard < 8; guard++) {
        char s[sizeof l->pend + 1];
        memcpy(s, l->pend, (size_t)l->np);
        s[l->np] = c;
        const int n = l->np + 1;
        if (n < (int)sizeof l->pend && is_head(s, n)) {
            memcpy(l->pend, s, (size_t)n);
            l->np = n;
            return;
        }
        const KiRomaji *e = exact(s, n);
        if (e) {
            put_out(l, e);
            return;
        }
        if (!l->np) {                  /* 表に無い字: そのまま */
            put(l, (uint16_t)(unsigned char)c);
            return;
        }
        settle_once(l);                /* 途中を片づけて、打った字でやり直す */
    }
}

static int in_table(char c)            /* 表の入力に現れる字か */
{
    for (int i = 0; i < KI_ROMAJI_N; i++)
        if (strchr(ki_romaji[i].in, c))
            return 1;
    return 0;
}

void ki_clear(KiLine *l)
{
    l->n = 0;
    l->np = 0;
}

void ki_commit(KiLine *l)
{
    while (l->np)
        settle_once(l);
}

void ki_key(KiLine *l, int c, int english)
{
    if (c >= 0xA1 && c <= 0xDF) {      /* カナキー（半角カナ） */
        ki_commit(l);
        if ((c == 0xDE || c == 0xDF) && l->n) {
            const uint16_t prev = l->buf[l->n - 1];
            for (int i = 0; i < KI_DAKUTEN_N; i++)
                if (ki_dakuten[i].base == prev && ki_dakuten[i].mark == (c == 0xDE ? 1 : 2)) {
                    l->buf[l->n - 1] = ki_dakuten[i].to;
                    return;
                }
        }
        put(l, ki_hankana[c - 0xA1]);
        return;
    }
    if (c < 0x20 || c > 0x7E)
        return;
    if (english || !in_table((char)c)) {
        ki_commit(l);
        put(l, (uint16_t)c);
        return;
    }
    romaji(l, (char)c);
}

void ki_backspace(KiLine *l)
{
    if (l->np)
        l->np--;
    else if (l->n)
        l->n--;
}
