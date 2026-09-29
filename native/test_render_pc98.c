/* PC-98 版の本文の組み方（render_pc98.c）のホスト検査 —— 禁則・ぶら下げ・英字の語・ふりがなの位置。
 *   test-pc98.sh が建てて流す（ホストの配列に向けて建てる = PC98_HOST）。
 * ★render_pc98.c を丸ごと取り込んで、組んだ行（hist）を直に見る。
 */
#include "render_pc98.c"

static int fails;

static void u8to16(const char *s, uint16_t *out, int *n)
{
    const unsigned char *p = (const unsigned char *)s;
    *n = 0;
    while (*p) {
        unsigned u = *p++;
        if (u >= 0xE0) { u = (u & 0x0F) << 12 | (p[0] & 0x3F) << 6 | (p[1] & 0x3F); p += 2; }
        else if (u >= 0xC0) { u = (u & 0x1F) << 6 | (p[0] & 0x3F); p += 1; }
        out[(*n)++] = (uint16_t)u;
    }
}

static void line_utf8(const VLine *v, char *out)
{
    int o = 0;
    for (int i = 0; i < v->n; i++) {
        const unsigned u = v->ch[i];
        if (u < 0x80) out[o++] = (char)u;
        else if (u < 0x800) { out[o++] = (char)(0xC0 | u >> 6); out[o++] = (char)(0x80 | (u & 0x3F)); }
        else { out[o++] = (char)(0xE0 | u >> 12); out[o++] = (char)(0x80 | (u >> 6 & 0x3F)); out[o++] = (char)(0x80 | (u & 0x3F)); }
    }
    out[o] = 0;
}

/* 1 論理行を組み、組んだ行の並びを "|" でつないで比べる */
static void check(const char *title, const char *in, const char *want)
{
    uint16_t s[512];
    int n;
    u8to16(in, s, &n);
    const long t0 = total;
    hist_line(s, n, INK);
    char got[2048] = "", one[512];
    for (long i = t0; i < total; i++) {
        line_utf8(HIST_AT(i), one);
        if (i > t0) strcat(got, "|");
        strcat(got, one);
    }
    if (strcmp(got, want)) {
        printf("NG %s\n  実際: %s\n  期待: %s\n", title, got, want);
        fails++;
    }
}

int main(void)
{
    if (!body_init()) return 1;
    jp_text_init();
    /* 全角 32 字 = ちょうど 1 行 */
    const char *k32 = "あいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほまみ";
    char in[512], want[1024];

    snprintf(in, sizeof in, "%s。", k32);
    snprintf(want, sizeof want, "%s。", k32);
    check("行頭禁則の句点は右の余白へぶら下げる", in, want);

    snprintf(in, sizeof in, "%s、む", k32);
    snprintf(want, sizeof want, "%s、|む", k32);
    check("読点もぶら下げ、続きは次の行", in, want);

    snprintf(in, sizeof in, "%s。」", k32);
    snprintf(want, sizeof want, "%s。|」", k32);
    check("ぶら下げは 1 字だけ（。」 の 」 は次の行）", in, want);

    /* 31 字 + 「 で 「 がちょうど行末に来る */
    snprintf(in, sizeof in, "%s", "あいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほま「みむ」");
    snprintf(want, sizeof want, "%s", "あいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほま|「みむ」");
    check("行末禁則の開き括弧は次の行へ道連れ", in, want);

    check("英字の語は割らない",
          "ZORK is a registered trademark of Infocom, Inc. All rights reserved.",
          "ZORK is a registered trademark of Infocom, Inc. All rights |reserved.");
    /* ★折った行の末尾の空白は残る（見えない。render.c と同じ） */

    /* ふりがな: 親字は格子のまま。読みは親字の中央（瓶 = 全角 1 字・びん = 2 字でちょうど） */
    check("ふりがなの付く語", "瓶が食卓に置かれている。", "瓶が食卓に置かれている。");
    {
        const VLine *v = HIST_AT(total - 1);
        /* 瓶(びん) は 64〜79 の中央 = 64。食卓(しょくたく) は 96〜127 に 40px → 中央から 92 */
        const int want_rx[] = { 64, 72, 92, 100, 108, 116, 124 };
        int ok = v->nr == 7;
        for (int i = 0; ok && i < 7; i++) ok = v->rx[i] == want_rx[i];
        if (!ok) {
            printf("NG ふりがなの位置:");
            for (int i = 0; i < v->nr; i++) printf(" %d", v->rx[i]);
            printf("\n");
            fails++;
        }
    }
    /* 読みが隣とぶつかるなら右へずらす（台所の窓 = 台所(だいどころ)・窓(まど)） */
    check("隣の読みとぶつからない", "台所の窓", "台所の窓");
    {
        const VLine *v = HIST_AT(total - 1);
        int ok = 1;
        for (int i = 1; i < v->nr; i++) ok &= v->rx[i] >= v->rx[i - 1] + RUBY_W;
        if (!ok || !v->nr) { printf("NG 読みが重なる\n"); fails++; }
    }

    if (fails) {
        printf("本文の組み方: %d 件ちがう\n", fails);
        return 1;
    }
    printf("本文の組み方: 全件一致\n");
    return 0;
}
