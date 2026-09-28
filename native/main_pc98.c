/* Zenmai PC-98 版の入口（計画 = docs/pc98-port-plan.md）。
 *
 * ★芯（VM・訳・語彙・セーブ）は session.c で、PS1 / SDL と共有している。
 *   ここに在るのは PC-98 の画面の外枠（上の帯・装飾・入力欄）と、入力の取り方だけ。
 *   本文は render_pc98.c（render.h の文字列の口）。
 *
 * 画面（1 行 24 ラスタ × 16 行。寸法と色は試作 pc98-mock/gen_screen.py と同じ・色は仮）:
 *   上の帯      縦 0〜31    0 行目に場所（左・黄）と得点（右）
 *   本文の上    縦 32〜44   装飾と同じ色
 *   左右の装飾  幅 48
 *   本文        2〜13 行目・横 64〜575（全角 32 字 × 12 行）
 *   入力欄の枠  縦 352〜399 15 行目。上に 2px の縁
 *
 * 入力は 2 通り:
 *   ZENMAI              … キーボード（BIOS から直に読む）。ローマ字 / カナキーでかな、CAPS で英字
 *                         （kana_input.h）。ROLL DOWN / ↑ で本文を遡り、ROLL UP / ↓ で戻る。
 *                         やめるのはゲームの「やめる」（quit）
 *   ZENMAI /S 台本.TXT  … 台本（UTF-8・1 行 1 コマンド）を流し、ZENMAI.LOG に記録する。
 *                         `#!keys` の後の行は**打鍵として**キーボードと同じ道（ki_key）を通す
 *                         （`#!text` で戻る。半角カナはカナキーの打鍵になる）。
 *                         `#!line 文` は文をそのまま本文に流す（組み方を画面で確かめる用。VM は回さない）。
 *                         ★同じものをホストでも建てて（build-pc98.sh）記録を突き合わせる（test-pc98.sh）
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc98_text.h"
#include "pc98_gfx.h"
#include "render.h"
#include "render_pc98.h"
#include "session.h"
#include "translate.h"
#include "kana_input.h"

#ifndef PC98_HOST
#include <i86.h>
#endif

extern const uint8_t zm_story[];       /* story_pc98.c（build-pc98.sh が zork1.z3 から焼く） */
extern const uint32_t zm_story_len;

enum { ROW_STATUS = 0, ROW_INPUT = 15, COL_L = BODY_COL0, COL_R = BODY_COL0 + BODY_CELLS };
static KiLine line;                    /* 入力欄 */

/* ---- 画面の外枠 ---- */

enum { C_BG, C_BAND, C_DECO, C_INPUT, C_INPUT_EDGE };   /* パレットの番号（8 = ふりがな） */

static void draw_chrome(void)
{
    gfx_palette(C_BG, 0, 0, 0);
    gfx_palette(C_BAND, 3, 2, 7);      /* 上の帯（紺） */
    gfx_palette(C_DECO, 4, 7, 3);      /* 左右の装飾（焦げ茶） */
    gfx_palette(C_INPUT, 5, 2, 5);     /* 入力欄の枠 */
    gfx_palette(C_INPUT_EDGE, 9, 5, 9);
    gfx_palette(RUBY_COLOR, 10, 10, 10);
    enum { TOP_H = 32, SIDE = 48, IN_Y = 352,
           TOP_DECO = BODY_ROW0 * TXT_RASTERS + RUBY_DY - 4 };   /* 本文 1 行目のふりがなの 4 ラスタ上 */
    gfx_rect(0, 0, GFX_W, TOP_H, C_BAND);
    gfx_rect(0, TOP_H, SIDE, IN_Y, C_DECO);
    gfx_rect(GFX_W - SIDE, TOP_H, GFX_W, IN_Y, C_DECO);
    gfx_rect(0, TOP_H, GFX_W, TOP_DECO, C_DECO);
    gfx_rect(0, IN_Y, GFX_W, GFX_H, C_INPUT);
    gfx_rect(0, IN_Y, GFX_W, IN_Y + 2, C_INPUT_EDGE);
}

static void log_status(const uint16_t *s, int n)
{
    fputs("# ", render_log);
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

/* 上の帯: 左 = 部屋名（訳す・黄）/ 右 = 得点。境目は Z-machine の桁埋めの空白 2 つ */
static void draw_status(void)
{
    const char *sb = sess_status();
    txt_clear(ROW_STATUS, ROW_STATUS, TA_WHITE);
    int name_end = 0;
    while (sb[name_end] && !(sb[name_end] == ' ' && sb[name_end + 1] == ' '))
        name_end++;
    uint16_t name[64];
    const int nn = tr_word_str(sb, name_end, name, 64);
    int col = COL_L;
    for (int i = 0; i < nn && col < COL_R - 24; i++)
        col += txt_put(ROW_STATUS, col, name[i], TA_YELLOW);
    int re = name_end;
    while (sb[re] == ' ') re++;
    int rl = 0;
    while (sb[re + rl]) rl++;
    while (rl > 0 && sb[re + rl - 1] == ' ') rl--;
    uint16_t sc[48];
    for (int i = 0; i < rl && i < 48; i++) {
        sc[i] = (uint8_t)sb[re + i];
        txt_put(ROW_STATUS, COL_R - rl + i, sc[i], TA_WHITE);
    }
    if (render_log) {
        log_status(name, nn);
        log_status(sc, rl < 48 ? rl : 48);
    }
}

static void die(const char *msg)
{
    gfx_fini();
    txt_fini();
    printf("zenmai: VM が止まった (%s)\n", msg);
    exit(1);
}

/* ---- 入力 ---- */

/* 台本の 1 行（UTF-8）を入力欄へ。戻り値 0 = 台本の終わり。
   ★`#!keys` の後は 1 字ずつ打鍵として ki_key を通す（キーボードと同じ道）。半角カナはカナキーの打鍵。
   ★`#!line 文` は文をそのまま本文に流して次の行へ進む（VM は回さない） */
static int script_line(FILE *f)
{
    static char buf[512];
    static uint16_t t[512];
    static int keys;
    for (;;) {
        if (!fgets(buf, sizeof buf, f))
            return 0;
        if (!strncmp(buf, "#!keys", 6)) { keys = 1; continue; }
        if (!strncmp(buf, "#!text", 6)) { keys = 0; continue; }
        const int show = !strncmp(buf, "#!line ", 7);
        int n = 0;
        const unsigned char *p = (const unsigned char *)buf + (show ? 7 : 0);
        while (*p && *p != '\n' && *p != '\r' && n < 512) {
            unsigned u = *p++;
            if (u >= 0xE0 && p[0] && p[1]) {
                u = (u & 0x0F) << 12 | (p[0] & 0x3F) << 6 | (p[1] & 0x3F);
                p += 2;
            } else if (u >= 0xC0 && p[0]) {
                u = (u & 0x1F) << 6 | (p[0] & 0x3F);
                p += 1;
            }
            t[n++] = (uint16_t)u;
        }
        if (show) {
            draw_plain(t, n, INK);
            continue;
        }
        ki_clear(&line);
        for (int i = 0; i < n; i++) {
            if (!keys) {
                if (line.n < KI_MAX) line.buf[line.n++] = t[i];
            } else if (t[i] >= 0xFF61 && t[i] <= 0xFF9F) {
                ki_key(&line, t[i] - 0xFF61 + 0xA1, 0);    /* 半角カナ → カナキーの打鍵 */
            } else {
                ki_key(&line, t[i], 0);
            }
        }
        ki_commit(&line);
        if (line.n && line.buf[0] != '#')    /* 空行と # の行は飛ばす */
            return 1;
    }
}

#ifndef PC98_HOST
static union REGS bios18(int ah)
{
    union REGS r;
    memset(&r, 0, sizeof r);
    r.h.ah = (unsigned char)ah;
    int386(0x18, &r, &r);
    return r;
}
static int kbd_caps(void) { return (bios18(0x02).h.al >> 1) & 1; }   /* シフト状態の bit1 */
static int kbd_hit(void) { return bios18(0x01).h.bh != 0; }
static int kbd_get(void) { return bios18(0x00).w.ax; }                /* 上 = キーの番号・下 = 字 */
#else
static int kbd_caps(void) { return 0; }
#endif

/* 入力欄: ＞ + 確定した字 + 組み立て途中のローマ字 + カーソル（反転の空白）。右端に打ち方（かな / 英字） */
static void draw_input(int caret)
{
    static const uint16_t kana[] = { 0x304B, 0x306A }, eng[] = { 0x82F1, 0x5B57 };
    txt_clear(ROW_INPUT, ROW_INPUT, TA_WHITE);
    const uint16_t *mode = kbd_caps() ? eng : kana;
    for (int i = 0; i < 2; i++)
        txt_put(ROW_INPUT, COL_R - 4 + 2 * i, mode[i], TA_CYAN);
    int col = COL_L;
    col += txt_put(ROW_INPUT, col, 0xFF1E, TA_CYAN);     /* ＞ */
    for (int i = 0; i < line.n; i++)
        col += txt_put(ROW_INPUT, col, line.buf[i], TA_WHITE);
    for (int i = 0; i < line.np; i++)
        col += txt_put(ROW_INPUT, col, (uint8_t)line.pend[i], TA_WHITE);
    if (caret)
        txt_put(ROW_INPUT, col, ' ', TA_WHITE | TA_REV);
}

#ifndef PC98_HOST
enum { K_ROLLUP = 0x36, K_ROLLDOWN = 0x37, K_UP = 0x3A, K_DOWN = 0x3D };

/* キーボードから 1 行（空でない行を Enter で送るまで戻らない） */
static void key_line(void)
{
    ki_clear(&line);
    draw_input(1);
    int caps = kbd_caps();
    for (;;) {
        while (!kbd_hit())             /* ★CAPS は字を出さないので、待つ間に見張って打ち方の表示を直す */
            if (kbd_caps() != caps) {
                caps = !caps;
                draw_input(1);
            }
        const int k = kbd_get(), scan = k >> 8 & 0x7F, c = k & 0xFF;
        if (scan == K_ROLLDOWN) { body_scroll(-(BODY_ROWS - 1)); continue; }
        if (scan == K_ROLLUP)   { body_scroll(BODY_ROWS - 1); continue; }
        if (scan == K_UP)       { body_scroll(-1); continue; }
        if (scan == K_DOWN)     { body_scroll(1); continue; }
        if (c == '\r') {
            ki_commit(&line);
            if (line.n) break;
            draw_input(1);
            continue;
        }
        if (c == 0x08)
            ki_backspace(&line);
        else if (c && c != 0x1B)       /* ★ESC とファンクションキー（字 0）は読み捨てる */
            ki_key(&line, c, caps);
        draw_input(1);
    }
    draw_input(0);
    if (!body_at_bottom()) {           /* 遡り中の確定は、まず下端へ跳んでひと呼吸置く */
        body_show(0);
        for (int i = 0; i < 18; i++) txt_vsync();
    }
}
#endif

int main(int argc, char **argv)
{
    FILE *script = 0;
#ifdef PC98_HOST
    if (argc < 2) {
        fprintf(stderr, "使い方: %s 台本.txt（記録は ZENMAI.LOG）\n", argv[0]);
        return 2;
    }
    script = fopen(argv[1], "rb");
#else
    if (argc >= 3 && (argv[1][0] == '/' || argv[1][0] == '-') && (argv[1][1] | 0x20) == 's')
        script = fopen(argv[2], "rb");
#endif
    if (argc >= 2 && !script) {
        printf("zenmai: 台本を開けない\n");
        return 1;
    }
    if (script)
        render_log = fopen("ZENMAI.LOG", "wb");

    txt_init();
    gfx_init();
    draw_chrome();
    jp_text_init();                    /* ふりがなを分ける描画器（jp_text.c）を本文に登録する */
    sess_start(0, zm_story, zm_story_len, die);
    draw_status();
    body_show(0);

    int pending_verb = -1;
    for (;;) {
        if (script) {
            if (!script_line(script)) break;
            draw_input(0);
        } else {
#ifndef PC98_HOST
            key_line();
#endif
        }
        const int no_turn = sess_submit_ja(line.buf, line.n, &pending_verb);
        ki_clear(&line);
        if (!no_turn)
            draw_status();
        body_show(!script);
        if (sess_quit()) break;
    }

    if (script) {
        body_show(0);                  /* 台本の終わりの #!line を見せる */
        fclose(script);
        if (render_log) fclose(render_log);
        render_log = 0;
#ifndef PC98_HOST
        /* ★台本の終わりの印（撮る側がテキスト VRAM の ASCII で待つ）。キーで DOS へ */
        static const char end[] = "[END]";
        txt_clear(ROW_INPUT, ROW_INPUT, TA_WHITE);
        for (int i = 0; end[i]; i++)
            txt_put(ROW_INPUT, COL_L + i, (uint8_t)end[i], TA_GREEN);
        kbd_get();
#endif
    }
    gfx_fini();
    txt_fini();
    return 0;
}
