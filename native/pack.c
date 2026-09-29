/* 作品の束（パック）を読む。規則は pack.h、書式は gen_pack.py。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pack.h"

enum { SEC_MAX = 16 };

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

/* 節を探す。在れば位置と長さを入れて 1 */
static int find_sec(const uint8_t *idx, int n, const char *id, uint32_t *off, uint32_t *len)
{
    for (int i = 0; i < n; i++) {
        if (!memcmp(idx + i * 12, id, 4)) {
            *off = le32(idx + i * 12 + 4);
            *len = le32(idx + i * 12 + 8);
            return 1;
        }
    }
    return 0;
}

static int read_at(FILE *f, uint32_t off, void *dst, uint32_t len)
{
    return fseek(f, (long)off, SEEK_SET) == 0 && fread(dst, 1, len, f) == len;
}

const char *pack_load(const char *path, ZmPack *pk)
{
    static uint8_t idx[SEC_MAX * 12];
    uint8_t head[8];
    uint32_t off, len;
    const char *err = 0;
    memset(pk, 0, sizeof *pk);
    FILE *f = fopen(path, "rb");
    if (!f)
        return "cannot open";
    const int n = fread(head, 1, 8, f) == 8 ? head[6] | head[7] << 8 : 0;
    if (memcmp(head, "ZMPK", 4) || (head[4] | head[5] << 8) != 1 || n < 1 || n > SEC_MAX
        || fread(idx, 1, (size_t)n * 12, f) != (size_t)n * 12)
        err = "not a Zenmai pack";
    else if (!find_sec(idx, n, "STRY", &off, &len) || len < 64)
        err = "no story";
    else if (!(pk->ram = malloc(len)))
        err = "not enough memory";
    else if (!read_at(f, off, pk->ram, len))
        err = "cannot read the story";
    else if (pk->ram[0] != 3)
        err = "not a version 3 story";
    if (!err) {
        /* ★差分の相手は動的領域（先頭からヘッダ 0Eh の値まで）だけあれば足りる（session.c の pack_state） */
        const uint32_t dyn = (uint32_t)pk->ram[0x0E] << 8 | pk->ram[0x0F];
        pk->len = len;
        if (dyn > len)
            err = "broken story header";
        else if (!(pk->init = malloc(dyn)))
            err = "not enough memory";
        else
            memcpy(pk->init, pk->ram, dyn);
    }
    if (!err && find_sec(idx, n, "NAME", &off, &len)) {
        if (len >= sizeof pk->name)
            len = sizeof pk->name - 1;
        if (!read_at(f, off, pk->name, len))
            err = "cannot read the name";
    }
    fclose(f);
    if (err) {
        free(pk->ram);
        free(pk->init);
        memset(pk, 0, sizeof *pk);
    }
    return err;
}
