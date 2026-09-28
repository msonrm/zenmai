/* Zenmai PC-98 版 —— 段 1: 素の Zork（計画 = docs/pc98-port-plan.md）。
 *
 * ★段 1 の目的は**いちばん大きな危険を先に潰す**こと: MojoZork・訳・語彙が
 *   Open Watcom + DOS/4GW で PC-98 の上で正しく動くか。画面は標準の 80×25 に
 *   訳文を流すだけで、ふりがな・禁則・24 ラスタの行は入れない。
 *
 * ★★**VM とのつなぎ（feed_cmd・render_output・submit_ja・セーブの圧縮）は main.c の写し。**
 *   段 2 で共有ファイルへ抜き出して、この写しを消す。それまでは main.c を直したら
 *   ここも見ること。
 *
 * 入力は 2 通り:
 *   ZENMAI              … キーボード（段 1 は ASCII だけ。英語のコマンドも cmd_run が通す）
 *   ZENMAI /S 台本.TXT  … 台本（UTF-8・1 行 1 コマンド）を流し、ZENMAI.LOG に記録する。
 *                         ★同じものをホストでも建てて（build-pc98.sh）記録を突き合わせる
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "pc98_text.h"
#include "translate.h"
#include "cmd.h"
#include "cmd_data.h"

#define main mojozork_main_unused
#include "vendor/mojozork.c"
#undef main

#ifndef PC98_HOST
#include <conio.h>
#endif

extern const uint8_t zm_story[];       /* story_pc98.c（build-pc98.sh が zork1.z3 から焼く） */
extern const uint32_t zm_story_len;

static uint8_t story_ram[90 * 1024];
static ZMachineState zm;
static char statusbuf[49];

/* ---- 記録（台本のときだけ）---- */
static FILE *logf;

static void log_utf16(const char *head, const uint16_t *s, int n)
{
    if (!logf) return;
    fputs(head, logf);
    for (int i = 0; i < n; i++) {
        const unsigned u = s[i];
        if (u < 0x80) {
            fputc((int)u, logf);
        } else if (u < 0x800) {
            fputc((int)(0xC0 | u >> 6), logf);
            fputc((int)(0x80 | (u & 0x3F)), logf);
        } else {
            fputc((int)(0xE0 | u >> 12), logf);
            fputc((int)(0x80 | (u >> 6 & 0x3F)), logf);
            fputc((int)(0x80 | (u & 0x3F)), logf);
        }
    }
    fputc('\n', logf);
}

/* ---- 画面: 0 行目 = 状態 / 2〜22 行目 = 本文 / 24 行目 = 入力欄 ---- */
enum { ROW_STATUS = 0, BODY_TOP = 2, BODY_BOT = 22, ROW_INPUT = 24,
       COL_L = 2, COL_R = 78 };        /* 本文は [COL_L, COL_R) の 76 桁 */
static int brow = BODY_TOP;            /* 次に書く行 */

static void body_newline(void)
{
    if (brow < BODY_BOT)
        brow++;
    else
        txt_scroll(BODY_TOP, BODY_BOT, TA_WHITE);
}

/* 1 論理行を折り返して流す。★段 1 は禁則なし。ASCII の語だけは割らない */
static void out_line(const uint16_t *s, int n, uint8_t attr)
{
    log_utf16("", s, n);
    int col = COL_L;
    for (int i = 0; i < n; ) {
        const uint16_t c = s[i];
        if (c > ' ' && c < 0x80) {
            int j = i;
            while (j < n && s[j] > ' ' && s[j] < 0x80) j++;
            if (col + (j - i) > COL_R && col > COL_L && j - i <= COL_R - COL_L) {
                body_newline();
                col = COL_L;
            }
        }
        const int w = txt_cells(c);
        if (col + w > COL_R) {
            body_newline();
            col = COL_L;
            if (c == ' ') { i++; continue; }
        }
        col += txt_put(brow, col, c, attr);
        i++;
    }
    body_newline();
}

static void out_blank(void)
{
    if (logf) fputc('\n', logf);
    body_newline();
}

static void out_echo(const uint16_t *s, int n)
{
    uint16_t buf[104];
    int m = 0;
    buf[m++] = 0xFF1E;                 /* ＞ */
    for (int i = 0; i < n && m < 104; i++)
        buf[m++] = s[i];
    out_blank();
    out_line(buf, m, TA_YELLOW);
}

/* ---- 状態行（部屋名は訳す）---- */
static void draw_status(void)
{
    const uint8_t a = TA_CYAN | TA_REV;
    txt_clear(ROW_STATUS, ROW_STATUS, a);
    int name_end = 0;
    while (statusbuf[name_end] &&
           !(statusbuf[name_end] == ' ' && statusbuf[name_end + 1] == ' '))
        name_end++;
    uint16_t name[64];
    const int nn = tr_word_str(statusbuf, name_end, name, 64);
    int col = 2;
    for (int i = 0; i < nn && col < 50; i++)
        col += txt_put(ROW_STATUS, col, name[i], a);
    int re = name_end;
    while (statusbuf[re] == ' ') re++;
    int rl = 0;
    while (statusbuf[re + rl]) rl++;
    while (rl > 0 && statusbuf[re + rl - 1] == ' ') rl--;
    for (int i = 0; i < rl; i++)
        txt_put(ROW_STATUS, TXT_COLS - 2 - rl + i, (uint8_t)statusbuf[re + i], a);
    if (logf) {
        uint16_t sc[48];
        for (int i = 0; i < rl && i < 48; i++) sc[i] = (uint8_t)statusbuf[re + i];
        log_utf16("# ", name, nn);
        log_utf16("# ", sc, rl);
    }
}

/* ======== ここから main.c の写し（段 2 で共有へ抜き出す）======== */

enum { OBUF_MAX = 12288 };
static char obuf[OBUF_MAX];
static int olen;

static void zm_writestr(const char *str, const uintptr slen)
{
    for (uintptr i = 0; i < slen && olen < OBUF_MAX - 1; i++)
        obuf[olen++] = str[i];
}

static void zm_die(const char *fmt, ...)
{
    txt_fini();
    printf("zenmai: VM が止まった (%s)\n", fmt);
    exit(1);
}

static uint8 *pend_input;
static uint8 pend_inputlen;
static uint16 pend_ops[2];

static void zm_read(void)
{
    updateStatusBar();
    uint8 *input = GState->story + GState->operands[0];
    const uint8 inputlen = *(input++);
    pend_input = input;
    pend_inputlen = inputlen;
    pend_ops[0] = GState->operands[0];
    pend_ops[1] = GState->operands[1];
    GState->step_completed = 1;
}

/* ---- セーブ（main.c と同じ書式。置き場はファイル ZENMAI.SAV）---- */
enum { SAVE_MAX = 62 * 128 - 2 };
static uint8_t savebuf[SAVE_MAX];

static int cmem_pack(const uint8_t *cur, const uint8_t *init, int len, uint8_t *out, int outmax)
{
    int o = 0;
    for (int i = 0; i < len; ) {
        uint8_t x = (uint8_t)(cur[i] ^ init[i]);
        if (x) {
            if (o >= outmax) return -1;
            out[o++] = x;
            i++;
        } else {
            int run = 0;
            while (i < len && run < 256 && cur[i] == init[i]) { i++; run++; }
            if (o + 2 > outmax) return -1;
            out[o++] = 0;
            out[o++] = (uint8_t)(run - 1);
        }
    }
    return o;
}

static void cmem_unpack(const uint8_t *in, int inlen, const uint8_t *init, uint8_t *out, int len)
{
    int i = 0, o = 0;
    while (i < inlen && o < len) {
        uint8_t x = in[i++];
        if (x) {
            out[o] = (uint8_t)(init[o] ^ x);
            o++;
        } else if (i < inlen) {
            int run = in[i++] + 1;
            while (run-- > 0 && o < len) { out[o] = init[o]; o++; }
        }
    }
    while (o < len) { out[o] = init[o]; o++; }
}

#define SAVE_HDR 14

static int pack_state(uint8_t *out, int outmax)
{
    const int dyn = GState->header.staticmem_addr;
    const int nstack = (int)(GState->sp - GState->stack);
    const uint32 pc = (uint32) (GState->pc - GState->story);
    if (outmax < SAVE_HDR + nstack * 2) return -1;
    out[0] = 'Z'; out[1] = 'N'; out[2] = 'M'; out[3] = '1';
    out[4] = (uint8_t)(dyn & 0xFF);        out[5] = (uint8_t)(dyn >> 8);
    out[6] = (uint8_t)(nstack & 0xFF);     out[7] = (uint8_t)(nstack >> 8);
    out[8] = (uint8_t)(GState->bp & 0xFF); out[9] = (uint8_t)(GState->bp >> 8);
    out[10] = (uint8_t)(pc & 0xFF);        out[11] = (uint8_t)((pc >> 8) & 0xFF);
    out[12] = (uint8_t)((pc >> 16) & 0xFF); out[13] = (uint8_t)((pc >> 24) & 0xFF);
    int o = SAVE_HDR;
    for (int i = 0; i < nstack; i++) {
        out[o++] = (uint8_t)(GState->stack[i] & 0xFF);
        out[o++] = (uint8_t)(GState->stack[i] >> 8);
    }
    const int n = cmem_pack(GState->story, zm_story, dyn, out + o, outmax - o);
    return n < 0 ? -1 : o + n;
}

static int unpack_state(const uint8_t *in, int len)
{
    const int nmax = (int) (sizeof GState->stack / sizeof GState->stack[0]);
    if (len < SAVE_HDR) return 0;
    if (in[0] != 'Z' || in[1] != 'N' || in[2] != 'M' || in[3] != '1') return 0;
    const int dyn = in[4] | (in[5] << 8);
    const int nstack = in[6] | (in[7] << 8);
    if (dyn != GState->header.staticmem_addr) return 0;
    if (nstack < 0 || nstack > nmax) return 0;
    if (len < SAVE_HDR + nstack * 2) return 0;
    const uint32 pc = (uint32) in[10] | ((uint32) in[11] << 8)
                    | ((uint32) in[12] << 16) | ((uint32) in[13] << 24);
    if (pc >= (uint32) GState->story_len) return 0;
    int o = SAVE_HDR;
    for (int i = 0; i < nstack; i++) {
        GState->stack[i] = (uint16)(in[o] | (in[o + 1] << 8));
        o += 2;
    }
    GState->sp = GState->stack + nstack;
    GState->bp = (uint16)(in[8] | (in[9] << 8));
    cmem_unpack(in + o, len - o, zm_story, GState->story, dyn);
    GState->pc = GState->story + pc;
    GState->logical_pc = pc;
    return 1;
}

static void zm_save(void)
{
    const uint8 *pc_before = GState->pc;
    doBranch(1);
    const int n = pack_state(savebuf, sizeof savebuf);
    if (n > 0) {
        FILE *f = fopen("ZENMAI.SAV", "wb");
        if (f) {
            const int ok = fwrite(savebuf, 1, (size_t)n, f) == (size_t)n;
            if (fclose(f) == 0 && ok) return;
        }
    }
    GState->pc = pc_before;
    doBranch(0);
}

static void zm_restore(void)
{
    FILE *f = fopen("ZENMAI.SAV", "rb");
    int n = -1;
    if (f) {
        n = (int)fread(savebuf, 1, sizeof savebuf, f);
        fclose(f);
    }
    if (n > 0 && unpack_state(savebuf, n)) return;
    doBranch(0);
}

static void run_until_read(void)
{
    GState->step_completed = 0;
    while (!GState->step_completed && !GState->quit)
        runInstruction();
}

static void vm_init(void)
{
    memcpy(story_ram, zm_story, zm_story_len);
    GState = &zm;
    GState->die = zm_die;
    GState->writestr = zm_writestr;
    initStory(0, story_ram, zm_story_len);
    GState->status_bar = statusbuf;
    GState->status_bar_len = sizeof statusbuf;
    GState->status_bar_enabled = 1;
    GState->story[1] &= ~(1 << 4);
    GState->opcodes[181].fn = zm_save;
    GState->opcodes[182].fn = zm_restore;
    GState->opcodes[228].fn = zm_read;
    for (uint8 i = 32; i <= 127; i++) GState->opcodes[i] = GState->opcodes[i % 32];
    for (uint8 i = 144; i <= 175; i++) GState->opcodes[i] = GState->opcodes[128 + (i % 16)];
    for (uint8 i = 192; i <= 223; i++) GState->opcodes[i] = GState->opcodes[i % 32];
}

static void feed_cmd(const char *cmd)
{
    uint8 i = 0;
    for (; cmd[i] && i < pend_inputlen - 1; i++) {
        char c = cmd[i];
        if (c >= 'A' && c <= 'Z')
            c = (char)('a' + (c - 'A'));
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || strchr(" .,!?_#'\"/\\-:()", c))
            ;
        else
            c = ' ';
        pend_input[i] = (uint8)c;
    }
    pend_input[i] = '\0';
    GState->operands[0] = pend_ops[0];
    GState->operands[1] = pend_ops[1];
    GState->operand_count = 2;
    pend_input = 0;
    tokenizeUserInput();
}

/* VM 出力を行ごとに訳して流す。">" だけの行は捨てる */
static void render_output(void)
{
    static char line[1024];
    static uint16_t ja[2048];
    static uint16_t en16[1024];
    int n = 0, blank_pending = 0, any = 0;
    for (int i = 0; i <= olen; i++) {
        char c = i < olen ? obuf[i] : '\n';
        if (c != '\n') {
            if (n < 1023) line[n++] = c;
            continue;
        }
        line[n] = '\0';
        int only_prompt = 1;
        for (int k = 0; k < n; k++)
            if (line[k] != '>' && line[k] != ' ') { only_prompt = 0; break; }
        if (n == 0 || only_prompt) {
            if (n == 0 && any)
                blank_pending = 1;
        } else {
            if (blank_pending) { out_blank(); blank_pending = 0; }
            int r = tr_line(line, ja, 2048);
            if (r >= 0) {
                out_line(ja, r, TA_WHITE);
            } else {
                for (int k = 0; k < n; k++)
                    en16[k] = (uint16_t)(unsigned char)line[k];
                out_line(en16, n, TA_WHITE);
            }
            any = 1;
        }
        n = 0;
    }
    olen = 0;
}

static int not_here(void)
{
    static const char pat[] = "can't see any";
    for (int i = 0; i + (int)sizeof(pat) - 1 <= olen; i++) {
        int ok = 1;
        for (int t = 0; pat[t] && ok; t++) {
            char c = obuf[i + t];
            if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
            ok = c == pat[t];
        }
        if (ok)
            return 1;
    }
    return 0;
}

static void put_frag16(uint16_t *dst, int *o, int max, int fi)
{
    for (int i = 0; i < cm_frags[fi].len && *o < max; i++)
        dst[(*o)++] = cm_jpool[cm_frags[fi].off + i];
}

enum { CMD_MAX = 36 };
static uint16_t comp[CMD_MAX];
static int clen;

static void submit_ja(int *pending_verb)
{
    static CmdRes cr;
    static uint16_t typed[CMD_MAX];
    int typed_n = clen;
    for (int i = 0; i < clen; i++) typed[i] = comp[i];
    cmd_run(comp, clen, *pending_verb, &cr);
    out_echo(typed, typed_n);
    clen = 0;
    if (!cr.has_command) {
        static uint16_t msg[256];
        int ml = 0;
        put_frag16(msg, &ml, 256, 13);           /* （ */
        if (cr.note_len) {
            for (int i = 0; i < cr.note_len && ml < 254; i++) msg[ml++] = cr.note[i];
        } else if (cr.trace == CMD_TR_NEG) {
            ml = 0;
            put_frag16(msg, &ml, 256, 10);       /* （打ち消し…） */
        } else {
            const int decl = cr.trace == CMD_TR_NOCMD;
            int anyw = 0;
            for (int i = 0; i < cr.unknown_n; i++) {
                if (!decl && cr.unknown_lens[i] <= 1) continue;
                if (anyw) put_frag16(msg, &ml, 256, 15);   /* ・ */
                put_frag16(msg, &ml, 256, 0);              /* 「 */
                for (int t = 0; t < cr.unknown_lens[i] && ml < 250; t++)
                    msg[ml++] = cr.unknown[i][t];
                if (ml < 254) msg[ml++] = 0x300D;          /* 」 */
                anyw = 1;
            }
            if (anyw) {
                put_frag16(msg, &ml, 256, 12);   /* は知らない言葉…） */
                out_line(msg, ml, TA_WHITE);
                return;
            }
            ml = 0;
            put_frag16(msg, &ml, 256, 11);       /* （読み取れなかった） */
        }
        if (cr.note_len)
            put_frag16(msg, &ml, 256, 14);       /* ） */
        out_line(msg, ml, TA_WHITE);
        return;
    }
    if (cr.needs_object) {
        *pending_verb = cr.verb_idx;
        out_line(cr.ask, cr.ask_len, TA_WHITE);
        return;
    }
    *pending_verb = -1;
    if (cr.echo_word_len)
        tr_set_echo16(cr.echo_word, cr.echo_word_len);
    else
        tr_set_echo16(typed, typed_n);
    tr_set_said16(cr.said, cr.said_len);
    feed_cmd(cr.command);
    int ai = 0;
    for (;;) {
        run_until_read();
        if (GState->quit)
            break;
        if (cr.alts_n && not_here() && ai < cr.alts_n) {
            olen = 0;
            char alt[96 + 1];
            for (int t = 0; t < cr.alts_lens[ai]; t++) alt[t] = cr.alts[ai][t];
            alt[cr.alts_lens[ai]] = '\0';
            ai++;
            feed_cmd(alt);
            continue;
        }
        break;
    }
    if (cr.alts_n && not_here()) {
        olen = 0;
        static uint16_t msg[64];
        int ml = 0;
        for (int i = 0; i < cr.obj_disp_len && ml < 40; i++)
            msg[ml++] = cr.obj_disp[i];
        put_frag16(msg, &ml, 64, 17);            /* など、ここには見当たらない。 */
        out_line(msg, ml, TA_WHITE);
    } else {
        render_output();
    }
    draw_status();
}

/* ======== 写しここまで ======== */

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
    int col = COL_L;
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
        logf = fopen("ZENMAI.LOG", "wb");

    txt_init();
    vm_init();
    run_until_read();
    render_output();
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
        submit_ja(&pending_verb);
        if (GState->quit) break;
    }

    if (script) {
        fclose(script);
        if (logf) fclose(logf);
        logf = 0;
#ifndef PC98_HOST
        /* ★台本の終わりの印（撮る側がテキスト VRAM の ASCII で待つ）。キーで DOS へ */
        static const char end[] = "[END]";
        txt_clear(ROW_INPUT, ROW_INPUT, TA_WHITE);
        for (int i = 0; end[i]; i++)
            txt_put(ROW_INPUT, COL_L + i, (uint8_t)end[i], TA_GREEN);
        getch();
#endif
    }
    txt_fini();
    return 0;
}
