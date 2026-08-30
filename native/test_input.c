/* input.c のゴールデンテスト(ホストビルド専用)。
 *
 *   gcc -std=c11 -O1 -Wall test_input.c input.c -o test_input && ./test_input
 *
 * 疑似ホスト(かなバッファ + キー履歴)の意味論は labo scripts/run-gamepad-golden.mjs と同一。
 */
#include <stdio.h>
#include <string.h>
#include "input.h"

static struct {
    unsigned short buf[64];
    int len;
    const char *keys[32], *skeys[8], *ckeys[8];
    int nkeys, nskeys, nckeys;
} H;
static GpMachine M;
static int ENG, LAST, FAILS;
static const char *CASENAME;

static void case_begin(const char *name, int eng)
{
    memset(&H, 0, sizeof H);
    gp_init(&M);
    ENG = eng;
    LAST = 0;
    CASENAME = name;
}

static void apply_kana(const unsigned short *t, int n, int rep)
{
    H.len -= rep;
    if (H.len < 0)
        H.len = 0;
    memcpy(H.buf + H.len, t, (size_t)n * 2);
    H.len += n;
}

static void do_frame(GpFrame f)
{
    GpAction a[4];
    int n = gp_step(&M, ENG, &f, a);
    int ops = 0;
    for (int i = 0; i < n; i++) {
        if (a[i].type == GPA_KANA) {
            apply_kana(a[i].text, a[i].tlen, a[i].replace);
            ops++;
        } else {
            unsigned short o[2];
            int rep, l = gp_resolve_youon(H.buf, H.len, o, &rep);
            if (l) {
                apply_kana(o, l, rep);
                ops++;
            }
        }
    }
    gp_sync_prev(&M, &f);
    LAST = ops;
}

static void act_kana(const unsigned short *t, int n, int rep)
{
    gp_break_rt_cycle(&M);
    apply_kana(t, n, rep);
    LAST = 1;
}

static void act_youon(void)
{
    gp_break_rt_cycle(&M);
    unsigned short o[2];
    int rep, l = gp_resolve_youon(H.buf, H.len, o, &rep);
    if (l)
        apply_kana(o, l, rep);
    LAST = l ? 1 : 0;
}

static void act_dakuten(void)
{
    gp_break_rt_cycle(&M);
    unsigned short o[2];
    int rep, l = gp_resolve_dakuten(H.buf, H.len, o, &rep);
    if (l)
        apply_kana(o, l, rep);
    LAST = l ? 1 : 0;
}

static void act_key(const char *name, int shift, int ctrl)
{
    gp_break_rt_cycle(&M);
    H.keys[H.nkeys++] = name;
    if (shift)
        H.skeys[H.nskeys++] = name;
    if (ctrl)
        H.ckeys[H.nckeys++] = name;
    LAST = 1;
}

static void set_tail(const unsigned short *t, int n)
{
    H.len = n;
    if (n)
        memcpy(H.buf, t, (size_t)n * 2);
}

static void fail(const char *where, const char *what)
{
    FAILS++;
    printf("NG: %s / %s: %s\n", CASENAME, where, what);
}

static void check_kana(const unsigned short *t, int n, const char *where)
{
    if (H.len != n || (n && memcmp(H.buf, t, (size_t)n * 2))) {
        fail(where, "kana 不一致:");
        printf("  期待:");
        for (int i = 0; i < n; i++) printf(" %04X", t[i]);
        printf("\n  実際:");
        for (int i = 0; i < H.len; i++) printf(" %04X", H.buf[i]);
        printf("\n");
    }
}

static void check_list(const char *const *e, int n, const char **g, int gn,
                       const char *where, const char *label)
{
    int ok = n == gn;
    for (int i = 0; ok && i < n; i++)
        ok = !strcmp(e[i], g[i]);
    if (!ok)
        fail(where, label);
}

static void check_keys(const char *const *e, int n, const char *w) { check_list(e, n, H.keys, H.nkeys, w, "keys 不一致"); }
static void check_skeys(const char *const *e, int n, const char *w) { check_list(e, n, H.skeys, H.nskeys, w, "shiftKeys 不一致"); }
static void check_ckeys(const char *const *e, int n, const char *w) { check_list(e, n, H.ckeys, H.nckeys, w, "ctrlKeys 不一致"); }

static void check_ops(int e, const char *where)
{
    if (LAST != e) {
        char msg[64];
        snprintf(msg, sizeof msg, "ops 期待 %d 実際 %d", e, LAST);
        fail(where, msg);
    }
}

#include "golden_cases.h"

int main(void)
{
    run_cases();
    if (FAILS) {
        printf("失敗 %d 件\n", FAILS);
        return 1;
    }
    printf("ゴールデン全件一致\n");
    return 0;
}
