/* Zenmai PC-98 版の入口（計画 = docs/pc98-port-plan.md）。
 *
 * ★芯（VM・訳・語彙・セーブ）は session.c で、PS1 / SDL と共有している。
 *   ここに在るのは PC-98 の画面の外枠（状態行・入力欄）と、入力の取り方だけ。
 *   本文は render_pc98.c（render.h の文字列の口）。
 *
 * ★段 2 の画面はまだ仮（標準の 80×25・ふりがな無し）。24 ラスタ × 16 行は段 4。
 *
 * 入力は 2 通り:
 *   ZENMAI              … キーボード。ローマ字 / カナキーでかな、CAPS で英字（kana_input.h）。
 *                         やめるのはゲームの「やめる」（quit）
 *   ZENMAI /S 台本.TXT  … 台本（UTF-8・1 行 1 コマンド）を流し、ZENMAI.LOG に記録する。
 *                         `#!keys` の後の行は**打鍵として**キーボードと同じ道（ki_key）を通す
 *                         （`#!text` で戻る。半角カナはカナキーの打鍵になる）。
 *                         ★同じものをホストでも建てて（build-pc98.sh）記録を突き合わせる（test-pc98.sh）
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc98_text.h"
#include "render_pc98.h"
#include "session.h"
#include "translate.h"
#include "kana_input.h"

#ifndef PC98_HOST
#include <conio.h>
#include <i86.h>
#endif

extern const uint8_t zm_story[];       /* story_pc98.c（build-pc98.sh が zork1.z3 から焼く） */
extern const uint32_t zm_story_len;

enum { ROW_STATUS = 0, ROW_INPUT = 24 };
static KiLine line;                    /* 入力欄 */

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

/* 状態行: 左 = 部屋名（訳す）/ 右 = 得点。境目は Z-machine の桁埋めの空白 2 つ */
static void draw_status(void)
{
    const char *sb = sess_status();
    const uint8_t a = TA_CYAN | TA_REV;
    txt_clear(ROW_STATUS, ROW_STATUS, a);
    int name_end = 0;
    while (sb[name_end] && !(sb[name_end] == ' ' && sb[name_end + 1] == ' '))
        name_end++;
    uint16_t name[64];
    const int nn = tr_word_str(sb, name_end, name, 64);
    int col = 2;
    for (int i = 0; i < nn && col < 50; i++)
        col += txt_put(ROW_STATUS, col, name[i], a);
    int re = name_end;
    while (sb[re] == ' ') re++;
    int rl = 0;
    while (sb[re + rl]) rl++;
    while (rl > 0 && sb[re + rl - 1] == ' ') rl--;
    uint16_t sc[48];
    for (int i = 0; i < rl && i < 48; i++) {
        sc[i] = (uint8_t)sb[re + i];
        txt_put(ROW_STATUS, TXT_COLS - 2 - rl + i, sc[i], a);
    }
    if (render_log) {
        log_status(name, nn);
        log_status(sc, rl < 48 ? rl : 48);
    }
}

static void die(const char *msg)
{
    txt_fini();
    printf("zenmai: VM が止まった (%s)\n", msg);
    exit(1);
}

/* ---- 入力 ---- */

/* 台本の 1 行（UTF-8）を入力欄へ。戻り値 0 = 台本の終わり。
   ★`#!keys` の後は 1 字ずつ打鍵として ki_key を通す（キーボードと同じ道）。半角カナはカナキーの打鍵 */
static int script_line(FILE *f)
{
    static char buf[512];
    static int keys;
    for (;;) {
        if (!fgets(buf, sizeof buf, f))
            return 0;
        if (!strncmp(buf, "#!keys", 6)) { keys = 1; continue; }
        if (!strncmp(buf, "#!text", 6)) { keys = 0; continue; }
        ki_clear(&line);
        const unsigned char *p = (const unsigned char *)buf;
        while (*p && *p != '\n' && *p != '\r') {
            unsigned u = *p++;
            if (u >= 0xE0 && p[0] && p[1]) {
                u = (u & 0x0F) << 12 | (p[0] & 0x3F) << 6 | (p[1] & 0x3F);
                p += 2;
            } else if (u >= 0xC0 && p[0]) {
                u = (u & 0x1F) << 6 | (p[0] & 0x3F);
                p += 1;
            }
            if (!keys) {
                if (line.n < KI_MAX) line.buf[line.n++] = (uint16_t)u;
            } else if (u >= 0xFF61 && u <= 0xFF9F) {
                ki_key(&line, (int)(u - 0xFF61 + 0xA1), 0);   /* 半角カナ → カナキーの打鍵 */
            } else {
                ki_key(&line, (int)u, 0);
            }
        }
        ki_commit(&line);
        if (line.n && line.buf[0] != '#')    /* 空行と # の行は飛ばす */
            return 1;
    }
}

#ifndef PC98_HOST
/* CAPS が入っているか（INT 18h AH=02h のシフト状態・bit1） */
static int kbd_caps(void)
{
    union REGS r;
    memset(&r, 0, sizeof r);
    r.h.ah = 0x02;
    int386(0x18, &r, &r);
    return (r.h.al >> 1) & 1;
}
#else
static int kbd_caps(void) { return 0; }
#endif

/* 入力欄: ＞ + 確定した字 + 組み立て途中のローマ字。右端に打ち方（かな / 英字） */
static void draw_input(void)
{
    static const uint16_t kana[] = { 0x304B, 0x306A }, eng[] = { 0x82F1, 0x5B57 };
    txt_clear(ROW_INPUT, ROW_INPUT, TA_WHITE);
    const uint16_t *mode = kbd_caps() ? eng : kana;
    for (int i = 0; i < 2; i++)
        txt_put(ROW_INPUT, PC98_BODY_R - 4 + 2 * i, mode[i], TA_CYAN);
    int col = PC98_BODY_L;
    col += txt_put(ROW_INPUT, col, 0xFF1E, TA_YELLOW);   /* ＞ */
    for (int i = 0; i < line.n; i++)
        col += txt_put(ROW_INPUT, col, line.buf[i], TA_WHITE);
    for (int i = 0; i < line.np; i++)
        col += txt_put(ROW_INPUT, col, (uint8_t)line.pend[i], TA_WHITE);
    txt_cursor(ROW_INPUT, col);
}

#ifndef PC98_HOST
/* キーボードから 1 行（空でない行を Enter で送るまで戻らない） */
static void key_line(void)
{
    ki_clear(&line);
    draw_input();
    int caps = kbd_caps();
    for (;;) {
        while (!kbhit())               /* ★CAPS は字を出さないので、待つ間に見張って打ち方の表示を直す */
            if (kbd_caps() != caps) {
                caps = !caps;
                draw_input();
            }
        const int c = getch();
        if (c == 0x1B) {               /* ★ESC はファンクションキーなどの列の頭。続きごと読み捨てる */
            while (kbhit()) getch();
            continue;
        }
        if (c == '\r' || c == '\n') {
            ki_commit(&line);
            if (line.n) break;
            draw_input();
            continue;
        }
        if (c == 0x08)
            ki_backspace(&line);
        else
            ki_key(&line, c, caps);
        draw_input();
    }
    draw_input();
    txt_cursor(-1, 0);
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
    sess_start(0, zm_story, zm_story_len, die);
    draw_status();

    int pending_verb = -1;
    for (;;) {
        if (script) {
            if (!script_line(script)) break;
            draw_input();
        } else {
#ifndef PC98_HOST
            key_line();
#endif
        }
        const int no_turn = sess_submit_ja(line.buf, line.n, &pending_verb);
        ki_clear(&line);
        if (!no_turn)
            draw_status();
        if (sess_quit()) break;
    }

    if (script) {
        fclose(script);
        if (render_log) fclose(render_log);
        render_log = 0;
#ifndef PC98_HOST
        /* ★台本の終わりの印（撮る側がテキスト VRAM の ASCII で待つ）。キーで DOS へ */
        static const char end[] = "[END]";
        txt_clear(ROW_INPUT, ROW_INPUT, TA_WHITE);
        for (int i = 0; end[i]; i++)
            txt_put(ROW_INPUT, PC98_BODY_L + i, (uint8_t)end[i], TA_GREEN);
        getch();
#endif
    }
    txt_fini();
    return 0;
}
