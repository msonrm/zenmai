/* 割り付け（render.c）の検査 —— 禁則処理が効いているか。
 *
 * ★見るのは「何行になったか」ではなく **どの字がどの行の頭と末に来たか**。
 *   行数だけだと、★**折り返しが全部壊れていても数が合う**ことがある。
 *
 * ★字の実装（glyph.h の 5 本）を**この検査が自分で持つ**。等幅（半角 12 / 全角 24）で
 *   答え、描く代わりに (行, x, 字) を控える —— これで割り付けが**そのまま読める**。
 *   焼いた版の送り幅と同じ数字なので、PS1 版の実挙動をそのまま見ていることになる。
 *
 *   cc -O1 -Wall -I. test_wrap.c render.c content_data.c -o t && ./t
 */
#include <stdio.h>
#include <string.h>
#include "render.h"

/* ---- 機械の境界（plat.h）: 何もしない ---- */
void gpu_init(void) {}
void gpu_display_on(void) {}
void gp0_fill(int x, int y, int w, int h, uint32_t rgb) { (void)x; (void)y; (void)w; (void)h; (void)rgb; }
void gp0_upload(int x, int y, int w, int h, const uint16_t *s) { (void)x; (void)y; (void)w; (void)h; (void)s; }
void gp0_copy(int a, int b, int c, int d, int e, int f) { (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; }
void wait_fields(int n) { (void)n; }
void pad_try_analog(void) {}
int pad_read(void) { return 0; }
int pad_read_ex(uint8_t axes[4]) { (void)axes; return 0; }

/* ---- 字の境界（glyph.h）: 等幅で答え、描く代わりに控える ---- */
enum { REC_N = 512 };
static struct { int y, x; uint16_t code; } rec[REC_N];
static int nrec;

void glyph_init(void) { nrec = 0; }
int  glyph_font_kind(void) { return 1; }
void glyph_clip(int y0, int y1) { (void)y0; (void)y1; }
/* ★全角の送りを**差し替えられる**ようにしてある。既定の 24 は焼いたビットマップ版
   （PS1）で、22 にすると FreeType 版（PortMaster）と同じ寸法になる ——
   ★**24 のままでは「24 と直に書いた場所」が炙り出せない**（24 == 24 で一致してしまう）。 */
static int cjk_w = 24;
int  glyph_w(uint16_t c) { return c < 0x80 ? 12 : cjk_w; }

void draw24(uint16_t (*buf)[W], int rows, int x, int y, uint16_t code, uint16_t color)
{
    (void)buf; (void)rows; (void)color;
    if (nrec < REC_N)
        rec[nrec++] = (typeof(rec[0])){ y, x, code };
}
void draw12(uint16_t (*buf)[W], int rows, int x, int y, uint16_t code, uint16_t color)
{
    (void)buf; (void)rows; (void)x; (void)y; (void)code; (void)color;
}

/* ★整形（1 対 1）—— 焼いた版と**同じ約束**で、code をそのままグリフ番号にする。
   検査が字の実装を自前で持っている以上、境界が増えればここも増える。 */
int shape_run(const uint16_t *s, int n, Shaped *out, int max)
{
    const int m = n < max ? n : max;
    for (int i = 0; i < m; i++) {
        out[i].gid = s[i];
        out[i].cluster = (uint16_t)i;
        out[i].adv = (int16_t)glyph_w(s[i]);
        out[i].dx = 0;
        out[i].dy = 0;
        out[i].face = 0;
    }
    return m;
}

/* ★★**こちらも控えること。** 本文の描画が draw24 から draw24_gid へ移ったので、
   ここを空実装にすると記録が丸ごと消え、**検査は何も見ないまま緑になる**
   （lines_of が空を返し、期待した行と比べる前に一致してしまう形）。 */
void draw24_gid(uint16_t (*buf)[W], int rows, int x, int y,
                uint16_t gid, int face, uint16_t color)
{
    (void)face;                        /* ★面は 1 つだけ */
    draw24(buf, rows, x, y, gid, color);
}

/* ---- 台 ---- */
static int ng;

/* 控えた記録を「行ごとの文字列」に畳む。行の切れ目 = y が変わったところ */
static int lines_of(char out[8][256])
{
    int n = 0, prev = -1;
    char *w = 0;
    for (int i = 0; i < nrec; i++) {
        if (rec[i].y != prev) {
            if (n >= 8) break;
            prev = rec[i].y;
            w = out[n++];
            *w = 0;
        }
        /* 見えるように: ASCII はそのまま、全角は 1 バイトの札に潰す */
        char c = rec[i].code < 0x80 ? (char)rec[i].code
               : rec[i].code == 0x3002 ? '.'      /* 。 */
               : rec[i].code == 0x3001 ? ','      /* 、 */
               : rec[i].code == 0x300C ? '['      /* 「 */
               : rec[i].code == 0x300D ? ']'      /* 」 */
               : rec[i].code == 0x30FC ? '-'      /* ー */
               : rec[i].code == 0x3063 ? 't'      /* っ */
               : rec[i].code == 0x2026 ? ':'      /* … ★記号（語を作らない） */
               : (rec[i].code >= 0x0900 && rec[i].code <= 0x097F) ? 'D'   /* デーヴァナーガリー */
               : (rec[i].code >= 0xAC00 && rec[i].code <= 0xD7A3) ? 'K'   /* ハングル */
               : '#';
        int l = (int)strlen(w);
        if (l < 250) { w[l] = c; w[l + 1] = 0; }
    }
    return n;
}

/* s（UTF-16 の並び）を 1 論理行として組み、行ごとの結果を expect と突き合わせる */
static void check(const char *name, const uint16_t *s, int n, const char *const *expect, int en)
{
    render_init();
    line_render(s, n, INK);
    char got[8][256];
    int gn = lines_of(got);
    int ok = gn == en;
    for (int i = 0; ok && i < gn; i++)
        ok = strcmp(got[i], expect[i]) == 0;
    if (ok) {
        printf("✓ %s\n", name);
        return;
    }
    printf("✗ %s\n", name);
    for (int i = 0; i < gn; i++)  printf("    実際 %d: %s\n", i + 1, got[i]);
    for (int i = 0; i < en; i++)  printf("    期待 %d: %s\n", i + 1, expect[i]);
    ng++;
}

/* 全角の「あ」を n 個 + 続き。★1 行は全角ちょうど 24 字（TEXT_W 576 ÷ 24） */
static uint16_t buf[128];
static int fill(int n, const uint16_t *tail, int tn)
{
    for (int i = 0; i < n; i++)
        buf[i] = 0x3042;               /* あ */
    for (int i = 0; i < tn; i++)
        buf[n + i] = tail[i];
    return n + tn;
}

#define A24 "########################"
#define A23 "#######################"

int main(void)
{
    /* ★まず素の折り返しが 24 字ちょうどで折れること（ここが狂うと以下が全部無意味） */
    {
        static const char *const e[] = { A24, A24 };
        check("素の折り返し: 全角 24 字で折る", buf, fill(48, 0, 0), e, 2);
    }
    /* 行頭禁則: 25 字目の 。 は次の行へ落とさず、右の余白へぶら下げる */
    {
        static const uint16_t t[] = { 0x3002 };
        static const char *const e[] = { A24 "." };
        check("行頭禁則: 。 をぶら下げる", buf, fill(24, t, 1), e, 1);
    }
    /* 小書き・長音も行頭に来ない */
    {
        static const uint16_t t[] = { 0x30FC };
        static const char *const e[] = { A24 "-" };
        check("行頭禁則: ー をぶら下げる", buf, fill(24, t, 1), e, 1);
    }
    {
        static const uint16_t t[] = { 0x3063 };
        static const char *const e[] = { A24 "t" };
        check("行頭禁則: っ をぶら下げる", buf, fill(24, t, 1), e, 1);
    }
    /* ★ぶら下げるのは 1 字だけ。2 字目は普通に折る（余白に入らない） */
    {
        static const uint16_t t[] = { 0x300D, 0x3002 };
        static const char *const e[] = { A24 "]", "." };
        check("★ぶら下げは 1 字まで", buf, fill(24, t, 2), e, 2);
    }
    /* 行末禁則: 24 字目の 「 は次の行へ道連れ */
    {
        static const uint16_t t[] = { 0x300C };
        int n = fill(23, t, 1);
        for (int i = 0; i < 24; i++) buf[n + i] = 0x3042;
        static const char *const e[] = { A23, "[" A23, "#" };
        check("行末禁則: 「 を次の行へ送る", buf, n + 24, e, 3);
    }
    /* ★道連れの相手がいないとき（「 が行頭）に無限に送らないこと */
    {
        static const uint16_t t[] = { 0x300C };
        int n = fill(24, t, 1);
        for (int i = 0; i < 24; i++) buf[n + i] = 0x3042;
        static const char *const e[] = { A24, "[" A23, "#" };
        check("★行頭の 「 は道連れにしない（送り続けない）", buf, n + 24, e, 3);
    }
    /* ★カナリア: 禁則の字が無ければ、これまで通り 24 字で折るだけ */
    {
        static const char *const e[] = { A24, A24, "#" };
        check("★カナリア: 禁則の字が無ければ何も変わらない", buf, fill(49, 0, 0), e, 3);
    }
    /* ★カナリア: 英文（語で折る）に手を出していないこと */
    {
        static const uint16_t s[] = { 'a','a',' ','b','b' };
        static const char *const e[] = { "aa bb" };
        check("★カナリア: 英文は語で折るまま", s, 5, e, 1);
    }

    /* ---- ★★空白で語を切る言語（ハングル / デーヴァナーガリー）----
     *
     * ★もとは「ASCII でなければどこでも折ってよい」だったので、**単語が行の途中で
     *   割れていた**（Higgins の実機で発覚・2026-09-03）。日本語は行中どこでも
     *   折れるので、Zork だけを見ているあいだは「ASCII でない」で足りていた。
     * ★1 行は全角ちょうど 24 字（TEXT_W 576 ÷ 24）。半角の空白は 12。 */
    {
        /* 22 字 ＋ 空白（= 540px）で、残り 36px。4 字の語（96px）は入らないので
           **語ごと次の行へ**。★直す前は 1 字だけ入って割れていた。 */
        uint16_t d[32];
        int n = 0;
        for (int i = 0; i < 22; i++) d[n++] = 0x092A;   /* प */
        d[n++] = ' ';
        for (int i = 0; i < 4; i++)  d[n++] = 0x0928;   /* न */
        static const char *const e[] = { "DDDDDDDDDDDDDDDDDDDDDD ", "DDDD" };
        check("★デーヴァナーガリーの語は割れない", d, n, e, 2);
    }
    {
        uint16_t k[32];
        int n = 0;
        for (int i = 0; i < 22; i++) k[n++] = 0xAC00;   /* 가 */
        k[n++] = ' ';
        for (int i = 0; i < 4; i++)  k[n++] = 0xD55C;   /* 한 */
        static const char *const e[] = { "KKKKKKKKKKKKKKKKKKKKKK ", "KKKK" };
        check("★ハングルの語も割れない", k, n, e, 2);
    }
    {
        /* ★カナリア: **日本語はどこでも折れるまま**。ここが「語」になってしまうと
           Zork の割り付けが丸ごと変わる（そして画素照合が赤くなる）。 */
        static const char *const e[] = { A24, "##" };
        check("★カナリア: 日本語は今までどおり行中で折る", buf, fill(26, 0, 0), e, 2);
    }
    {
        /* ★★カナリア: **記号は語を作らない**。`……` `──` が 1 かたまりになると
             Zork の画素が変わるので、ここは「日本語の範囲を列挙する」実装を弾く門。
             23 字（552px）＋ … は 576 でちょうど入り、2 つ目で折れる。 */
        uint16_t t[32];
        int n = fill(23, 0, 0);
        for (int i = 0; i < n; i++) t[i] = buf[i];
        t[n++] = 0x2026;
        t[n++] = 0x2026;
        static const char *const e[] = { A23 ":", ":" };
        check("★★カナリア: 記号（…）は語にならない", t, n, e, 2);
    }

    /* ★送り幅が 24 でないフォントでも、**ルビの付いた親字が glyph_w の送りで並ぶ**か。
     *
     * ★`render.c` はここを長らく `24 * blen` と直に書いていた。焼いた版では 24 が
     *   正しいので**画素一致テストでは原理的に捕まらず**、FreeType 版の実機でだけ
     *   ルビ付きの語が 1 字あたり 2px ずつ間延びしていた（2026-08-30 に実測）。
     *   だから見張りも「24 ではない寸法」でやらないと意味がない。 */
    {
        cjk_w = 22;                        /* = FreeType 版の本文 22px */
        render_init();
        static const uint16_t base[4] = { 0x6F22, 0x6F22, 0x6F22, 0x6F22 };
        static const uint16_t ruby[2] = { 0x304B, 0x3093 };
        push_ruby(base, 4, ruby, 2, INK);  /* ルビは親字より狭い = 親字が幅を決める */
        flush_vline(INK);
        int ok = nrec == 4;
        for (int i = 1; ok && i < nrec; i++)
            ok = rec[i].x - rec[i - 1].x == 22;
        if (ok) {
            printf("✓ ★ルビ付きの親字も送り幅どおり（22px）に並ぶ\n");
        } else {
            printf("✗ ★ルビ付きの親字が送り幅どおりでない（24 を直に書いていないか）\n");
            printf("    実際の頭: ");
            for (int i = 0; i < nrec; i++) printf("%d ", rec[i].x);
            printf("\n    期待: 22px 間隔の 4 つ\n");
            ng++;
        }
        /* ★カナリア: この見張りが「間隔」を本当に見ているか。24 に戻せば赤になること */
        cjk_w = 24;
        render_init();
        push_ruby(base, 4, ruby, 2, INK);
        flush_vline(INK);
        int step_ok = nrec == 4 && rec[1].x - rec[0].x == 22;
        if (step_ok) {
            printf("✗ ★カナリア: 24px のフォントでも 22px 間隔に見える = 物差しが死んでいる\n");
            ng++;
        } else {
            printf("✓ ★カナリア: フォントを変えれば間隔もちゃんと変わる\n");
        }
    }

    if (ng == 0) {
        printf("\n--- 15 件すべて通った ---\n");
        return 0;
    }
    printf("\n--- ★%d 件 食い違った ---\n", ng);
    return 1;
}
