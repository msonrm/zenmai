/* PC-98 版の本文 —— render.h の**文字列の口**をテキスト画面で実装する（render.c の対）。
 *
 * ★session.c が流す先（draw_plain / draw_echo / hist_blank）と、ふりがなを分ける jp_text.c が
 *   呼ぶ口（push_text / push_ruby / flush_vline）を同じ名前で持つので、どちらも無改造で載る。
 *   ★画素の側（canvas・strip・view_px・build_strip）は PC-98 には無い。
 *
 * ★組み方（計画 = docs/pc98-port-plan.md の段 4）:
 *   - 1 行 = 全角 32 字（64 桁）。字はテキスト画面、ふりがなはグラフィック画面（美咲ゴシック・灰）
 *   - 禁則は kinsoku.h（PS1 / SDL と同じ表）。行頭禁則の 1 字は**右の余白へぶら下げる**（窓の右の
 *     全角 1 字ぶん = 72〜73 桁目）。行末禁則の開き括弧は次の行へ道連れ。英字の語は割らない
 *   - ★ふりがなは**親字を動かさない**（テキストは 8px の格子）。読みは親字の中央に置き、前の読みと
 *     ぶつかるなら右へずらす（試作 pc98-mock/gen_screen.py と同じ規則）。PS1 は箱を広げて親字を
 *     寄せるが、それは 4px 単位になるので格子に乗らない
 *   - 行の高さは一律（どの行にも 8 ラスタのふりがなの帯がある）
 *
 * ★本文は「組んだ行」（VLine）の環で持ち、窓（12 行）はそこから描き直す。遡りも同じ描き方。
 * 記録: render_log を開いておくと、積んだ論理行を UTF-8 で書く（組み方に依らない＝台本の突き合わせ用）。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "render.h"
#include "kinsoku.h"
#include "pc98_text.h"
#include "pc98_gfx.h"
#include "pc98_theme.h"
#include "misaki_data.h"
#include "render_pc98.h"

FILE *render_log;
void (*line_render)(const uint16_t *s, int n, uint16_t color);

/* ---- 組んだ行 ---- */
enum { VL_CHARS = BODY_CELLS + HANG_CELLS, VL_RUBY = 64, HIST_N = 400 };
typedef struct {
    uint16_t ch[VL_CHARS];             /* 左から（全角も 1 つ） */
    uint8_t n, attr, nr;
    uint16_t rx[VL_RUBY];              /* ふりがな 1 字ごとの x（画素） */
    uint16_t rc[VL_RUBY];
} VLine;

/* ★環は起動後に確保する（body_init）。157KB あるので、焼き込むと本体の像が大きくなる（pack.h と同じ理由・2026-09-29）。
 * ★しかも 50 行（約 20KB）ずつの塊に分けて取る —— DOS/4GW は拡張メモリが尽きると、残りを通常メモリから
 *   1 本あたり 59KB 以下の細切れでしか返さない。1 本で 157KB を求めると拡張 1MB の機械では取れない
 *   （docs/pc98-port-plan.md の「段 8」） */
enum { HIST_CHUNK = 50 };
static VLine *hist_blk[HIST_N / HIST_CHUNK];
#define HIST_AT(i) (&hist_blk[(i) % HIST_N / HIST_CHUNK][(i) % HIST_CHUNK])
static long total;                     /* これまでに積んだ組んだ行の数（捨てた分を含む） */
static long view;                      /* 窓の上端の行 */

static long hist_min(void) { return total > HIST_N ? total - HIST_N : 0; }

int body_init(void)
{
    for (int b = 0; b < HIST_N / HIST_CHUNK; b++)
        if (!hist_blk[b] && !(hist_blk[b] = malloc(sizeof(VLine) * HIST_CHUNK)))
            return 0;
    return 1;
}

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

static uint8_t attr_of(uint16_t color)
{
    return (uint8_t)theme_attr(color == ACCENT ? TH_ECHO : TH_TEXT);
}

/* ---- 割り付け（render.c と同じ形。幅の単位は桁 = 8px）---- */
typedef struct {
    uint16_t ch;                       /* 字（ふりがなの付いたかたまりでは 0） */
    uint8_t blen, rlen, w;
    const uint16_t *base, *ruby;
} Frag;
static Frag frags[VL_CHARS + 8];
static int nfrag, fw;

static void flush_line(uint16_t color)
{
    VLine *v = HIST_AT(total);
    v->n = 0;
    v->nr = 0;
    v->attr = attr_of(color);
    int cell = 0, last_end = RUBY_X_MIN;
    for (int i = 0; i < nfrag; i++) {
        const Frag *f = &frags[i];
        if (!f->rlen) {
            if (v->n < VL_CHARS) v->ch[v->n++] = f->ch;
            cell += f->w;
            continue;
        }
        for (int k = 0; k < f->blen && v->n < VL_CHARS; k++)
            v->ch[v->n++] = f->base[k];
        const int bx = (BODY_COL0 + cell) * 8, bw = f->w * 8, rw = RUBY_W * f->rlen;
        int rx = bx + (bw - rw) / 2;   /* 親字の中央 */
        if (rx < last_end) rx = last_end;              /* 前の読みとぶつかるなら右へ */
        if (rx + rw > RUBY_X_MAX && RUBY_X_MAX - rw >= last_end)
            rx = RUBY_X_MAX - rw;                      /* 右の装飾に掛かるなら、ぶつからない範囲で左へ */
        for (int k = 0; k < f->rlen && v->nr < VL_RUBY; k++) {
            v->rx[v->nr] = (uint16_t)(rx + RUBY_W * k);
            v->rc[v->nr++] = f->ruby[k];
        }
        last_end = rx + rw;
        cell += f->w;
    }
    total++;
    nfrag = 0;
    fw = 0;
}

void flush_vline(uint16_t color)
{
    if (nfrag) flush_line(color);
}

/* 折り返しのために行を送る。★行末禁則の字が末尾に残るなら道連れにする */
static void line_break(uint16_t color)
{
    Frag carry;
    int has = 0;
    if (nfrag > 1 && !frags[nfrag - 1].rlen && kinsoku_tail(frags[nfrag - 1].ch)) {
        carry = frags[--nfrag];
        fw -= carry.w;
        has = 1;
    }
    flush_line(color);
    if (has) {
        frags[nfrag++] = carry;
        fw += carry.w;
    }
}

static void room_for_one(uint16_t color)
{
    if (nfrag >= (int)(sizeof frags / sizeof *frags))
        flush_line(color);
}

void push_char(uint16_t ch, uint16_t color)
{
    Frag f;                            /* ★Watcom は自動変数の集成体を非定数で初期化できない */
    f.ch = ch; f.blen = 1; f.rlen = 0; f.w = (uint8_t)txt_cells(ch); f.base = f.ruby = 0;
    room_for_one(color);
    if (fw + f.w > BODY_CELLS && fw > 0) {
        /* 行頭禁則: 1 字だけ右の余白へぶら下げる */
        if (kinsoku_head(ch) && fw + f.w <= BODY_CELLS + HANG_CELLS) {
            frags[nfrag++] = f;
            fw += f.w;
            return;
        }
        line_break(color);
        if (ch == ' ') return;         /* 折り返し直後の空白は捨てる */
        room_for_one(color);
    }
    frags[nfrag++] = f;
    fw += f.w;
}

void push_text(const uint16_t *s, int n, uint16_t color)
{
    int i = 0;
    while (i < n) {
        if (!kinsoku_word(s[i])) {     /* ★空白もここ */
            push_char(s[i++], color);
            continue;
        }
        int j = i, w = 0;
        while (j < n && kinsoku_word(s[j]))
            w += txt_cells(s[j++]);
        if (w <= BODY_CELLS && fw + w > BODY_CELLS && fw > 0)
            line_break(color);
        for (; i < j; i++)
            push_char(s[i], color);
    }
}

/* ★箱の幅は**親字の幅だけ**（読みが長くても広げない ＝ 親字を格子から動かさない） */
void push_ruby(const uint16_t *base, int blen, const uint16_t *ruby, int rlen, uint16_t color)
{
    int w = 0;
    for (int k = 0; k < blen; k++)
        w += txt_cells(base[k]);
    room_for_one(color);
    if (fw + w > BODY_CELLS && fw > 0) {
        line_break(color);
        room_for_one(color);
    }
    Frag *f = &frags[nfrag++];
    f->ch = 0; f->blen = (uint8_t)blen; f->rlen = (uint8_t)rlen; f->w = (uint8_t)w;
    f->base = base; f->ruby = ruby;
    fw += w;
}

static void plain_render(const uint16_t *s, int n, uint16_t color)
{
    push_text(s, n, color);
    flush_vline(color);
}

/* ---- 積む（render.h の口）---- */

void hist_line(const uint16_t *s, int n, uint16_t color)
{
    log_line(s, n);
    if (!line_render) line_render = plain_render;
    if (n == 0) {                      /* 空の論理行も 1 行ぶん空ける */
        flush_line(color);
        return;
    }
    line_render(s, n, color);
}

void hist_blank(void)
{
    log_line(0, 0);
    nfrag = 0;
    fw = 0;
    flush_line(INK);
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

/* ---- 窓 ---- */

static const uint8_t *misaki(uint16_t u)
{
    int lo = 0, hi = MISAKI_N - 1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        if (misaki_code[mid] == u) return misaki_glyph[mid];
        if (misaki_code[mid] < u) lo = mid + 1; else hi = mid - 1;
    }
    return 0;
}

static void draw_row(int r, const VLine *v)
{
    const int row = BODY_ROW0 + r, y = row * TXT_RASTERS;
    txt_clear(row, row, TA_WHITE);
    gfx_rect(DECO_W, y, RUBY_X_MAX, y + RUBY_BAND, 0);   /* ふりがなの帯を消す（左右の装飾の内側） */
    if (!v) return;
    int col = BODY_COL0;
    for (int i = 0; i < v->n; i++)
        col += txt_put(row, col, v->ch[i], v->attr);
    for (int i = 0; i < v->nr; i++) {
        const uint8_t *g = misaki(v->rc[i]);
        if (g) gfx_glyph8(v->rx[i] + RUBY_DX, y + RUBY_DY, g, RUBY_COLOR);
    }
}

static void draw_window(void)
{
    for (int r = 0; r < BODY_ROWS; r++) {
        const long i = view + r;
        draw_row(r, i >= hist_min() && i < total ? HIST_AT(i) : 0);
    }
    /* 下に続きがある印（▼）。本文の最下行の下の空きに、グラフィックで置く */
    gfx_rect(MARK_X, MARK_Y, MARK_X + 16, MARK_Y + 16, 0);
    if (view + BODY_ROWS < total) {
        const int cx = MARK_X + 8, y0 = MARK_Y + (16 - MARK_H) / 2;
        for (int i = 0; i < MARK_H; i++) {
            const int w = MARK_W - 2 * (i / 2);                          /* 2 行ごとに 1 画素ずつ両側が細る */
            gfx_fill(cx - w / 2, y0 + i, cx - w / 2 + w, y0 + i + 1, MARK_COLOR);
        }
    }
}

static long bottom(void)
{
    long t = total - BODY_ROWS;
    if (t < hist_min()) t = hist_min();
    return t;
}

void body_show(int animate)
{
    const long t = bottom();
    if (animate && view < t && t - view <= BODY_ROWS * 4) {
        while (view < t) {             /* 1 行ずつ送って見せる */
            view++;
            txt_vsync();
            txt_vsync();
            draw_window();
        }
        return;
    }
    view = t;
    draw_window();
}

int body_scroll(int d)
{
    long v = view + d;
    if (v > bottom()) v = bottom();
    if (v < hist_min()) v = hist_min();
    if (v == view) return 0;
    view = v;
    draw_window();
    return 1;
}

int body_at_bottom(void)
{
    return view >= bottom();
}
