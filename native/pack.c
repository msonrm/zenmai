/* 作品の一覧を作り、選んだ作品を開く。規則は pack.h、パックの書式は gen_pack.py。 */
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

enum { SEC_MAX = 16, INFO_MAX = 1024, NAMES_MAX = 64 };

/* 表の登録（ctab.py が作る <mod>_tab.c） */
extern const TlTab translate_tabs[], cmd_tabs[], ruby_tabs[];
extern const int translate_tabs_n, cmd_tabs_n, ruby_tabs_n;
extern const unsigned long translate_schema, cmd_schema, ruby_schema;
static const struct { const char *id; const TlTab *tabs; const int *n; const unsigned long *schema; } TABS[] = {
    { "TRAN", translate_tabs, &translate_tabs_n, &translate_schema },
    { "CMDS", cmd_tabs, &cmd_tabs_n, &cmd_schema },
    { "RUBY", ruby_tabs, &ruby_tabs_n, &ruby_schema },
};
enum { TABS_N = 3 };

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static int read_at(FILE *f, uint32_t off, void *dst, uint32_t len)
{
    return fseek(f, (long)off, SEEK_SET) == 0 && fread(dst, 1, len, f) == len;
}

/* ---- パックの索引 ---- */

typedef struct {
    uint8_t idx[SEC_MAX * 12];
    int n;
} Index;

/* パックの頭と索引を読む。パックなら 1 */
static int read_index(FILE *f, Index *x)
{
    uint8_t head[8];
    if (fread(head, 1, 8, f) != 8 || memcmp(head, "ZMPK", 4) || (head[4] | head[5] << 8) != 1)
        return 0;
    x->n = head[6] | head[7] << 8;
    return x->n >= 1 && x->n <= SEC_MAX && fread(x->idx, 1, (size_t)x->n * 12, f) == (size_t)x->n * 12;
}

/* 節を探す。在れば位置と長さを入れて 1 */
static int find_sec(const Index *x, const char *id, uint32_t *off, uint32_t *len)
{
    for (int i = 0; i < x->n; i++) {
        if (!memcmp(x->idx + i * 12, id, 4)) {
            *off = le32(x->idx + i * 12 + 4);
            *len = le32(x->idx + i * 12 + 8);
            return 1;
        }
    }
    return 0;
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

/* ---- ディレクトリ ---- */

static char names[NAMES_MAX][13];
static int nnames;

/* カレントディレクトリのファイルの名前を集めて、名前の順に並べる */
static void list_dir(void)
{
    nnames = 0;
#ifdef PC98_HOST
    DIR *d = opendir(".");
    struct dirent *e;
    while (d && nnames < NAMES_MAX && (e = readdir(d)))
        if (e->d_name[0] != '.' && strlen(e->d_name) <= 12)
            strcpy(names[nnames++], e->d_name);
    if (d)
        closedir(d);
#else
    struct find_t ft;
    for (unsigned r = _dos_findfirst("*.*", _A_NORMAL | _A_RDONLY, &ft); !r && nnames < NAMES_MAX; r = _dos_findnext(&ft))
        strcpy(names[nnames++], ft.name);
    _dos_findclose(&ft);
#endif
    for (int i = 1; i < nnames; i++)       /* 挿し込み整列（数十個なので） */
        for (int j = i; j > 0 && strcmp(names[j - 1], names[j]) > 0; j--) {
            char t[13];
            strcpy(t, names[j]);
            strcpy(names[j], names[j - 1]);
            strcpy(names[j - 1], t);
        }
}

static int has_ext(const char *name, const char *ext)
{
    const char *dot = strrchr(name, '.');
    if (!dot || strlen(dot) != strlen(ext))
        return 0;
    for (int i = 0; dot[i]; i++) {
        const char c = dot[i] >= 'a' && dot[i] <= 'z' ? (char)(dot[i] - 32) : dot[i];
        if (c != ext[i])
            return 0;
    }
    return 1;
}

static int is_story_name(const char *name) { return has_ext(name, ".Z3") || has_ext(name, ".DAT"); }

/* 名前（拡張子なし・大文字・8 字まで） */
static void set_base(char *base, const char *name)
{
    int n = 0;
    while (name[n] && name[n] != '.' && n < 8) {
        const char c = name[n];
        base[n++] = (char)(c >= 'a' && c <= 'z' ? c - 32 : c);
    }
    base[n] = '\0';
}

/* ---- story ---- */

/* story のヘッダ（先頭 64 バイト）を読む。★版 3 だけ（MojoZork が版 3 の処理系なので） */
static int story_header(const char *name, uint8_t *h)
{
    FILE *f = fopen(name, "rb");
    if (!f)
        return 0;
    const int ok = fread(h, 1, 64, f) == 64;
    fclose(f);
    return ok && h[0] == 3;
}

static int story_matches(const char *name, const ZmPack *pk)
{
    uint8_t h[64];
    return story_header(name, h)
        && (h[2] << 8 | h[3]) == pk->release
        && !memcmp(h + 0x12, pk->serial, 6)
        && (h[0x1C] << 8 | h[0x1D]) == pk->checksum;
}

/* パックに合う story を探す: INFO の story= → パックと同じ名前の .Z3 → 全部の .Z3 / .DAT */
static void find_story(ZmPack *pk, const char *hint)
{
    char name[13];
    snprintf(name, sizeof name, "%s.Z3", pk->base);
    if (hint[0] && strlen(hint) <= 12 && story_matches(hint, pk))
        strcpy(pk->story, hint);
    else if (story_matches(name, pk))
        strcpy(pk->story, name);
    else
        for (int i = 0; i < nnames && !pk->story[0]; i++)
            if (is_story_name(names[i]) && story_matches(names[i], pk))
                strcpy(pk->story, names[i]);
}

/* パックを 1 つ調べる（story と表はまだ読まない）。パックとして読めれば 1 */
static int probe_pack(const char *name, ZmPack *pk)
{
    static Index x;
    static char info[INFO_MAX + 1];
    uint8_t id[10];
    uint32_t off, len;
    char hint[13];
    FILE *f = fopen(name, "rb");
    if (!f)
        return 0;
    int ok = read_index(f, &x) && find_sec(&x, "IDNT", &off, &len) && len == 10 && read_at(f, off, id, 10);
    info[0] = '\0';
    if (ok && find_sec(&x, "INFO", &off, &len) && len <= INFO_MAX && read_at(f, off, info, len))
        info[len] = '\0';
    fclose(f);
    if (!ok)
        return 0;
    memset(pk, 0, sizeof *pk);
    strcpy(pk->pack, name);
    set_base(pk->base, name);
    pk->release = (uint16_t)(id[0] | id[1] << 8);
    memcpy(pk->serial, id + 2, 6);
    pk->checksum = (uint16_t)(id[8] | id[9] << 8);
    info_get(info, "title", pk->title, sizeof pk->title);
    if (!pk->title[0])
        strcpy(pk->title, name);
    pk->has_ja = 1;
    for (int i = 0; i < TABS_N; i++)
        if (!find_sec(&x, TABS[i].id, &off, &len))
            pk->has_ja = 0;
    info_get(info, "story", hint, sizeof hint);
    find_story(pk, hint);
    return 1;
}

int pack_list(ZmPack *w, int max)
{
    int n = 0;
    list_dir();
    for (int i = 0; i < nnames && n < max; i++)
        if (has_ext(names[i], ".ZMP") && probe_pack(names[i], &w[n]))
            n++;
    const int npack = n;
    /* ★どのパックにも使われていない story は、英語だけの作品として並べる */
    for (int i = 0; i < nnames && n < max; i++) {
        uint8_t h[64];
        int used = 0;
        for (int k = 0; k < npack; k++)
            used |= !strcmp(w[k].story, names[i]);
        if (used || !is_story_name(names[i]) || !story_header(names[i], h))
            continue;
        ZmPack *pk = &w[n++];
        memset(pk, 0, sizeof *pk);
        strcpy(pk->story, names[i]);
        strcpy(pk->title, names[i]);
        set_base(pk->base, names[i]);
        pk->release = (uint16_t)(h[2] << 8 | h[3]);
        memcpy(pk->serial, h + 0x12, 6);
        pk->checksum = (uint16_t)(h[0x1C] << 8 | h[0x1D]);
    }
    return n;
}

/* ---- 開く ---- */

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

const char *pack_open(ZmPack *pk, int ja)
{
    static char msg[96];
    static Index x;
    uint32_t off, len;
    if (!pk->story[0]) {
        /* ★どの story を置けばよいかを言う（題は UTF-8 なので DOS の画面に出せない。識別で言う） */
        snprintf(msg, sizeof msg, "no story file (release %u / serial %s) in this directory",
                 (unsigned)pk->release, pk->serial);
        return msg;
    }
    const char *err = load_story(pk);
    if (err || !ja)
        return err;
    /* ★訳・語彙・ふりがなの表（段 8 の B）。本体には焼き込んでいない。英語で遊ぶなら読まない */
    FILE *f = fopen(pk->pack, "rb");
    if (!f || !read_index(f, &x))
        err = "cannot read the pack";
    for (int i = 0; i < TABS_N && !err; i++) {
        if (!find_sec(&x, TABS[i].id, &off, &len))
            err = "no Japanese tables";
        else
            err = tab_load(f, off, len, TABS[i].tabs, *TABS[i].n, *TABS[i].schema);
    }
    if (f)
        fclose(f);
    return err;
}
