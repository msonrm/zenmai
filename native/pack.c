/* 作品の束（パック）を読み、合う story を探して読む。規則は pack.h、書式は gen_pack.py。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pack.h"
#include "tabload.h"
#ifdef PC98_HOST
#include <dirent.h>
#else
#include <dos.h>
#endif

enum { SEC_MAX = 16, INFO_MAX = 1024 };

/* 表の登録（ctab.py が作る <mod>_tab.c） */
extern const TlTab translate_tabs[], cmd_tabs[], ruby_tabs[];
extern const int translate_tabs_n, cmd_tabs_n, ruby_tabs_n;
extern const unsigned long translate_schema, cmd_schema, ruby_schema;

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

/* INFO（key=value の行）から 1 つ取り出す。無ければ空 */
static void info_get(const char *info, const char *key, char *dst, int max)
{
    const int kl = (int)strlen(key);
    dst[0] = '\0';
    for (const char *p = info; *p; ) {
        const char *e = strchr(p, '\n');
        const int n = e ? (int)(e - p) : (int)strlen(p);
        if (n > kl && p[kl] == '=' && !memcmp(p, key, (size_t)kl)) {
            int vl = n - kl - 1;
            if (vl > max - 1) vl = max - 1;
            memcpy(dst, p + kl + 1, (size_t)vl);
            dst[vl] = '\0';
            return;
        }
        p += n + (e ? 1 : 0);
    }
}

/* ---- story を探す ---- */

/* story のヘッダ（先頭 64 バイト）が識別と合えば 1 */
static int story_matches(const char *name, const ZmPack *pk)
{
    uint8_t h[64];
    FILE *f = fopen(name, "rb");
    if (!f)
        return 0;
    const int ok = fread(h, 1, sizeof h, f) == sizeof h;
    fclose(f);
    return ok && h[0] == 3
        && (h[2] << 8 | h[3]) == pk->release
        && !memcmp(h + 0x12, pk->serial, 6)
        && (h[0x1C] << 8 | h[0x1D]) == pk->checksum;
}

static int is_story_name(const char *name)
{
    const char *dot = strrchr(name, '.');
    if (!dot || strlen(name) > 12)
        return 0;
    char ext[5] = { 0 };
    for (int i = 0; i < 4 && dot[i]; i++)
        ext[i] = (char)(dot[i] >= 'a' && dot[i] <= 'z' ? dot[i] - 32 : dot[i]);
    return !strcmp(ext, ".Z3") || !strcmp(ext, ".DAT");
}

/* カレントディレクトリの .Z3 / .DAT から識別の合うものを探す。見つかれば pk->story に入れて 1 */
static int scan_stories(ZmPack *pk)
{
    int found = 0;
#ifdef PC98_HOST
    DIR *d = opendir(".");
    if (!d)
        return 0;
    struct dirent *e;
    while (!found && (e = readdir(d)))
        if (is_story_name(e->d_name) && story_matches(e->d_name, pk)) {
            strcpy(pk->story, e->d_name);
            found = 1;
        }
    closedir(d);
#else
    struct find_t ft;
    for (unsigned r = _dos_findfirst("*.*", _A_NORMAL | _A_RDONLY, &ft); !r && !found; r = _dos_findnext(&ft))
        if (is_story_name(ft.name) && story_matches(ft.name, pk)) {
            strcpy(pk->story, ft.name);
            found = 1;
        }
    _dos_findclose(&ft);
#endif
    return found;
}

static int find_story(ZmPack *pk, const char *hint)
{
    char name[13];
    if (hint[0] && strlen(hint) <= 12 && story_matches(hint, pk)) {
        strcpy(pk->story, hint);
        return 1;
    }
    snprintf(name, sizeof name, "%s.Z3", pk->base);
    if (story_matches(name, pk)) {
        strcpy(pk->story, name);
        return 1;
    }
    return scan_stories(pk);
}

/* story を読んで、作業域と動的領域の控えを作る */
static const char *load_story(ZmPack *pk)
{
    FILE *f = fopen(pk->story, "rb");
    if (!f)
        return "cannot open the story";
    const char *err = 0;
    long len = 0;
    if (fseek(f, 0, SEEK_END) || (len = ftell(f)) < 64 || fseek(f, 0, SEEK_SET))
        err = "cannot read the story";
    else if (!(pk->ram = malloc((size_t)len)))
        err = "not enough memory";
    else if (fread(pk->ram, 1, (size_t)len, f) != (size_t)len)
        err = "cannot read the story";
    fclose(f);
    if (err)
        return err;
    /* ★差分の相手は動的領域（先頭からヘッダ 0Eh の値まで）だけあれば足りる（session.c の pack_state） */
    const uint32_t dyn = (uint32_t)pk->ram[0x0E] << 8 | pk->ram[0x0F];
    pk->len = (uint32_t)len;
    if (dyn > pk->len)
        return "broken story header";
    if (!(pk->init = malloc(dyn)))
        return "not enough memory";
    memcpy(pk->init, pk->ram, dyn);
    return 0;
}

/* パックの名前（拡張子なし・大文字・8 字まで）を path から */
static void set_base(ZmPack *pk, const char *path)
{
    const char *s = path;
    for (const char *p = path; *p; p++)
        if (*p == '/' || *p == '\\' || *p == ':')
            s = p + 1;
    int n = 0;
    while (s[n] && s[n] != '.' && n < 8) {
        const char c = s[n];
        pk->base[n++] = (char)(c >= 'a' && c <= 'z' ? c - 32 : c);
    }
    pk->base[n] = '\0';
}

const char *pack_load(const char *path, ZmPack *pk)
{
    static uint8_t idx[SEC_MAX * 12];
    static char info[INFO_MAX + 1];
    static char msg[96];
    uint8_t head[8], id[10];
    uint32_t off, len;
    const char *err = 0;
    char hint[13];
    memset(pk, 0, sizeof *pk);
    set_base(pk, path);
    FILE *f = fopen(path, "rb");
    if (!f)
        return "cannot open";
    const int n = fread(head, 1, 8, f) == 8 ? head[6] | head[7] << 8 : 0;
    info[0] = '\0';
    if (memcmp(head, "ZMPK", 4) || (head[4] | head[5] << 8) != 1 || n < 1 || n > SEC_MAX
        || fread(idx, 1, (size_t)n * 12, f) != (size_t)n * 12)
        err = "not a Zenmai pack";
    else if (!find_sec(idx, n, "IDNT", &off, &len) || len != 10 || !read_at(f, off, id, 10))
        err = "no story identity";
    else if (find_sec(idx, n, "INFO", &off, &len)) {    /* INFO は無くてもよい */
        if (len > INFO_MAX || !read_at(f, off, info, len))
            err = "cannot read the info";
        else
            info[len] = '\0';
    }
    fclose(f);
    if (err)
        return err;
    pk->release = (uint16_t)(id[0] | id[1] << 8);
    memcpy(pk->serial, id + 2, 6);
    pk->checksum = (uint16_t)(id[8] | id[9] << 8);
    info_get(info, "title", pk->title, sizeof pk->title);
    info_get(info, "story", hint, sizeof hint);
    if (!find_story(pk, hint)) {
        /* ★どの story を置けばよいかを言う（題は UTF-8 なので DOS の画面に出せない。識別で言う） */
        snprintf(msg, sizeof msg, "no story file (release %u / serial %s) in this directory",
                 (unsigned)pk->release, pk->serial);
        return msg;
    }
    if ((err = load_story(pk))) {
        free(pk->ram);
        free(pk->init);
        pk->ram = pk->init = 0;
        return err;
    }
    /* ★訳・語彙・ふりがなの表（段 8 の B）。本体には焼き込んでいない */
    static const struct { const char *id; const TlTab *tabs; const int *n; const unsigned long *schema; } T[] = {
        { "TRAN", translate_tabs, &translate_tabs_n, &translate_schema },
        { "CMDS", cmd_tabs, &cmd_tabs_n, &cmd_schema },
        { "RUBY", ruby_tabs, &ruby_tabs_n, &ruby_schema },
    };
    if (!(f = fopen(path, "rb")))
        return "cannot open";
    for (int i = 0; i < 3 && !err; i++) {
        if (!find_sec(idx, n, T[i].id, &off, &len))
            err = "no Japanese tables";
        else
            err = tab_load(f, off, len, T[i].tabs, *T[i].n, *T[i].schema);
    }
    fclose(f);
    return err;
}
