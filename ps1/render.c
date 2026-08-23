/* Zenmai PS1 描画層 + パッド(実装)。
 *
 * 2026-08-20 全面改修: 履歴を「描画済みピクセルの巻物」から「テキスト行の環」へ。
 *   - 本文はテキスト(hpool/hline)で保持し、表示のたびに窓(canvas バンド)へ描き直す
 *   - 同じメモリでゲーム一周分の履歴を遡れる。長文(案内書 1,244 字)も丸ごと残る
 *   - 行の高さは追加時に採寸(render_dry)して確定。規則は従来と同一(gen_mock が正典)
 * 行の描画器は差し替え可能(line_render)。既定 = ASCII 語折返しの素描画、
 * 日本語ビルドは jp_text.c が実行時ルビ付き描画を登録する。
 */
#include "render.h"
#include "glyphs.h"

uint16_t canvas[WIN_H][W];             /* 表示窓の描画バンド */
uint16_t strip[CMD_H][W];
int pad_prev, pad_seen;
int body_top, body_h;
int view_px;
int first_line;
int cursor;                            /* 描画バンド内の行位置(採寸時は仮想) */

/* ---- GPU ---- */

void gpu_init(void)
{
    GP1 = 0x00000000;
    GP1 = 0x08000027;                  /* 640×480i NTSC 15bpp */
    GP1 = 0x06C60260;
    GP1 = 0x07042010;
    GP1 = 0x05000000;
}

/* GPU がコマンド語を受け付けられるまで待つ(GPUSTAT bit26)。
 * 待たずに連発すると 16 語の FIFO が溢れ、実機ではコマンドが落ちる
 * (VRAM 面内コピーの連発でスクロールに断片が残った実測あり) */
static void gpu_sync(void)
{
    while (!(GPUSTAT & (1u << 26))) { }
}

void gp0_fill(int x, int y, int w, int h, uint32_t rgb24)
{
    gpu_sync();
    GP0 = 0x02000000 | rgb24;
    GP0 = (uint32_t)(y << 16) | (uint32_t)x;
    GP0 = (uint32_t)(h << 16) | (uint32_t)w;
}

void gp0_upload(int x, int y, int w, int h, const uint16_t *src)
{
    gpu_sync();
    GP0 = 0xA0000000;
    GP0 = (uint32_t)(y << 16) | (uint32_t)x;
    GP0 = (uint32_t)(h << 16) | (uint32_t)w;
    const uint32_t *p = (const uint32_t *)src;
    for (int i = 0; i < w * h / 2; i++)
        GP0 = p[i];
}

void wait_fields(int n)
{
    uint32_t last = GPUSTAT >> 31;
    while (n > 0) {
        uint32_t b = GPUSTAT >> 31;
        if (b != last) { last = b; n--; }
    }
}

/* ---- パッド ---- */

#define JOY_DATA (*(volatile uint8_t *)0x1F801040)
#define JOY_STAT (*(volatile uint32_t *)0x1F801044)
#define JOY_MODE (*(volatile uint16_t *)0x1F801048)
#define JOY_CTRL (*(volatile uint16_t *)0x1F80104A)
#define JOY_BAUD (*(volatile uint16_t *)0x1F80104E)

static int pad_exchange(const uint8_t *tx, int txn, uint8_t *rx)
{
    JOY_CTRL = 0x1003;
    JOY_MODE = 0x000D;
    JOY_BAUD = 0x88;
    int total = txn;
    int n = 0;
    for (int i = 0; i < total; i++) {
        int t = 2000;
        while (!(JOY_STAT & 0x1) && --t) { }
        if (!t) break;
        JOY_DATA = i < txn ? tx[i] : 0;
        t = 2000;
        while (!(JOY_STAT & 0x2) && --t) { }
        if (!t) break;
        rx[n++] = JOY_DATA;
        for (volatile int d = 0; d < 60; d++) { }
        if (i == 1) {
            int want = 3 + (rx[1] & 0x0F) * 2;
            if (want > total) total = want > 9 ? 9 : want;
        }
    }
    JOY_CTRL = 0;
    return n;
}

void pad_try_analog(void)
{
    static const uint8_t enter[9] = {0x01, 0x43, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t setan[9] = {0x01, 0x44, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t exitc[9] = {0x01, 0x43, 0x00, 0x00, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A};
    uint8_t rx[9];
    pad_exchange(enter, 5, rx);
    for (volatile int d = 0; d < 300; d++) { }
    pad_exchange(setan, 9, rx);
    for (volatile int d = 0; d < 300; d++) { }
    pad_exchange(exitc, 9, rx);
}

int pad_read_ex(uint8_t axes[4])
{
    static const uint8_t tx[5] = {0x01, 0x42, 0, 0, 0};
    uint8_t rx[9];
    axes[0] = axes[1] = axes[2] = axes[3] = 0x80;
    int n = pad_exchange(tx, 5, rx);
    if (n >= 9 && rx[1] == 0x73) {
        axes[0] = rx[5];
        axes[1] = rx[6];
        axes[2] = rx[7];
        axes[3] = rx[8];
        return ~(rx[3] | (rx[4] << 8)) & 0xFFFF;
    }
    if (n >= 5 && rx[1] == 0x41)
        return ~(rx[3] | (rx[4] << 8)) & 0xFFFF;
    return -1;
}

int pad_read(void)
{
    uint8_t axes[4];
    return pad_read_ex(axes);
}

static int rs_held;                    /* フリック検出の倒し込み状態(render_init で零化) */

/* 右スティックのフリック検出。倒し込んだ瞬間に優勢軸の方向を 1 回だけ返し、
 * ニュートラル(閾値 0.5 未満)へ戻るまで再発火しない。1=左 2=上 3=右 4=下 */
int pad_rstick_flick(const uint8_t axes[4])
{
    int dx = (int)axes[0] - 0x80, dy = (int)axes[1] - 0x80;
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    int dir = 0;
    if (ax >= 0x40 || ay >= 0x40)
        dir = ax >= ay ? (dx < 0 ? 1 : 3) : (dy < 0 ? 2 : 4);
    int fire = dir && !rs_held;
    rs_held = dir;
    return fire ? dir : 0;
}

/* ---- グリフ(クリップつき描画) ---- */

static int clip_y0, clip_y1;
static int render_dry;                 /* 1 = 採寸のみ(描かない) */

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

void draw24(uint16_t (*buf)[W], int x, int y, uint16_t code, uint16_t color)
{
    int i = find_glyph(base_info, BASE_N, code);
    if (i < 0) return;
    for (int r = 0; r < 24; r++) {
        int ry = y + r;
        if (ry < clip_y0 || ry >= clip_y1) continue;
        unsigned bits = base_rows[i][r];
        for (int c = 0; bits; c++, bits >>= 1)
            if (bits & 1) buf[ry][x + c] = color;
    }
}

void draw12(uint16_t (*buf)[W], int x, int y, uint16_t code, uint16_t color)
{
    int i = find_glyph(ruby_info, RUBY_N, code);
    if (i < 0) return;
    for (int r = 0; r < 12; r++) {
        int ry = y + r;
        if (ry < clip_y0 || ry >= clip_y1) continue;
        unsigned bits = ruby_rows[i][r];
        for (int c = 0; bits; c++, bits >>= 1)
            if (bits & 1) buf[ry][x + c] = color;
    }
}

/* ---- 割り付け(gen_mock.py の移植) ---- */

typedef struct { uint16_t ch; uint8_t blen, rlen; const uint16_t *base, *ruby; uint16_t w; } Frag;
static Frag frags[96];
static int nfrag, fw;

void flush_vline(uint16_t color)
{
    if (!nfrag) return;
    int is_jp = 0;
    for (int i = 0; i < nfrag && !is_jp; i++) {
        Frag *f = &frags[i];
        if (f->rlen) {
            for (int k = 0; k < f->blen; k++)
                if (f->base[k] > 0x7F) { is_jp = 1; break; }
        } else if (f->ch > 0x7F) {
            is_jp = 1;
        }
    }
    cursor += is_jp ? RUBY_ZONE : (first_line ? 0 : LEAD);
    first_line = 0;
    if (!render_dry) {
        int x = MARGIN;
        for (int i = 0; i < nfrag; i++) {
            Frag *f = &frags[i];
            if (f->rlen) {
                int bw = 24 * f->blen, rw = 12 * f->rlen;
                int bx = x + (f->w - bw) / 2, rx = x + (f->w - rw) / 2;
                for (int k = 0; k < f->blen; k++)
                    draw24(canvas, bx + 24 * k, cursor, f->base[k], color);
                for (int k = 0; k < f->rlen; k++)
                    draw12(canvas, rx + 12 * k, cursor - 13, f->ruby[k], color);
            } else {
                draw24(canvas, x, cursor, f->ch, color);
            }
            x += f->w;
        }
    }
    cursor += BASE;
    nfrag = 0;
    fw = 0;
}

void push_char(uint16_t ch, uint16_t color)
{
    int w = glyph_w(ch);
    if (fw + w > TEXT_W && fw > 0) {
        flush_vline(color);
        if (ch == ' ') return;         /* 折り返し直後の空白は捨てる */
    }
    frags[nfrag++] = (Frag){ch, 1, 0, 0, 0, (uint16_t)w};
    fw += w;
}

void push_text(const uint16_t *s, int n, uint16_t color)
{
    int i = 0;
    while (i < n) {
        uint16_t ch = s[i];
        if (ch > 0x7F || ch == ' ') {
            push_char(ch, color);
            i++;
            continue;
        }
        int j = i, w = 0;
        while (j < n && s[j] <= 0x7F && s[j] != ' ')
            w += glyph_w(s[j++]);
        if (w <= TEXT_W && fw + w > TEXT_W && fw > 0)
            flush_vline(color);
        for (; i < j; i++)
            push_char(s[i], color);
    }
}

void push_ruby(const uint16_t *base, int blen, const uint16_t *ruby, int rlen, uint16_t color)
{
    int bw = 24 * blen, rw = 12 * rlen;
    int w = bw > rw ? bw : rw;
    if (fw + w > TEXT_W && fw > 0)
        flush_vline(color);
    frags[nfrag++] = (Frag){0, (uint8_t)blen, (uint8_t)rlen, base, ruby, (uint16_t)w};
    fw += w;
}

/* ---- 行の描画器(差し替え可能) ---- */

static void plain_render(const uint16_t *s, int n, uint16_t color)
{
    push_text(s, n, color);
    flush_vline(color);
}

void (*line_render)(const uint16_t *s, int n, uint16_t color) = plain_render;

/* ---- テキスト履歴 ---- */

enum { HPOOL_N = 49152, HLINE_N = 3072 };
typedef struct { uint32_t y; uint32_t off; uint16_t len; uint16_t color; } HLine;
static uint16_t hpool[HPOOL_N];
static HLine hline[HLINE_N];
static int hhead, hcount;              /* 環(hhead から hcount 本) */
static uint32_t wpos;                  /* hpool の書込位置 */
static uint32_t total_h;               /* 履歴の総高さ(px。捨てた分を含む) */
static int stale_y = -1;               /* 窓内に食い込んだ未描画の追記の先頭 hist 行(-1 = 無し) */

static HLine *hl(int i) { return &hline[(hhead + i) % HLINE_N]; }

static void hist_evict(void)
{
    if (hcount == 0) return;
    hhead = (hhead + 1) % HLINE_N;
    hcount--;
}

static uint32_t hpool_alloc(int n)
{
    if (wpos + (uint32_t)n > HPOOL_N)
        wpos = 0;
    for (;;) {                          /* 書込先と重なる古い行を捨てる */
        if (hcount == 0) break;
        HLine *h = hl(0);
        if (h->len && h->off < wpos + (uint32_t)n && h->off + h->len > wpos)
            hist_evict();
        else
            break;
    }
    uint32_t off = wpos;
    wpos += (uint32_t)n;
    return off;
}

int hist_min_view(void)
{
    return hcount ? (int)hl(0)->y : (int)total_h;
}

int hist_total(void) { return (int)total_h; }

void hist_line(const uint16_t *s, int n, uint16_t color)
{
    if (n > 1000) n = 1000;
    if (hcount >= HLINE_N - 1)
        hist_evict();
    int h;
    if (n == 0) {
        h = BLANK;
        first_line = 0;
    } else {
        render_dry = 1;
        first_line = total_h == 0;
        cursor = 0;
        nfrag = 0;
        fw = 0;
        line_render(s, n, color);
        h = cursor;
        render_dry = 0;
    }
    uint32_t off = n ? hpool_alloc(n) : 0;
    for (int i = 0; i < n; i++)
        hpool[off + i] = s[i];
    HLine *nl = &hline[(hhead + hcount) % HLINE_N];
    nl->y = total_h;
    nl->off = off;
    nl->len = (uint16_t)n;
    nl->color = color;
    /* 本文が窓より短い間は、追記した行が「すでに窓の内側」に落ちる。
     * 高速スクロールは窓内を描き直さないので、ここで覚えて後で描き足す */
    if (stale_y < 0 && (int)nl->y < view_px + body_h)
        stale_y = (int)nl->y;
    hcount++;
    total_h += (uint32_t)h;
}

void hist_blank(void)
{
    hist_line(0, 0, INK);
}

/* ---- 表示窓 ---- */

void fill_rows(uint16_t (*buf)[W], int y, int h, uint16_t color)
{
    uint32_t v = (uint32_t)color | ((uint32_t)color << 16);
    uint32_t *p = (uint32_t *)buf[y];
    for (int i = 0; i < h * W / 2; i++)
        p[i] = v;
}

static void view_clamp(void)
{
    int lo = hist_min_view();
    int hi = (int)total_h - body_h;
    if (hi < lo) hi = lo;
    if (view_px < lo) view_px = lo;
    if (view_px > hi) view_px = hi;
}

void render_window(void)
{
    view_clamp();
    stale_y = -1;                      /* 全面を描き直すので追記漏れは消える */
    fill_rows(canvas, 0, body_h, BG);
    clip_y0 = 0;
    clip_y1 = body_h;
    for (int i = 0; i < hcount; i++) {
        HLine *h = hl(i);
        uint32_t hh = (i + 1 < hcount ? hl(i + 1)->y : total_h) - h->y;
        if ((int)(h->y + hh) <= view_px) continue;
        if ((int)h->y >= view_px + body_h) break;
        if (!h->len) continue;
        first_line = h->y == 0;
        cursor = (int)h->y - view_px;
        nfrag = 0;
        fw = 0;
        line_render(hpool + h->off, h->len, h->color);
    }
    clip_y0 = 0;
    clip_y1 = WIN_H;
    gp0_upload(0, body_top, W, body_h, canvas[0]);
}

void view_bottom(void)
{
    view_px = (int)total_h - body_h;
    view_clamp();
}

static void gp0_copy(int sx, int sy, int dx, int dy, int w, int h)
{
    gpu_sync();
    GP0 = 0x80000000;                  /* VRAM 面内コピー */
    GP0 = (uint32_t)(sy << 16) | (uint32_t)sx;
    GP0 = (uint32_t)(dy << 16) | (uint32_t)dx;
    GP0 = (uint32_t)(h << 16) | (uint32_t)w;
}

/* band 行範囲 [y0,y1) に、そこへかかる履歴行だけを描く(bg 済み前提) */
static void render_lines_range(int y0, int y1)
{
    clip_y0 = y0;
    clip_y1 = y1;
    for (int i = 0; i < hcount; i++) {
        HLine *h = hl(i);
        uint32_t hh = (i + 1 < hcount ? hl(i + 1)->y : total_h) - h->y;
        if ((int)(h->y + hh) <= view_px + y0) continue;
        if ((int)h->y >= view_px + y1) break;
        if (!h->len) continue;
        first_line = h->y == 0;
        cursor = (int)h->y - view_px;
        nfrag = 0;
        fw = 0;
        line_render(hpool + h->off, h->len, h->color);
    }
    clip_y0 = 0;
    clip_y1 = WIN_H;
}

/* 窓内に食い込んだ未描画の追記を描き足す(view_px を動かす前に呼ぶ) */
static void refresh_stale(void)
{
    if (stale_y < 0)
        return;
    int y0 = stale_y - view_px;
    stale_y = -1;
    if (y0 >= body_h)
        return;
    if (y0 < 0)
        y0 = 0;
    int y1 = (int)total_h - view_px;   /* 本文の末尾まで(それより下は BG のまま) */
    if (y1 > body_h)
        y1 = body_h;
    if (y1 <= y0)
        return;
    fill_rows(canvas, y0, y1 - y0, BG);
    render_lines_range(y0, y1);
    gp0_upload(0, body_top + y0, W, y1 - y0, canvas[y0]);
}

/* d だけずらす(高速路: 画面は GPU コピー、露出帯だけ描き直す)。動いたら 1 */
int view_scroll(int d)
{
    refresh_stale();
    int old = view_px;
    view_px += d;
    view_clamp();
    int delta = view_px - old;
    if (!delta)
        return 0;
    int ad = delta > 0 ? delta : -delta;
    if (ad >= body_h) {
        render_window();
        return 1;
    }
    /* 露出帯は先に CPU で描いておき、コピー→転送を隙間なく連続発行する。
     * (コピーの後に描くと、描いている数 ms のあいだ端の帯に古い画素が
     *  見えたまま走査され、スクロール中に端がばたつく) */
    if (delta > 0) {
        int y0 = body_h - ad;
        fill_rows(canvas, y0, ad, BG);
        render_lines_range(y0, body_h);
        /* 内容が上へ流れる: 上から ad 刻みでコピー(各チャンクは重ならない) */
        for (int y = 0; y < body_h - ad; y += ad) {
            int h = body_h - ad - y < ad ? body_h - ad - y : ad;
            gp0_copy(0, body_top + y + ad, 0, body_top + y, W, h);
        }
        gp0_upload(0, body_top + y0, W, ad, canvas[y0]);
    } else {
        fill_rows(canvas, 0, ad, BG);
        render_lines_range(0, ad);
        /* 内容が下へ流れる: 下から ad 刻みでコピー */
        int y = body_h - ad;
        while (y > 0) {
            int h = y < ad ? y : ad;
            gp0_copy(0, body_top + y - h, 0, body_top + y - h + ad, W, h);
            y -= h;
        }
        gp0_upload(0, body_top, W, ad, canvas[0]);
    }
    return 1;
}

/* 新しい内容が積まれた後、下端まで半行送りで見せる */
void scroll_new(void)
{
    int target = (int)total_h - body_h;
    int lo = hist_min_view();
    if (target < lo) target = lo;
    if (view_px >= target) {
        view_px = target;
        render_window();
        return;
    }
    while (view_px < target) {
        int step = target - view_px < SCROLL_STEP ? target - view_px : SCROLL_STEP;
        wait_fields(1);
        view_scroll(step);
    }
}

void render_init(void)
{
    nfrag = 0;
    fw = 0;
    render_dry = 0;
    clip_y0 = 0;
    clip_y1 = WIN_H;
    body_top = TOP;
    body_h = BODY_H;
    stale_y = -1;
    rs_held = 0;
    hhead = 0;
    hcount = 0;
    wpos = 0;
    total_h = 0;
    view_px = 0;
    first_line = 1;
    cursor = 0;
}

/* ---- 互換ラッパ(既存 main 用) ---- */

void draw_plain(const uint16_t *s, int n, uint16_t color)
{
    hist_line(s, n, color);
}

void draw_logical(const Line *ln, uint16_t color)
{
    if (ln->cnt == 0) {
        hist_blank();
        return;
    }
    uint16_t buf[192];
    int n = 0;
    for (int i = 0; i < ln->cnt; i++) {
        const Item *it = &citems[ln->off + i];
        for (int k = 0; k < it->blen && n < 192; k++)
            buf[n++] = pool[it->boff + k];
    }
    hist_line(buf, n, color);
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

/* ---- コマンド欄 ---- */

/* コマンド欄を strip[][] に組む(送りはしない)。
   ★試し打ちの行が**同じ絵**を使えるように分けてある —— 描き方を写すと、
     本物と模したものがいつか食い違う。 */
void build_strip(uint16_t bg, const uint16_t *cmd, int len, int caret,
                 const uint16_t *ind, int ilen)
{
    fill_rows(strip, 0, CMD_H, bg);
    draw24(strip, MARGIN, 0, 0xFF1E, ACCENT);
    int x = MARGIN + 24;
    int caret_x = x;
    for (int i = 0; i < len; i++) {
        if (i == caret) caret_x = x;
        draw24(strip, x, 0, cmd[i], INK);
        x += glyph_w(cmd[i]);
    }
    if (caret >= len) {
        caret_x = x;
        for (int r = 0; r < 24; r++)
            for (int c = 0; c < 3; c++) strip[r][caret_x + 2 + c] = ACCENT;
    } else {
        for (int r = 0; r < 24; r++)
            for (int c = 0; c < 2; c++) strip[r][caret_x - 1 + c] = ACCENT;
    }
    int iw = 0;
    for (int i = 0; i < ilen; i++)
        iw += glyph_w(ind[i]);
    if (iw < 24) iw = 24;
    int kx = W - MARGIN - iw;
    for (int c = kx - 3; c <= kx + iw + 2; c++) strip[0][c] = strip[CMD_H - 1][c] = DIM;
    for (int r = 0; r < CMD_H; r++) strip[r][kx - 3] = strip[r][kx + iw + 2] = DIM;
    for (int i = 0, ix = kx; i < ilen; i++) {
        draw24(strip, ix, 0, ind[i], ACCENT);
        ix += glyph_w(ind[i]);
    }
}

void draw_strip(const uint16_t *cmd, int len, int caret, const uint16_t *ind, int ilen)
{
    build_strip(BG, cmd, len, caret, ind, ilen);
    gp0_upload(0, CMD_Y, W, CMD_H, strip[0]);
}
