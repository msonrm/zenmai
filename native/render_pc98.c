/* PC-98 版の本文 —— render.h の**文字列の口**をテキスト画面で実装する（render.c の対）。
 *
 * ★session.c が流す先は draw_plain / draw_echo / hist_blank だけなので、
 *   これを同じ名前で持てば芯はそのまま載る（plat.h・glyph.h と同じ「実装を差し替える」形）。
 *   ★画素の側（canvas・strip・view_*・build_strip）は PC-98 には無い。
 *
 * ★段 2 の本文はまだ仮: 標準の 80×25 の 2〜22 行目に流すだけで、禁則もふりがなも無い
 *   （ASCII の語だけは割らない）。24 ラスタ × 16 行・ふりがな・禁則は段 4 で入れる ——
 *   そのときは jp_text.c を載せ、push_text / push_ruby / flush_vline をここに持つ。
 *
 * 記録: render_log を開いておくと、積んだ論理行を UTF-8 で書く（台本の突き合わせ用）。
 */
#include <stdio.h>
#include "render.h"
#include "pc98_text.h"
#include "render_pc98.h"

FILE *render_log;

static int brow = PC98_BODY_TOP;       /* 次に書く行 */

static void log_line(const uint16_t *s, int n)
{
    if (!render_log) return;
    for (int i = 0; i < n; i++) {
        const unsigned u = s[i];
        if (u < 0x80) {
            fputc((int)u, render_log);
        } else if (u < 0x800) {
            fputc((int)(0xC0 | u >> 6), render_log);
            fputc((int)(0x80 | (u & 0x3F)), render_log);
        } else {
            fputc((int)(0xE0 | u >> 12), render_log);
            fputc((int)(0x80 | (u >> 6 & 0x3F)), render_log);
            fputc((int)(0x80 | (u & 0x3F)), render_log);
        }
    }
    fputc('\n', render_log);
}

static void body_newline(void)
{
    if (brow < PC98_BODY_BOT)
        brow++;
    else
        txt_scroll(PC98_BODY_TOP, PC98_BODY_BOT, TA_WHITE);
}

static uint8_t attr_of(uint16_t color)
{
    return color == ACCENT ? TA_YELLOW : TA_WHITE;
}

/* 1 論理行を折り返して流す。★段 2 は禁則なし。ASCII の語だけは割らない */
void hist_line(const uint16_t *s, int n, uint16_t color)
{
    enum { L = PC98_BODY_L, R = PC98_BODY_R };
    const uint8_t attr = attr_of(color);
    log_line(s, n);
    int col = L;
    for (int i = 0; i < n; ) {
        const uint16_t c = s[i];
        if (c > ' ' && c < 0x80) {
            int j = i;
            while (j < n && s[j] > ' ' && s[j] < 0x80) j++;
            if (col + (j - i) > R && col > L && j - i <= R - L) {
                body_newline();
                col = L;
            }
        }
        const int w = txt_cells(c);
        if (col + w > R) {
            body_newline();
            col = L;
            if (c == ' ') { i++; continue; }
        }
        col += txt_put(brow, col, c, attr);
        i++;
    }
    body_newline();
}

void hist_blank(void)
{
    log_line(0, 0);
    body_newline();
}

void draw_plain(const uint16_t *s, int n, uint16_t color)
{
    hist_line(s, n, color);
}

void draw_echo(const uint16_t *s, int n)
{
    uint16_t buf[104];
    int m = 0;
    buf[m++] = 0xFF1E;                 /* ＞ */
    for (int i = 0; i < n && m < 104; i++)
        buf[m++] = s[i];
    hist_blank();
    hist_line(buf, m, ACCENT);
}
