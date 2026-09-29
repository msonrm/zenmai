/* パックの節から表を読む。規則は tabload.h、節の書式は ctab.py。 */
#include <stdlib.h>
#include <string.h>
#include "tabload.h"

enum { IDX_SIZE = 28, TABS_MAX = 32 };

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

/* 1 行（欄を詰めたもの）を C の構造体へ。★リトルエンディアンの機械（x86・ホスト）なので欄はそのまま写せる */
static void unpack_row(uint8_t *dst, const uint8_t *src, const TlTab *t)
{
    for (int i = 0; i < t->nf; i++) {
        memcpy(dst + t->f[i].off, src, t->f[i].size);
        src += t->f[i].size;
    }
}

const char *tab_load(FILE *f, uint32_t off, uint32_t len,
                     const TlTab *tabs, int n, unsigned long schema)
{
    static uint8_t idx[TABS_MAX * IDX_SIZE];
    uint8_t head[8];
    if (len < 8 || fseek(f, (long)off, SEEK_SET) || fread(head, 1, 8, f) != 8)
        return "cannot read the tables";
    const int m = head[4] | head[5] << 8;
    if (le32(head) != (uint32_t)schema)
        return "the tables are for another version of Zenmai";
    if (m > TABS_MAX || fread(idx, 1, (size_t)m * IDX_SIZE, f) != (size_t)m * IDX_SIZE)
        return "broken tables";
    for (int k = 0; k < n; k++) {
        const TlTab *t = &tabs[k];
        const uint8_t *e = 0;
        for (int i = 0; i < m && !e; i++)
            if (!strncmp((const char *)idx + i * IDX_SIZE, t->name, 16))
                e = idx + i * IDX_SIZE;
        if (!e)
            return "a table is missing";
        const uint32_t rows = le32(e + 16), at = le32(e + 20);
        const unsigned rs = e[24] | e[25] << 8;
        unsigned want = 0;
        for (int i = 0; i < t->nf; i++)
            want += t->f[i].size;
        if (rs != want || at + (uint64_t)rows * rs > len)
            return "broken tables";
        uint8_t *dst = malloc(rows ? (size_t)rows * t->elem : 1);
        if (!dst)
            return "not enough memory";
        if (fseek(f, (long)(off + at), SEEK_SET))
            return "cannot read the tables";
        if (rs == t->elem && t->nf == 1) {
            /* 値の列（文字の溜め）: 詰め物が無いのでそのまま読む */
            if (fread(dst, rs, rows, f) != rows)
                return "cannot read the tables";
        } else {
            /* 構造体の列: 一時の溜めに読んで 1 行ずつ展開する */
            uint8_t *src = malloc(rows ? (size_t)rows * rs : 1);
            if (!src)
                return "not enough memory";
            if (fread(src, rs, rows, f) != rows) {
                free(src);
                return "cannot read the tables";
            }
            memset(dst, 0, (size_t)rows * t->elem);
            for (uint32_t r = 0; r < rows; r++)
                unpack_row(dst + r * t->elem, src + r * rs, t);
            free(src);
        }
        *t->ptr = dst;
        *t->count = rows;
    }
    return 0;
}
