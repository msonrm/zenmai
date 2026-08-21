/* Zenmai PS1 — 英語版 Zork(段階 2)。MojoZork + 描画層 + 英語 T9 入力。
 *
 * 埋め込みは multizorkd 方式(#include + opcode 表差し替え + step_completed)。
 * story(z3)は objcopy で焼き込み、RAM へコピーして走らせる。
 * 入力: T9(stepEnglish・ゴールデン済み)。Select=空白 / Start=確定 / R2 長押し=BS。
 */
#include <stdint.h>
#include "render.h"
#include "input.h"

#define main mojozork_main_unused
#include "vendor/mojozork.c"
#undef main

extern const uint8_t _binary_story_bin_start[];
extern const uint8_t _binary_story_bin_end[];
extern char __bss_start[], __bss_end[];

static uint8_t story_ram[128 * 1024];
static ZMachineState zm;
static char statusbuf[49];             /* 48 桁 + NUL(半角 48 字 = 576px = 本文幅) */

/* ---- ステータス行(最上部セーフエリア内 y=24..48) ---- */
enum { STATUS_Y = 24, STATUS_H = 24 };
#define PANEL 0x0863                   /* web ダークテーマの --panel を RGB555 化 */
static uint16_t sbar[STATUS_H][W];

static void draw_status(void)
{
    fill_rows(sbar, 0, STATUS_H, PANEL);
    int x = MARGIN;
    for (int i = 0; i < 48 && statusbuf[i]; i++) {
        uint16_t ch = (uint16_t)(unsigned char)statusbuf[i];
        draw24(sbar, x, 0, ch, INK);
        x += glyph_w(ch);
    }
    gp0_upload(0, STATUS_Y, W, STATUS_H, sbar[0]);
}

enum { OBUF_MAX = 16384, CMD_MAX = 36 };
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
    gp0_fill(0, 0, W, 16, 0xFF);       /* 上端に赤帯 = VM 死亡の目印 */
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

static void zm_save(void) { doBranch(0); }      /* 未対応 → 失敗として分岐 */
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
    GState->story[1] &= ~(1 << 4);     /* ステータス行あり、とゲームに教える */
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

/* VM 出力(ASCII・改行区切り)を巻物へ。">" だけの行(プロンプト)は捨てる */
static void render_output(void)
{
    /* Zork I の最長段落(ちらし・碑文)でも収まる幅。96 だと "the most am" で切れた */
    static uint16_t line[1024];
    int n = 0, blank_pending = 0, any = 0;
    for (int i = 0; i <= olen; i++) {
        char c = i < olen ? obuf[i] : '\n';
        if (c != '\n') {
            if (n < 1024) line[n++] = (uint16_t)(unsigned char)c;
            continue;
        }
        int only_prompt = 1;
        for (int k = 0; k < n; k++)
            if (line[k] != '>' && line[k] != ' ') { only_prompt = 0; break; }
        if (n == 0 || only_prompt) {
            if (n == 0 && any)
                blank_pending = 1;     /* 空行は次の実行行の前でまとめて 1 つ */
        } else {
            if (blank_pending) { hist_blank(); blank_pending = 0; }
            draw_plain(line, n, INK);
            any = 1;
        }
        n = 0;
    }
    olen = 0;
}

/* T9 行の表示ラベル(数字より文字群のほうが引ける。実機フィードバック 2026-08-19) */
static const char *ROW_LABEL[10] =
    {"1", "ABC", "DEF", "GHI", "JKL", "MNO", "PQRS", "TUV", "WXYZ", "0"};

static void row_label(int row, uint16_t out[4], int *n)
{
    const char *s = (row >= 0 && row < 10) ? ROW_LABEL[row] : "1";
    *n = 0;
    while (s[*n] && *n < 4) { out[*n] = (uint16_t)s[*n]; (*n)++; }
}

static GpMachine gm;
static uint16_t comp[CMD_MAX];
static int clen, caret;

/* キャレット位置へ挿入(replace はキャレット直前から消す) */
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

__attribute__((section(".text.start"), noreturn)) void _start(void)
{
    for (char *p = __bss_start; p < __bss_end; p++)   /* PS-EXE は .bss を零化しない */
        *p = 0;
    gpu_init();
    gp0_fill(0, 0, W, H, 0x0F1214);
    render_init();
    body_top = STATUS_Y + STATUS_H + 8;            /* ステータス行のぶん本文窓を下げる */
    body_h = CMD_Y - 8 - body_top;

    vm_init();
    run_until_read();
    render_output();
    view_bottom();
    render_window();
    draw_status();
    { uint16_t ind[4]; int in_; row_label(0, ind, &in_); draw_strip(0, 0, 0, ind, in_); }
    GP1 = 0x03000000;

    int fields = 0, rt_hold = 0, dirty = 1, prev_row = -1;
    int ls_prev = 0, ls_rep = 0;
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
        int an = gp_step(&gm, 1 /* english */, &f, a);
        for (int i = 0; i < an; i++)
            if (a[i].type == GPA_KANA) { comp_insert(a[i].text, a[i].tlen, a[i].replace); dirty = 1; }
        gp_sync_prev(&gm, &f);

        /* 右スティック = GIME 由来のフリック(↓空白 / ←BS / ↓確定は日本語版のみ) */
        int rs = pad_rstick_flick(axes);
        if (rs)
            gp_break_rt_cycle(&gm);    /* スティック入力で RT 循環は切れる(GIME 準拠) */

        if ((edge & BTN_SELECT) || rs == 4) {   /* Select / 右スティック↓ = 空白 */
            uint16_t sp = ' ';
            comp_insert(&sp, 1, 0);
            gm.engSmartCaps = 0;       /* 空白で SmartCaps は解ける(engClearSmartCaps 相当) */
            gm.engCapsLock = 0;
            dirty = 1;
        }
        if (p & BTN_R2) {              /* R2 長押し = BS 1 回だけ(タップは "0") */
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
        if ((edge & BTN_R3) && clen) { /* R3 押込み = 入力行クリア */
            clen = 0;
            caret = 0;
            gm.eagerSet = 0;
            dirty = 1;
        }
        if ((edge & (BTN_START | BTN_L3)) && clen) {
            /* 遡り中の確定は、まず下端へ跳んでひと呼吸置く */
            if (view_px < hist_total() - body_h) {
                view_bottom();
                render_window();
                wait_fields(18);
            }
            char cmd[CMD_MAX + 1];
            for (int i = 0; i < clen; i++)
                cmd[i] = (char)comp[i];
            cmd[clen] = '\0';
            draw_echo(comp, clen);
            feed_cmd(cmd);
            run_until_read();
            render_output();
            draw_status();
            clen = 0;
            caret = 0;
            gm.eagerSet = 0;
            scroll_new();
            dirty = 1;
            if (GState->quit)
                break;
        }
        /* L2+↑↓(デジタル)/ 左スティック縦(アナログ)= 履歴スクロール */
        if (fields % 3 == 0) {
            int up = (clen == 0 && (p & BTN_L2) && (p & BTN_UP)) || axes[3] < 0x40;
            int dn = (clen == 0 && (p & BTN_L2) && (p & BTN_DOWN)) || axes[3] > 0xC0;
            if (up) view_scroll(-SCROLL_STEP);
            if (dn) view_scroll(SCROLL_STEP);
        }
        /* 左スティック横 = キャレット移動(エッジ + 6 フィールドごとリピート) */
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
        if (dirty || row != prev_row) {
            uint16_t ind[4];
            int in_;
            row_label(row, ind, &in_);
            draw_strip(comp, clen, caret, ind, in_);
            prev_row = row;
            dirty = 0;
        }
    }
    for (;;) { }
}
