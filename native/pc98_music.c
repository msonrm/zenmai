/* PC-98 の曲（PMD に頼む）と、部屋ごとの割り当て（INI）。規則は pc98_music.h。 */
#include "pc98_music.h"
#include <stdio.h>
#include <string.h>

/* ---- 設定（INI）---- */
enum { MAP_N = 96, KEY_N = 40, VAL_N = 16, LINE_N = 160 };
typedef struct { char key[KEY_N]; char val[VAL_N]; unsigned char prefix; } Map;
static Map music_map[MAP_N], pic_map[MAP_N];
static int nmusic, npic;
static int music_on = 1;
static char title_file[VAL_N] = "CANON.M";
static char cur_music[VAL_N], cur_pic[VAL_N];

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

static int ieq(const char *a, const char *b, int n)      /* 先頭 n 字が（大文字小文字を除いて）同じか */
{
    for (int i = 0; i < n; i++)
        if (lower((unsigned char)a[i]) != lower((unsigned char)b[i])) return 0;
    return 1;
}

static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
    return s;
}

static void add(Map *m, int *n, const char *key, const char *val)
{
    if (*n >= MAP_N) return;
    Map *p = &m[*n];
    const int kl = (int)strlen(key);
    if (kl >= KEY_N || strlen(val) >= VAL_N || !kl) return;
    strcpy(p->key, key);
    strcpy(p->val, val);
    p->prefix = p->key[kl - 1] == '*';
    if (p->prefix) p->key[kl - 1] = 0;           /* 前方一致の `*` は外して持つ（`*` だけなら空 = すべてに合う） */
    (*n)++;
}

/* section = 0: [zenmai] / 1: [music] / 2: [picture]。ファイルが無ければ何もしない */
static void read_ini(const char *path, int zenmai_only)
{
    FILE *f = fopen(path, "r");
    if (!f) return;
    char line[LINE_N];
    int sec = -1;
    while (fgets(line, sizeof line, f)) {
        char *semi = strchr(line, ';');
        if (semi) *semi = 0;
        char *s = trim(line);
        if (!*s) continue;
        if (*s == '[') {
            char *e = strchr(s, ']');
            if (e) *e = 0;
            s++;
            sec = ieq(s, "zenmai", 6) && !s[6] ? 0 : ieq(s, "music", 5) && !s[5] ? 1 : ieq(s, "picture", 7) && !s[7] ? 2 : -1;
            continue;
        }
        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = 0;
        char *k = trim(s), *v = trim(eq + 1);
        if (zenmai_only) {
            if (sec != 0) continue;
            if (ieq(k, "music", 5) && !k[5]) music_on = !(ieq(v, "off", 3) && !v[3]);
            else if (ieq(k, "title", 5) && !k[5] && strlen(v) < VAL_N) strcpy(title_file, strcmp(v, "-") ? v : "");
        } else if (sec == 1) {
            add(music_map, &nmusic, k, v);
        } else if (sec == 2) {
            add(pic_map, &npic, k, v);
        }
    }
    fclose(f);
}

void music_config(void) { read_ini("ZENMAI.INI", 1); }

const char *music_title_file(void) { return music_on ? title_file : ""; }

void music_work(const char *base)
{
    char path[32];
    nmusic = npic = 0;
    cur_music[0] = cur_pic[0] = 0;
    snprintf(path, sizeof path, "%s.INI", base);
    read_ini(path, 0);
}

static const char *lookup(const Map *m, int n, const char *name, int nl)
{
    for (int i = 0; i < n; i++) {
        const int kl = (int)strlen(m[i].key);
        if (m[i].prefix ? nl >= kl && ieq(name, m[i].key, kl) : nl == kl && ieq(name, m[i].key, kl))
            return strcmp(m[i].val, "-") ? m[i].val : "";
    }
    return "";
}

int music_room(const char *name, int n)
{
    while (n > 0 && name[n - 1] == ' ') n--;
    int changed = 0;
    if (nmusic && music_on) {
        const char *f = lookup(music_map, nmusic, name, n);
        if (strcmp(f, cur_music)) {                  /* ★同じ曲なら頭に戻さない */
            if (*f) music_start(f); else music_stop();
            changed |= MUSIC_CHANGED;
        }
    }
    if (npic) {
        const char *f = lookup(pic_map, npic, name, n);
        if (strcmp(f, cur_pic)) {
            strcpy(cur_pic, f);
            changed |= PICTURE_CHANGED;
        }
    }
    return changed;
}

const char *music_current(void) { return cur_music; }
const char *picture_current(void) { return cur_pic; }

/* ---- PMD ---- */
#ifdef PC98_HOST
void music_start(const char *file) { snprintf(cur_music, sizeof cur_music, "%s", file); }
void music_stop(void) { cur_music[0] = 0; }
#else
#include <i86.h>
#include <dos.h>

enum { PMD_START = 0x00, PMD_STOP = 0x01, PMD_FADE = 0x02, PMD_MUSDAT = 0x06, PMD_SIZE = 0x22 };
enum { FADE_SPEED = 12 };              /* 1 = いちばん遅い */

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
    snprintf(cur_music, sizeof cur_music, "%s", file);      /* ★鳴らせなくても「この曲」と覚える（記録と同じ判断にする） */
    const unsigned seg = find_pmd();
    if (!seg) return;
    FILE *f = fopen(file, "rb");
    if (!f) return;
    pmd(PMD_STOP, 0);
    playing = 0;
    const union REGS sz = pmd(PMD_SIZE, 0);            /* AL = 曲の枠（KB） */
    const union REGS at = pmd(PMD_MUSDAT, 0);          /* DX = 曲を置く場所（★DS は使わない） */
    const size_t cap = (size_t)sz.h.al * 1024 - 1;
    const size_t n = fread(lin(seg, at.w.dx), 1, cap + 1, f);
    fclose(f);
    if (!n || n > cap) return;                         /* 枠に入らない曲は鳴らさない */
    pmd(PMD_START, 0);
    playing = 1;
}

void music_stop(void)
{
    cur_music[0] = 0;
    if (!playing) return;
    playing = 0;
    pmd(PMD_FADE, FADE_SPEED);
}
#endif
