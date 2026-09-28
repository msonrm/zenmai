/* Zenmai PC-98 版の入口（計画 = docs/pc98-port-plan.md）。
 *
 * ★芯（VM・訳・語彙・セーブ）は session.c で、PS1 / SDL と共有している。
 *   ここに在るのは PC-98 の画面の外枠（状態行・入力欄）と、入力の取り方だけ。
 *   本文は render_pc98.c（render.h の文字列の口）。
 *
 * ★段 2 の画面はまだ仮（標準の 80×25・ふりがな無し）。24 ラスタ × 16 行は段 4。
 *
 * 入力は 2 通り:
 *   ZENMAI              … キーボード（まだ ASCII だけ。英語のコマンドも cmd_run が通す）。ESC でやめる
 *   ZENMAI /S 台本.TXT  … 台本（UTF-8・1 行 1 コマンド）を流し、ZENMAI.LOG に記録する。
 *                         ★同じものをホストでも建てて（build-pc98.sh）記録を突き合わせる（test-pc98.sh）
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "pc98_text.h"
#include "render_pc98.h"
#include "session.h"
#include "translate.h"

#ifndef PC98_HOST
#include <conio.h>
#endif

extern const uint8_t zm_story[];       /* story_pc98.c（build-pc98.sh が zork1.z3 から焼く） */
extern const uint32_t zm_story_len;

enum { ROW_STATUS = 0, ROW_INPUT = 24, CMD_MAX = 36 };
static uint16_t comp[CMD_MAX];
static int clen;

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

/* 台本の 1 行（UTF-8）を comp へ。戻り値 0 = 台本の終わり */
static int script_line(FILE *f)
{
    static char buf[512];
    for (;;) {
        if (!fgets(buf, sizeof buf, f))
            return 0;
        clen = 0;
        const unsigned char *p = (const unsigned char *)buf;
        while (*p && *p != '\n' && *p != '\r' && clen < CMD_MAX) {
            unsigned u = *p++;
            if (u >= 0xE0 && p[0] && p[1]) {
                u = (u & 0x0F) << 12 | (p[0] & 0x3F) << 6 | (p[1] & 0x3F);
                p += 2;
            } else if (u >= 0xC0 && p[0]) {
                u = (u & 0x1F) << 6 | (p[0] & 0x3F);
                p += 1;
            }
            comp[clen++] = (uint16_t)u;
        }
        if (clen && comp[0] != '#')    /* 空行と # の行は飛ばす */
            return 1;
    }
}

static void draw_input(void)
{
    txt_clear(ROW_INPUT, ROW_INPUT, TA_WHITE);
    int col = PC98_BODY_L;
    col += txt_put(ROW_INPUT, col, 0xFF1E, TA_YELLOW);   /* ＞ */
    for (int i = 0; i < clen; i++)
        col += txt_put(ROW_INPUT, col, comp[i], TA_WHITE);
    txt_cursor(ROW_INPUT, col);
}

#ifndef PC98_HOST
/* キーボードから 1 行。戻り値 0 = ESC（やめる） */
static int key_line(void)
{
    clen = 0;
    draw_input();
    for (;;) {
        const int c = getch();
        if (c == 0x1B)
            return 0;
        if (c == '\r' || c == '\n') {
            if (clen) break;
            continue;
        }
        if (c == 0x08) {
            if (clen) clen--;
        } else if (c >= 0x20 && c < 0x7F && clen < CMD_MAX) {
            comp[clen++] = (uint16_t)c;
        }
        draw_input();
    }
    txt_cursor(-1, 0);
    return 1;
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
            if (!key_line()) break;
#endif
        }
        const int no_turn = sess_submit_ja(comp, clen, &pending_verb);
        clen = 0;
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
