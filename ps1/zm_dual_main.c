/* Zenmai PS1 — 統合版 Zork(英語 / 日本語)。起動時に言語を選ぶ。
 *
 * Z-machine(MojoZork)・story・入出力バッファは 1 つを共有し、
 * ステータス行・出力描画・入力ループだけを言語で分岐する。
 * story が同一なので、将来のセーブデータは言語に依存しない
 * (英語で保存 → 日本語で再開、が Z-machine 状態の受け渡しだけで成立する)。
 *
 * 英語: T9 入力(zm_main.c と同じ)。日本語: かな→cmd.c(zm_ja_main.c と同じ)。
 */
#include <stdint.h>
#include "render.h"
#include "input.h"
#include "translate.h"
#include "cmd.h"
#include "cmd_data.h"
#include "card.h"
#include "ui_data.h"

#define main mojozork_main_unused
#include "vendor/mojozork.c"
#undef main

extern const uint8_t _binary_story_bin_start[];
extern const uint8_t _binary_story_bin_end[];
extern char __bss_start[], __bss_end[];

static uint8_t story_ram[90 * 1024];   /* z3 は 84.8KB。スタック余地を確保 */
static ZMachineState zm;
static char statusbuf[49];
static int lang_en;                    /* 1 = ENGLISH / 0 = にほんご */

/* ---- ステータス行(日本語では部屋名を訳す) ---- */
enum { STATUS_Y = 24, STATUS_H = 24 };
#define PANEL 0x0863
static uint16_t sbar[STATUS_H][W];

static void draw_status(void)
{
    fill_rows(sbar, 0, STATUS_H, PANEL);
    if (lang_en) {
        int x = MARGIN;
        for (int i = 0; i < 48 && statusbuf[i]; i++) {
            uint16_t ch = (uint16_t)(unsigned char)statusbuf[i];
            draw24(sbar, x, 0, ch, INK);
            x += glyph_w(ch);
        }
        gp0_upload(0, STATUS_Y, W, STATUS_H, sbar[0]);
        return;
    }
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
enum { OBUF_MAX = 12288, CMD_MAX = 36 };
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

/* ---- セーブ / ロード ----
 *
 * ★保存するのは動的メモリ・スタック・bp・PC の 4 つだけ。訳も入力方式も**状態ではない**
 *   ので、英語で保存して日本語で再開しても、そのまま続く。
 *
 * 動的メモリ 11,282 バイトは 1 ブロック(8KB)に生では入らないので、初期イメージとの
 * XOR を RLE で畳む(Quetzal の CMem と同じ考え)。本体が story の原本を持っているから、
 * 比べる相手はタダで手に入る。
 */
static uint8_t savebuf[CARD_DATA_MAX];

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
            out[o++] = 0;                          /* 0 の後ろは「同じが続く数-1」 */
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
    while (o < len) { out[o] = init[o]; o++; }     /* 残りは初期値のまま */
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
    const int n = cmem_pack(GState->story, _binary_story_bin_start, dyn, out + o, outmax - o);
    return n < 0 ? -1 : o + n;
}

static int unpack_state(const uint8_t *in, int len)
{
    const int nmax = (int) (sizeof GState->stack / sizeof GState->stack[0]);
    if (len < SAVE_HDR) return 0;
    if (in[0] != 'Z' || in[1] != 'N' || in[2] != 'M' || in[3] != '1') return 0;
    const int dyn = in[4] | (in[5] << 8);
    const int nstack = in[6] | (in[7] << 8);
    /* ★書き換える前に全部確かめる。途中で諦めると壊れた状態で続いてしまう */
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
    cmem_unpack(in + o, len - o, _binary_story_bin_start, GState->story, dyn);
    GState->pc = GState->story + pc;
    GState->logical_pc = pc;
    return 1;
}

static void zm_save(void)
{
    /* ★分岐を先に解決してから状態を採る。restore は「save が成功した直後」へ戻るので、
       保存する PC は**分岐を通った後**でなければならない。失敗したら巻き戻して偽で分岐する。 */
    const uint8 *pc_before = GState->pc;
    doBranch(1);
    const int n = pack_state(savebuf, sizeof savebuf);
    if (n > 0 && card_save(savebuf, n)) return;
    GState->pc = pc_before;
    doBranch(0);
}

static void zm_restore(void)
{
    const int n = card_load(savebuf, sizeof savebuf);
    /* 成功すると PC は保存時のもの(= save の分岐先)に置き換わるので、
       この命令の分岐は実行しない —— Z-machine の仕様どおり
       (画面には save 側の「よし。」が出る)。 */
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

/* VM 出力を行ごとに巻物へ(日本語なら訳す)。">" だけの行は捨てる */
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
            int r = lang_en ? -1 : tr_line(line, ja, 2048);
            if (r >= 0) {
                draw_plain(ja, r, INK);
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

/* ---- 入力(共有バッファ) ---- */
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

/* T9 行の表示ラベル(英語) */
static const char *ROW_LABEL[10] =
    {"1", "ABC", "DEF", "GHI", "JKL", "MNO", "PQRS", "TUV", "WXYZ", "0"};

static void row_label(int row, uint16_t out[4], int *n)
{
    const char *s = (row >= 0 && row < 10) ? ROW_LABEL[row] : "1";
    *n = 0;
    while (s[*n] && *n < 4) { out[*n] = (uint16_t)s[*n]; (*n)++; }
}

/* ---- 言語選択メニュー ---- */

static void menu_line(int y, const uint16_t *s, int n, int selected)
{
    fill_rows(sbar, 0, STATUS_H, 0x0442 /* BG */);
    int w = 0;
    for (int i = 0; i < n; i++) w += glyph_w(s[i]);
    int x = (W - w) / 2;
    if (selected)
        draw24(sbar, x - 40, 0, 0xFF1E, ACCENT);   /* ＞ */
    for (int i = 0; i < n; i++) {
        draw24(sbar, x, 0, s[i], selected ? ACCENT : INK);
        x += glyph_w(s[i]);
    }
    gp0_upload(0, y, W, STATUS_H, sbar[0]);
}

static int lang_menu(void)             /* 0 = にほんご / 1 = ENGLISH */
{
    static const uint16_t title[] = {'Z','E','N','M','A','I',' ',' ','Z','O','R','K',' ','I'};
    static const uint16_t o_ja[] = {0x306B, 0x307B, 0x3093, 0x3054};   /* にほんご */
    static const uint16_t o_en[] = {'E','N','G','L','I','S','H'};
    enum { Y_TITLE = 168, Y_JA = 240, Y_EN = 280 };
    int sel = 0;
    menu_line(Y_TITLE, title, 14, 0);
    menu_line(Y_JA, o_ja, 4, sel == 0);
    menu_line(Y_EN, o_en, 7, sel == 1);
    GP1 = 0x03000000;                  /* 表示オン(メニューが最初の画面) */
    int prev = 0;
    for (;;) {
        wait_fields(1);
        int p = pad_read();
        if (p < 0) { prev = 0; continue; }
        int edge = p & ~prev;
        prev = p;
        if (edge & (BTN_UP | BTN_DOWN)) {
            sel ^= 1;
            menu_line(Y_JA, o_ja, 4, sel == 0);
            menu_line(Y_EN, o_en, 7, sel == 1);
        }
        if (edge & (BTN_START | BTN_CIR | BTN_X))
            return sel;
    }
}

/* ---- オプション画面 ----
 *
 * ★入口は「コマンドを打っていないときの Start」。Start は入力中は確定なので、
 *   空のときだけが空いている(実測して確かめた)。Select は濁点 / 空白で埋まっている。
 * ★ここに出る文字列は全部**こちらのもの**で、原作の文は 1 つも出ない。だから訳の表は
 *   通さず完成行で持つ(`gen_ui.py`)。「その文字列は誰のものか」がそのまま設計になる。
 * ★**本文とは別の画面で完結させ、背景色から変える**。ここはゲームの外側なので、
 *   物語の紙面にシステムの文字列を混ぜない —— 一度は履歴へ流す形で作ったが、
 *   「専用画面で完結し、閉じたら本文へ戻る」が正しい姿だった(実機の指摘で作り直した)。
 * ★**画面に出す字は同梱フォントに入っていなければならない**。glyphs.h は「使う字だけ」
 *   なので、gen_ui.py が `ui_chars.txt` を書き出し gen_data.py がそれを読む。
 *   繋ぐ前は ─ ○ ォ 企 典 坂 の 6 字が**無いまま空白で描かれる**ところだった。
 */
#define OPT_BG24 0x3A1E10              /* GPU フィル(0xBBGGRR) = 濃い藍。本文は 0x0F1214 */
#define OPT_BG   0x1C62                /* 同じ色の RGB555(バッファ塗り用) */
#define OPT_TEXT 0x4A52                /* ライセンス全文(控えめ) */

enum { OPT_LICENSE = 0, OPT_CLOSE, OPT_N };
enum { OPTM_MENU = 0, OPTM_LICENSE };
enum { OPT_Y_TITLE = 152, OPT_Y_ITEM = 248, OPT_Y_STEP = 40, OPT_Y_HINT = 400 };
enum { LIC_Y_TITLE = 24, LIC_Y_TOP = 72, LIC_ROW_H = 24, LIC_ROWS = 13 };

static int opt_open, opt_sel, opt_mode, lic_top, lic_count;
static short lic_idx[LIC_N];

/* オプション画面の 1 行。center=0 で左揃え(ライセンス本文)、1 で中央(見出し・項目)。 */
static void opt_row(int y, const uint16_t *s, int n, uint16_t color, int center, int mark)
{
    fill_rows(sbar, 0, STATUS_H, OPT_BG);
    int w = 0;
    for (int i = 0; i < n; i++)
        w += glyph_w(s[i]);
    int x = center ? (W - w) / 2 : MARGIN;
    if (mark)
        draw24(sbar, x - 40, 0, 0xFF1E, ACCENT);   /* ＞ */
    for (int i = 0; i < n; i++) {
        if (x + glyph_w(s[i]) > W - MARGIN)
            break;
        draw24(sbar, x, 0, s[i], color);
        x += glyph_w(s[i]);
    }
    gp0_upload(0, y, W, STATUS_H, sbar[0]);
}

static void opt_pick(int y, const uint16_t *ja, int jn, const uint16_t *en, int en_n, int sel)
{
    opt_row(y, lang_en ? en : ja, lang_en ? en_n : jn, sel ? ACCENT : INK, 1, sel);
}

static void options_draw(int sel)
{
    gp0_fill(0, 0, W, H, OPT_BG24);
    opt_pick(OPT_Y_TITLE, UI_TITLE_JA, UI_TITLE_JA_N, UI_TITLE_EN, UI_TITLE_EN_N, 0);
    opt_pick(OPT_Y_ITEM, UI_LICENSE_JA, UI_LICENSE_JA_N,
             UI_LICENSE_EN, UI_LICENSE_EN_N, sel == OPT_LICENSE);
    opt_pick(OPT_Y_ITEM + OPT_Y_STEP, UI_CLOSE_JA, UI_CLOSE_JA_N,
             UI_CLOSE_EN, UI_CLOSE_EN_N, sel == OPT_CLOSE);
    opt_pick(OPT_Y_HINT, UI_HINT_JA, UI_HINT_JA_N, UI_HINT_EN, UI_HINT_EN_N, 0);
}

/* 表示する行だけを集める(言語で出し分けるので番号が飛ぶ) */
static void license_index(void)
{
    const int want = lang_en ? 2 : 1;
    lic_count = 0;
    for (int i = 0; i < LIC_N; i++)
        if (!LIC_LINES[i].lang || LIC_LINES[i].lang == want)
            lic_idx[lic_count++] = (short)i;
}

static void license_draw(void)
{
    gp0_fill(0, 0, W, H, OPT_BG24);
    opt_pick(LIC_Y_TITLE, UI_LICENSE_JA, UI_LICENSE_JA_N,
             UI_LICENSE_EN, UI_LICENSE_EN_N, 0);
    for (int r = 0; r < LIC_ROWS; r++) {
        const int i = lic_top + r;
        if (i >= lic_count)
            break;
        const LicLine *l = &LIC_LINES[lic_idx[i]];
        opt_row(LIC_Y_TOP + r * LIC_ROW_H, LIC_POOL + l->off, l->len,
                l->dim ? OPT_TEXT : INK, 0, 0);
    }
    opt_pick(OPT_Y_HINT, UI_SCROLL_JA, UI_SCROLL_JA_N, UI_SCROLL_EN, UI_SCROLL_EN_N, 0);
}

/* ★オプション画面は**別ループを回さない**。開いている間の 1 フレーム分をここで処理し、
   パッドを読むのは対話ループの 1 箇所だけに保つ([[hechima-dual-path-hazard]])。
   戻り値 0 = オプションを閉じて本文へ戻る。 */
static int options_step(int edge)
{
    if (opt_mode == OPTM_LICENSE) {
        const int maxtop = lic_count > LIC_ROWS ? lic_count - LIC_ROWS : 0;
        int moved = 0;
        if ((edge & BTN_UP) && lic_top > 0) { lic_top--; moved = 1; }
        if ((edge & BTN_DOWN) && lic_top < maxtop) { lic_top++; moved = 1; }
        if (edge & BTN_L1) { lic_top -= LIC_ROWS; if (lic_top < 0) lic_top = 0; moved = 1; }
        if (edge & BTN_R1) { lic_top += LIC_ROWS; if (lic_top > maxtop) lic_top = maxtop; moved = 1; }
        if (moved)
            license_draw();
        if (edge & BTN_START)                /* Start はいつでも「閉じる」(トグル) */
            return 0;
        if (edge & (BTN_X | BTN_CIR)) {      /* 一段戻ってメニューへ */
            opt_mode = OPTM_MENU;
            options_draw(opt_sel);
        }
        return 1;
    }
    if (edge & (BTN_UP | BTN_DOWN)) {
        opt_sel = (opt_sel + (edge & BTN_UP ? OPT_N - 1 : 1)) % OPT_N;
        options_draw(opt_sel);
    }
    if (edge & BTN_CIR) {
        if (opt_sel == OPT_LICENSE) {
            opt_mode = OPTM_LICENSE;
            lic_top = 0;
            license_index();
            license_draw();
            return 1;
        }
        return 0;                            /* とじる */
    }
    if (edge & (BTN_X | BTN_START))
        return 0;
    return 1;
}

static void options_open(void)
{
    opt_open = 1;
    opt_sel = 0;
    opt_mode = OPTM_MENU;
    /* ★全ボタンを「押されている」扱いにしてから開く。そうしないと**開けた Start が
       そのまま閉じるエッジになる**。離せば次のフレームでエッジは自然に消える。 */
    pad_prev = -1;
    options_draw(opt_sel);
}

static void options_close(void)
{
    opt_open = 0;
    gp0_fill(0, 0, W, H, 0x0F1214);      /* オプションの色を消してから本文を描き直す */
    render_window();
}

/* ---- 対話ループ(英語 = zm_main.c と同じ) ---- */

__attribute__((noreturn)) static void interactive_en(void)
{
    int fields = 0, rt_hold = 0, dirty = 1, prev_row = -1;
    int ls_prev = 0, ls_rep = 0;
    gp_init(&gm);
    clen = 0;
    caret = 0;
    /* ★いま押されているものを「押下済み」として引き継ぐ。0 で始めると、**言語メニューを
       抜けた Start がそのまま最初のエッジになり、オプション画面が勝手に開く**
       (実測。開いたまま入力を横取りするので、以降どのボタンも効かなくなっていた)。 */
    pad_prev = pad_read();
    if (pad_prev < 0)
        pad_prev = 0;
    for (;;) {
        wait_fields(1);
        fields++;
        uint8_t axes[4];
        int p = pad_read_ex(axes);
        if (p < 0) { pad_prev = 0; continue; }
        int edge = p & ~pad_prev;
        pad_prev = p;

        if (opt_open) {                 /* オプションを開いている間は入力を横取りする */
            if (!options_step(edge)) {
                options_close();
                dirty = 1;
            }
            continue;
        }

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

        /* 右スティック = GIME 由来のフリック(↓空白 / ←BS) */
        int rs = pad_rstick_flick(axes);
        if (rs)
            gp_break_rt_cycle(&gm);

        if ((edge & BTN_SELECT) || rs == 4) {   /* Select / 右スティック↓ = 空白 */
            uint16_t sp = ' ';
            comp_insert(&sp, 1, 0);
            gm.engSmartCaps = 0;
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
        if ((edge & BTN_START) && !clen) {   /* 打っていないときの Start = オプション */
            options_open();
            continue;
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

/* ---- 対話ループ(日本語 = zm_ja_main.c と同じ) ---- */

__attribute__((noreturn)) static void interactive_ja(void)
{
    int fields = 0, rt_hold = 0, dirty = 1;
    int ls_prev = 0, ls_rep = 0;
    int pending_verb = -1;
    uint16_t prev_rowchar = 0;
    gp_init(&gm);
    clen = 0;
    caret = 0;
    /* ★いま押されているものを「押下済み」として引き継ぐ。0 で始めると、**言語メニューを
       抜けた Start がそのまま最初のエッジになり、オプション画面が勝手に開く**
       (実測。開いたまま入力を横取りするので、以降どのボタンも効かなくなっていた)。 */
    pad_prev = pad_read();
    if (pad_prev < 0)
        pad_prev = 0;
    for (;;) {
        wait_fields(1);
        fields++;
        uint8_t axes[4];
        int p = pad_read_ex(axes);
        if (p < 0) { pad_prev = 0; continue; }
        int edge = p & ~pad_prev;
        pad_prev = p;

        if (opt_open) {                 /* オプションを開いている間は入力を横取りする */
            if (!options_step(edge)) {
                options_close();
                dirty = 1;
            }
            continue;
        }

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
            gp_break_rt_cycle(&gm);

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
        if ((edge & BTN_R3) && clen) { /* R3 押込み = 入力行クリア */
            clen = 0;
            caret = 0;
            gm.eagerSet = 0;
            dirty = 1;
        }
        if ((edge & BTN_START) && !clen) {   /* 打っていないときの Start = オプション */
            options_open();
            continue;
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

__attribute__((section(".text.start"), noreturn)) void _start(void)
{
    for (char *p = __bss_start; p < __bss_end; p++)
        *p = 0;
    gpu_init();
    gp0_fill(0, 0, W, H, 0x0F1214);
    render_init();
    jp_text_init();                    /* 描画器は共通(ASCII 行にはルビ帯が付かない) */
    body_top = STATUS_Y + STATUS_H + 8;
    body_h = CMD_Y - 8 - body_top;

    pad_try_analog();
    lang_en = lang_menu();
    gp0_fill(0, 0, W, H, 0x0F1214);    /* メニューを消す */

    vm_init();
    run_until_read();
    render_output();
    view_bottom();
    render_window();
    draw_status();
    if (lang_en) {
        uint16_t ind[4];
        int in_;
        row_label(0, ind, &in_);
        draw_strip(0, 0, 0, ind, in_);
        interactive_en();
    }
    draw_strip(0, 0, 0, 0, 0);
    interactive_ja();
}
