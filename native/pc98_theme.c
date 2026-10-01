/* PC-98 の画面の装い（色）と場面ごとの替え方。規則は pc98_theme.h。 */
#include "pc98_theme.h"
#include "pc98_gfx.h"
#include "pc98_text.h"
#include <stdio.h>
#include <string.h>

enum { SCENE_N = 48, KEY_N = 40, LINE_N = 200 };
typedef struct { char key[KEY_N]; unsigned char prefix; short v[TH_N]; } Scene;

static const char *const names[TH_N] = {
    "band", "top", "side", "input", "body", "ruby",
    "status", "score", "prompt", "input_fg", "text", "echo",
};
static const struct { const char *name; int attr; } colors[] = {
    { "black", 0x01 }, { "blue", 0x21 }, { "red", 0x41 }, { "magenta", 0x61 },
    { "green", 0x81 }, { "cyan", 0xA1 }, { "yellow", 0xC1 }, { "white", 0xE1 },
};
/* 既定 = 今までの色 */
#define DEFAULTS { 0x237, 0x743, 0x743, 0x255, 0x000, 0xAAA, \
                   TA_YELLOW, TA_WHITE, TA_CYAN, TA_WHITE, TA_WHITE, TA_CYAN }
static const short defaults[TH_N] = DEFAULTS;

/* ★初めから既定が入っている（INI を読まない検査・起動画面でも文字の属性が 0 にならない） */
static short base[TH_N] = DEFAULTS;    /* 全体（既定 + [theme]） */
static short cur[TH_N] = DEFAULTS;     /* 今の場面（base に場面を重ねたもの） */
static Scene scene[SCENE_N];
static int nscene;

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

static int ieq(const char *a, const char *b, int n)      /* 先頭 n 字が（大文字小文字を除いて）同じか */
{
    for (int i = 0; i < n; i++)
        if (lower((unsigned char)a[i]) != lower((unsigned char)b[i])) return 0;
    return 1;
}
static int seq(const char *a, const char *b) { const int n = (int)strlen(b); return ieq(a, b, n) && !a[n]; }

static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
    return s;
}

static int hex(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    c = lower(c);
    return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

/* 値を読む。背景など（#RGB）と文字（名前）で形が違う。読めなければ -1 */
static int parse_value(int key, const char *v)
{
    if (key < TH_FG0) {
        if (v[0] != '#' || hex(v[1]) < 0 || hex(v[2]) < 0 || hex(v[3]) < 0 || v[4]) return -1;
        return hex(v[1]) << 8 | hex(v[2]) << 4 | hex(v[3]);
    }
    for (int i = 0; i < 8; i++)
        if (seq(v, colors[i].name)) return colors[i].attr;
    return -1;
}

static int key_of(const char *k)
{
    for (int i = 0; i < TH_N; i++)
        if (seq(k, names[i])) return i;
    return -1;
}

static void set(short *arr, const char *k, const char *v, int global)
{
    const int key = key_of(k);
    if (key < 0 || (!global && key >= TH_TEXT)) return;     /* text / echo は全体だけ */
    const int x = parse_value(key, v);
    if (x >= 0) arr[key] = (short)x;
}

static void add_scene(char *room, char *rest)
{
    if (nscene >= SCENE_N) return;
    Scene *p = &scene[nscene];
    const int kl = (int)strlen(room);
    if (!kl || kl >= KEY_N) return;
    strcpy(p->key, room);
    p->prefix = p->key[kl - 1] == '*';
    if (p->prefix) p->key[kl - 1] = 0;
    for (int i = 0; i < TH_N; i++) p->v[i] = -1;
    for (char *t = strtok(rest, " \t"); t; t = strtok(0, " \t")) {
        char *eq = strchr(t, '=');
        if (!eq) continue;
        *eq = 0;
        set(p->v, t, eq + 1, 0);
    }
    nscene++;
}

/* sec: 0 = [theme] / 1 = [scene]。ファイルが無ければ何もしない */
static void read_ini(const char *path, int with_scene)
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
            sec = seq(s, "theme") ? 0 : seq(s, "scene") && with_scene ? 1 : -1;
            continue;
        }
        char *eq = strchr(s, '=');
        if (!eq || sec < 0) continue;
        *eq = 0;
        char *k = trim(s), *v = trim(eq + 1);
        if (sec == 0) set(base, k, v, 1);
        else add_scene(k, v);
    }
    fclose(f);
}

static void reset_cur(void) { memcpy(cur, base, sizeof cur); }

void theme_config(void)
{
    memcpy(base, defaults, sizeof base);
    nscene = 0;
    read_ini("ZENMAI.INI", 0);
    reset_cur();
}

void theme_work(const char *base_name)
{
    char path[32];
    nscene = 0;
    snprintf(path, sizeof path, "%s.INI", base_name);
    read_ini(path, 1);
    reset_cur();
}

int theme_room(const char *name, int n)
{
    while (n > 0 && name[n - 1] == ' ') n--;
    short next[TH_N];
    memcpy(next, base, sizeof next);
    for (int i = 0; i < nscene; i++) {
        const int kl = (int)strlen(scene[i].key);
        if (scene[i].prefix ? n >= kl && ieq(name, scene[i].key, kl) : n == kl && ieq(name, scene[i].key, kl)) {
            for (int k = 0; k < TH_N; k++)
                if (scene[i].v[k] >= 0) next[k] = scene[i].v[k];
            break;                                    /* 上から順に見て最初に合ったもの */
        }
    }
    if (!memcmp(next, cur, sizeof cur)) return 0;
    memcpy(cur, next, sizeof cur);
    return 1;
}

static void pal(int idx, int rgb) { gfx_palette(idx, rgb >> 4 & 15, rgb >> 8 & 15, rgb & 15); }   /* gfx_palette は G, R, B の順 */

void theme_apply(void)
{
    pal(PAL_BODY, cur[TH_BODY]);
    pal(PAL_BAND, cur[TH_BAND]);
    pal(PAL_SIDE, cur[TH_SIDE]);
    pal(PAL_INPUT, cur[TH_INPUT]);
    pal(PAL_TOP, cur[TH_TOP]);
    pal(PAL_RUBY, cur[TH_RUBY]);
    const int a = cur[TH_INPUT_FG];                        /* キャレット・▼ = コマンド文字色（属性 bit7 = G・bit6 = R・bit5 = B） */
    gfx_palette(PAL_CARET, a >> 7 & 1 ? 15 : 0, a >> 6 & 1 ? 15 : 0, a >> 5 & 1 ? 15 : 0);
}

int theme_attr(int key) { return cur[key]; }
int theme_rgb(int key) { return cur[key]; }

void theme_describe(char *buf, int n)
{
    int w = 0;
    for (int i = 0; i < TH_N && w < n - 16; i++)
        w += snprintf(buf + w, (size_t)(n - w), i < TH_FG0 ? "%s=#%03X " : "%s=%02X ", names[i], cur[i]);
    if (w > 0) buf[w - 1] = 0;
}
