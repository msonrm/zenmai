/* Zenmai 字の実装 — 焼いたビットマップ（glyphs.h）。
 *
 * ★render.c から切り出した（2026-08-30）。中身は 1 行も変えていない ——
 *   境界を作るのが目的で、振る舞いを変えるのが目的ではない。
 *
 * ★**PS1 はこれしか使えない**（freestanding / mini-libc なので FreeType は積めない）。
 *   そして SDL 側でもこれを選んでビルドすれば、**PS1 と画素一致する**
 *   ——それが test-sdl.sh の台。字を差し替えても、組版と入力の回帰検査は
 *   この版で走らせ続けられる。
 *
 * glyphs.h は KH ドットフォントの 24px 埋め込みビットマップの抽出物なので
 * ★**MIT ではなく SIL OFL 1.1** の下にある（vendor/kh-dotfont/README.md）。
 */
#include "glyph.h"
#include "glyphs.h"

static int clip_y0, clip_y1;

void glyph_init(void) { }              /* 焼いてあるので開くものが無い */

int glyph_font_kind(void) { return 1; }   /* KH ドットフォントの埋め込みビットマップ */

/* ★rows で止めるのは**焼いた版では実質何もしない**（字は必ず 24 行に収まるので、
   呼ぶ側が y を正しく渡していれば器から出ない）。それでも書いてあるのは、
   2 つの実装が**同じ契約**を守っていることを形で示すため。 */

void glyph_clip(int y0, int y1)
{
    clip_y0 = y0;
    clip_y1 = y1;
}

static int find_glyph(const GInfo *info, int n, uint16_t code)
{
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (info[mid].code == code) return mid;
        if (info[mid].code < code) lo = mid + 1; else hi = mid - 1;
    }
    return -1;
}

int glyph_w(uint16_t code)
{
    int i = find_glyph(base_info, BASE_N, code);
    return i < 0 ? 24 : base_info[i].width;
}

void draw24(uint16_t (*buf)[W], int rows, int x, int y, uint16_t code, uint16_t color)
{
    int i = find_glyph(base_info, BASE_N, code);
    if (i < 0) return;
    for (int r = 0; r < 24; r++) {
        int ry = y + r;
        if (ry < clip_y0 || ry >= clip_y1 || ry < 0 || ry >= rows) continue;
        unsigned bits = base_rows[i][r];
        for (int c = 0; bits; c++, bits >>= 1)
            if (bits & 1) buf[ry][x + c] = color;
    }
}

void draw12(uint16_t (*buf)[W], int rows, int x, int y, uint16_t code, uint16_t color)
{
    int i = find_glyph(ruby_info, RUBY_N, code);
    if (i < 0) return;
    for (int r = 0; r < 12; r++) {
        int ry = y + r;
        if (ry < clip_y0 || ry >= clip_y1 || ry < 0 || ry >= rows) continue;
        unsigned bits = ruby_rows[i][r];
        for (int c = 0; bits; c++, bits >>= 1)
            if (bits & 1) buf[ry][x + c] = color;
    }
}

/* ---- 整形 —— 焼いた版は**永久に 1 対 1** ----
 *
 * ★PS1 に HarfBuzz は載らない（freestanding / libc 無し / 2MB RAM）ので、
 *   ここが置き換わることは無い。★**それでも境界を持たせる**のは、上の層
 *   （render.c）を「整形を通す形」に一本化しておかないと、FreeType 版だけが
 *   別の道を通ることになり、**PS1 と SDL の画素一致で検査できなくなる**から。
 */
int shape_run(const uint16_t *s, int n, Shaped *out, int max)
{
    const int m = n < max ? n : max;
    for (int i = 0; i < m; i++) {
        out[i].gid = s[i];
        out[i].cluster = (uint16_t)i;
        out[i].adv = (int16_t)glyph_w(s[i]);
        out[i].dx = 0;
        out[i].dy = 0;
    }
    return m;
}

void draw24_gid(uint16_t (*buf)[W], int rows, int x, int y, uint16_t gid, uint16_t color)
{
    draw24(buf, rows, x, y, gid, color);
}
