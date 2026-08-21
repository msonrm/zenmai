/* Zenmai PS1 — 日本語版 Zork(段階 3+4)。MojoZork + 訳の層 + かな→コマンド変換。
 *
 * 出力: translate.c(walkthrough 709 行を JS と全一致で検証済み)+ 実行時ルビ。
 * 入力: かな T9(ゴールデン 35 件)→ cmd.c(コーパス 3,907 件を JS と全一致で検証済み)。
 * 対話プロトコル(断り書き・聞き返し・別案トライアル)は web/main.js と同形。
 */
#include <stdint.h>
#include "render.h"
#include "input.h"
#include "translate.h"
#include "cmd.h"
#include "cmd_data.h"

#define main mojozork_main_unused
#include "vendor/mojozork.c"
#undef main

extern const uint8_t _binary_story_bin_start[];
extern const uint8_t _binary_story_bin_end[];
extern char __bss_start[], __bss_end[];

static uint8_t story_ram[90 * 1024];   /* z3 は 84.8KB。スタック余地を確保 */
static ZMachineState zm;
static char statusbuf[49];

/* ---- ステータス行(部屋名は訳す) ---- */
enum { STATUS_Y = 24, STATUS_H = 24 };
#define PANEL 0x0863
static uint16_t sbar[STATUS_H][W];

static void draw_status(void)
{
    fill_rows(sbar, 0, STATUS_H, PANEL);
    int name_end = 0;
    while (statusbuf[name_end] &&
           !(statusbuf[name_end] == ' ' && statusbuf[name_end + 1] == ' '))
        name_end++;
    uint16_t jname[64];
    int jn = tr_word_str(statusbuf, name_end, jname, 64);
    int x = MARGIN;
    for (int i = 0; i < jn; i++) {
        draw24(sbar, x, 0, jname[i], INK);
        x += glyph_w(jname[i]);
    }
    int re = name_end;
    while (statusbuf[re] == ' ') re++;
    int rl = 0;
    while (statusbuf[re + rl]) rl++;
    while (rl > 0 && statusbuf[re + rl - 1] == ' ') rl--;
    int rw = 0;
    for (int i = 0; i < rl; i++) rw += glyph_w((uint16_t)(unsigned char)statusbuf[re + i]);
    int rx = W - MARGIN - rw;
    for (int i = 0; i < rl; i++) {
        uint16_t ch = (uint16_t)(unsigned char)statusbuf[re + i];
        draw24(sbar, rx, 0, ch, INK);
        rx += glyph_w(ch);
    }
    gp0_upload(0, STATUS_Y, W, STATUS_H, sbar[0]);
}

/* ---- VM 側(MojoZork 埋め込み) ---- */
enum { OBUF_MAX = 12288, CMD_MAX = 36 };   /* RAM 上限に合わせて縮小 */
static char obuf[OBUF_MAX];
static int olen;

static void zm_writestr(const char *str, const uintptr slen)
{
    for (uintptr i = 0; i < slen && olen < OBUF_MAX - 1; i++)
        obuf[olen++] = str[i];
}

__attribute__((noreturn)) static void zm_die(const char *fmt, ...)
{
    (void)fmt;
    gp0_fill(0, 0, W, 16, 0xFF);
    for (;;) { }
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

static void zm_save(void) { doBranch(0); }
static void zm_restore(void) { doBranch(0); }

static void run_until_read(void)
{
    GState->step_completed = 0;
    while (!GState->step_completed && !GState->quit)
        runInstruction();
}

static void vm_init(void)
{
    const uint32 len = (uint32)(_binary_story_bin_end - _binary_story_bin_start);
    memcpy(story_ram, _binary_story_bin_start, len);
    GState = &zm;
    GState->die = zm_die;
    GState->writestr = zm_writestr;
    initStory(0, story_ram, len);
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

/* VM 出力を行ごとに訳して巻物へ */
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
            if (blank_pending) { hist_blank(); blank_pending = 0; }
            int r = tr_line(line, ja, 2048);
            if (r >= 0) {
                draw_plain(ja, r, INK);          /* 描画器がルビを振る */
            } else {
                for (int k = 0; k < n; k++)
                    en16[k] = (uint16_t)(unsigned char)line[k];
                draw_plain(en16, n, INK);
            }
            any = 1;
        }
        n = 0;
    }
    olen = 0;
}

/* 「You can't see any X here!」= 手数を消費しない空振り(別案トライアル用) */
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

/* ---- 入力(かな T9)---- */
static GpMachine gm;
static uint16_t comp[CMD_MAX];
static int clen, caret;

static void comp_insert(const uint16_t *t, int tn, int replace)
{
    int del = replace > caret ? caret : replace;
    for (int i = caret - del; i + del < clen; i++)
        comp[i] = comp[i + del];
    clen -= del;
    caret -= del;
    if (clen + tn > CMD_MAX)
        tn = CMD_MAX - clen;
    for (int i = clen - 1; i >= caret; i--)
        comp[i + tn] = comp[i];
    for (int i = 0; i < tn; i++)
        comp[caret + i] = t[i];
    clen += tn;
    caret += tn;
}

static void comp_bs(void)
{
    if (!caret) return;
    for (int i = caret - 1; i + 1 < clen; i++)
        comp[i] = comp[i + 1];
    clen--;
    caret--;
}

static void put_frag16(uint16_t *dst, int *o, int max, int fi)
{
    for (int i = 0; i < cm_frags[fi].len && *o < max; i++)
        dst[(*o)++] = cm_jpool[cm_frags[fi].off + i];
}

__attribute__((section(".text.start"), noreturn)) void _start(void)
{
    for (char *p = __bss_start; p < __bss_end; p++)
        *p = 0;
    gpu_init();
    gp0_fill(0, 0, W, H, 0x0F1214);
    render_init();
    jp_text_init();                    /* ルビつき描画器 */
    body_top = STATUS_Y + STATUS_H + 8;
    body_h = CMD_Y - 8 - body_top;

    vm_init();
    run_until_read();
    render_output();
    view_bottom();
    render_window();
    draw_status();
    draw_strip(0, 0, 0, 0, 0);
    GP1 = 0x03000000;

    int fields = 0, rt_hold = 0, dirty = 1;
    int ls_prev = 0, ls_rep = 0;
    int pending_verb = -1;
    uint16_t prev_rowchar = 0;
    gp_init(&gm);
    clen = 0;
    caret = 0;
    pad_prev = 0;
    pad_try_analog();
    for (;;) {
        wait_fields(1);
        fields++;
        uint8_t axes[4];
        int p = pad_read_ex(axes);
        if (p < 0) { pad_prev = 0; continue; }
        int edge = p & ~pad_prev;
        pad_prev = p;

        int dir = (p & BTN_LEFT) ? 1 : (p & BTN_UP) ? 2 : (p & BTN_RIGHT) ? 3
                : (p & BTN_DOWN) ? 4 : 0;
        int row = dir + ((p & BTN_L1) ? 5 : 0);
        int vowel = (p & BTN_R1) ? 0 : (p & BTN_SQ) ? 1 : (p & BTN_TRI) ? 2
                  : (p & BTN_CIR) ? 3 : (p & BTN_X) ? 4 : -1;
        int cc = !!(p & BTN_LEFT) + !!(p & BTN_UP) + !!(p & BTN_RIGHT)
               + !!(p & BTN_DOWN) + !!(p & BTN_L1);
        GpFrame f = { fields * 17, row, vowel, vowel >= 0,
                      !!(p & BTN_L2), !!(p & BTN_R2), cc };
        GpAction a[4];
        int an = gp_step(&gm, 0 /* japanese */, &f, a);
        for (int i = 0; i < an; i++) {
            if (a[i].type == GPA_KANA) {
                comp_insert(a[i].text, a[i].tlen, a[i].replace);
            } else {                   /* GPA_YOUON(PS1 適応: 対象外の っ は捨てる) */
                uint16_t o_[2];
                int rep, l = gp_resolve_youon(comp, caret, o_, &rep);
                if (l && rep > 0) comp_insert(o_, l, rep);
            }
            dirty = 1;
        }
        gp_sync_prev(&gm, &f);

        /* 右スティック = GIME 由来のフリック(↑濁点 / ←BS / →ー / ↓確定) */
        int rs = pad_rstick_flick(axes);
        if (rs)
            gp_break_rt_cycle(&gm);    /* スティック入力で ん→を 循環は切れる(GIME 準拠) */

        if ((edge & BTN_SELECT) || rs == 2) {   /* Select / 右スティック↑ = 濁点トグル */
            uint16_t o_[2];
            int rep, l = gp_resolve_dakuten(comp, caret, o_, &rep);
            if (l) { comp_insert(o_, l, rep); dirty = 1; }
        }
        if (p & BTN_R2) {              /* R2 長押し = BS 1 回 */
            rt_hold++;
            if (rt_hold == 24) {
                gp_consume_rt(&gm);
                if (caret) { comp_bs(); gm.eagerSet = 0; dirty = 1; }
            }
        } else {
            rt_hold = 0;
        }
        if (rs == 1 && caret) {        /* 右スティック← = BS 1 回 */
            comp_bs();
            gm.eagerSet = 0;
            dirty = 1;
        }
        if (rs == 3) {                 /* 右スティック→ = 長音「ー」 */
            uint16_t ch = 0x30FC;
            comp_insert(&ch, 1, 0);
            dirty = 1;
        }
        if ((edge & BTN_R3) && clen) { /* R3 押込み = 入力行クリア(GIME の RS 押込み=キャンセル) */
            clen = 0;
            caret = 0;
            gm.eagerSet = 0;
            dirty = 1;
        }
        if (((edge & (BTN_START | BTN_L3)) || rs == 4) && clen) {
            /* 遡り中の確定は、まず下端へ跳んでひと呼吸置く */
            if (view_px < hist_total() - body_h) {
                view_bottom();
                render_window();
                wait_fields(18);
            }
            static CmdRes cr;
            static uint16_t typed[CMD_MAX];
            int typed_n = clen;
            for (int i = 0; i < clen; i++) typed[i] = comp[i];
            cmd_run(comp, clen, pending_verb, &cr);
            draw_echo(typed, typed_n);           /* 打った通りを反響 */
            clen = 0;
            caret = 0;
            gm.eagerSet = 0;
            dirty = 1;
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
                    int anyw = 0;
                    for (int i = 0; i < cr.unknown_n; i++) {
                        if (cr.unknown_lens[i] <= 1) continue;
                        if (anyw) put_frag16(msg, &ml, 256, 15);   /* ・ */
                        put_frag16(msg, &ml, 256, 0);              /* 「 */
                        for (int t = 0; t < cr.unknown_lens[i] && ml < 250; t++)
                            msg[ml++] = cr.unknown[i][t];
                        if (ml < 254) msg[ml++] = 0x300D;          /* 」 */
                        anyw = 1;
                    }
                    if (anyw) {
                        put_frag16(msg, &ml, 256, 12);   /* は知らない言葉…） */
                        draw_plain(msg, ml, INK);
                        goto after_msg;
                    }
                    ml = 0;
                    put_frag16(msg, &ml, 256, 11);       /* （読み取れなかった） */
                }
                if (cr.note_len)
                    put_frag16(msg, &ml, 256, 14);       /* ） */
                draw_plain(msg, ml, INK);
after_msg:
                scroll_new();
                continue;
            }
            if (cr.needs_object) {
                pending_verb = cr.verb_idx;
                draw_plain(cr.ask, cr.ask_len, INK);
                scroll_new();
                continue;
            }
            pending_verb = -1;
            if (cr.echo_word_len)
                tr_set_echo16(cr.echo_word, cr.echo_word_len);
            else
                tr_set_echo16(typed, typed_n);
            feed_cmd(cr.command);
            int ai = 0;
            for (;;) {
                run_until_read();
                if (GState->quit)
                    break;
                if (cr.alts_n && not_here() && ai < cr.alts_n) {
                    olen = 0;                            /* 空振りは映さず次の候補 */
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
                /* 全部外れ: 打った言い方で断る */
                olen = 0;
                static uint16_t msg[64];
                int ml = 0;
                for (int i = 0; i < cr.obj_disp_len && ml < 40; i++)
                    msg[ml++] = cr.obj_disp[i];
                put_frag16(msg, &ml, 64, 17);            /* など、ここには見当たらない。 */
                draw_plain(msg, ml, INK);
            } else {
                render_output();
            }
            draw_status();
            scroll_new();
            if (GState->quit)
                break;
        }
        /* L2+↑↓ / 左スティック縦 = 履歴スクロール */
        if (fields % 3 == 0) {
            int up = (clen == 0 && (p & BTN_L2) && (p & BTN_UP)) || axes[3] < 0x40;
            int dn = (clen == 0 && (p & BTN_L2) && (p & BTN_DOWN)) || axes[3] > 0xC0;
            if (up) view_scroll(-SCROLL_STEP);
            if (dn) view_scroll(SCROLL_STEP);
        }
        /* 左スティック横 = キャレット移動 */
        {
            int d = axes[2] < 0x40 ? -1 : axes[2] > 0xC0 ? 1 : 0;
            int fire = 0;
            if (d != ls_prev) { ls_prev = d; ls_rep = 0; fire = d != 0; }
            else if (d && ++ls_rep >= 6) { ls_rep = 0; fire = 1; }
            if (fire) {
                int nc = caret + d;
                if (nc >= 0 && nc <= clen && nc != caret) {
                    caret = nc;
                    gm.eagerSet = 0;
                    dirty = 1;
                }
            }
        }
        uint16_t rowchar = gp_row_char(row);
        if (dirty || rowchar != prev_rowchar) {
            draw_strip(comp, clen, caret, &rowchar, 1);
            prev_rowchar = rowchar;
            dirty = 0;
        }
    }
    for (;;) { }
}
