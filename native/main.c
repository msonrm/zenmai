/* Zenmai — 統合版 Zork(英語 / 日本語)。起動時に言語を選ぶ。
 *
 * ★PS1（build.sh）と SDL2 = PortMaster / デスクトップ（build-sdl.sh）で
 *   **このファイルごと共有する**。分かれるのは入口だけ（末尾の #ifdef ZM_SDL）で、
 *   機械に触る部分は plat.h の後ろにある。
 *
 * Z-machine(MojoZork)・story・入出力バッファは 1 つを共有し、
 * ステータス行・出力描画・入力ループだけを言語で分岐する。
 * story が同一なので、将来のセーブデータは言語に依存しない
 * (英語で保存 → 日本語で再開、が Z-machine 状態の受け渡しだけで成立する)。
 *
 * 英語: T9 入力。日本語: かな→cmd.c。
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
#ifndef ZM_SDL
extern char __bss_start[], __bss_end[];   /* link.ld が置く。SDL 版には無い */
#endif

static uint8_t story_ram[90 * 1024];   /* z3 は 84.8KB。スタック余地を確保 */
static ZMachineState zm;
static char statusbuf[49];
static int lang_en;                    /* 1 = ENGLISH / 0 = 日本語 */

/* ---- ステータス行(日本語では部屋名を訳す) ---- */
enum { STATUS_Y = 24, STATUS_H = 24 };
/* ★状態行の地色。本文(BG = 16,16,8)より**3 段ぶん**持ち上げて 40,40,32 にしてある。
   1 段(24,24,16)では実機でまず見分けが付かず、2 段でもまだ足りなかった ——
   RGB555 の 1 段は 8/255 しかないので、この暗さの帯では 3 段でようやく帯に見える。
   ★本文より**明るい**側で差を付ける（暗い側だと窓の縁と見分けが付かない）。 */
#define PANEL 0x10A5
/* ★本文の外側（言語メニュー・オプション）はこの藍で塗る。
   **地色だけで本文と区別が付く**ので、枠線は引かない（実機の判断） */
#define OPT_BG   0x1C62                /* 濃い藍(RGB555 の 1 本だけ持つ) */
#define OPT_EDGE 0x36B9                /* 板の枠・仕切り(= ACCENT) */
#define OPT_TEXT 0x4A52                /* 札と本文(控えめ) */
static uint16_t sbar[STATUS_H][W];

/* 画面いっぱいを 1 色で塗る。
   ★**GPU のフィル(gp0_fill)は地色に使わない**。字のある行はバッファ転送で描くので、
     塗りと転送が混ざると**地色がほんのわずかに食い違う**（実機で見えた。転送は 15bit を
     そのまま書き、フィルは 24bit を落として書く別経路なので、道が違えば食い違いうる）。
     同じ道で塗れば食い違いようがない —— 色定数も 15bit の 1 本だけになる。 */
static void paint_screen(uint16_t color)
{
    fill_rows(sbar, 0, STATUS_H, color);
    for (int y = 0; y < H; y += STATUS_H)
        gp0_upload(0, y, W, STATUS_H, sbar[0]);
}

/* 状態行。左＝部屋名 / 右＝スコア。
 *
 * ★**空白詰めに頼らない。** Z-machine は 49 字の桁を空白で埋めて右端を作るので、
 *   等幅フォントならそのまま流すだけで揃う。★ところがプロポーショナルにすると
 *   空白も字も細くなり、**スコアが中央寄りへ流れる**（2026-08-30・実機の指摘。
 *   英語面だけ「そのまま流す」経路だったので、英語だけ崩れていた）。
 *   だから左右とも**自分で測って置く**。日本語面は元からこの形だった ——
 *   ★片方だけの経路を残すと、片方だけ壊れる。
 */
static void draw_status(void)
{
    fill_rows(sbar, 0, STATUS_H, PANEL);

    /* 部屋名とスコアの境目は**空白 2 つ**（Z-machine の桁埋め） */
    int name_end = 0;
    while (statusbuf[name_end] &&
           !(statusbuf[name_end] == ' ' && statusbuf[name_end + 1] == ' '))
        name_end++;

    /* 左: 部屋名（日本語面は訳す） */
    uint16_t name[64];
    int nn;
    if (lang_en) {
        nn = name_end < 64 ? name_end : 64;
        for (int i = 0; i < nn; i++)
            name[i] = (uint16_t)(unsigned char)statusbuf[i];
    } else {
        nn = tr_word_str(statusbuf, name_end, name, 64);
    }
    int x = MARGIN;
    for (int i = 0; i < nn; i++) {
        draw24(sbar, STATUS_H, x, 0, name[i], INK);
        x += glyph_w(name[i]);
    }

    /* 右: スコア（前後の空白を落として右寄せ） */
    int re = name_end;
    while (statusbuf[re] == ' ') re++;
    int rl = 0;
    while (statusbuf[re + rl]) rl++;
    while (rl > 0 && statusbuf[re + rl - 1] == ' ') rl--;
    int rw = 0;
    for (int i = 0; i < rl; i++) rw += glyph_w((uint16_t)(unsigned char)statusbuf[re + i]);
    int rx = W - MARGIN - rw;
    if (rx < x + 24)                   /* 名前と重なるなら諦めて右へ寄せきらない */
        rx = x + 24;
    for (int i = 0; i < rl; i++) {
        uint16_t ch = (uint16_t)(unsigned char)statusbuf[re + i];
        draw24(sbar, STATUS_H, rx, 0, ch, INK);
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

/* コマンド欄に収まる幅（欄の左端から、右端の印の箱の手前まで）。
   ★**字数ではなく幅**で止める —— かなは 24px・英字は 12px なので、
     字数で決めるとどちらかで必ず外す（36 字入れるとかなでは欄を突き抜けていた）。 */
enum { COMP_X0 = MARGIN + 24, COMP_W_MAX = W - MARGIN - 24 - 12 - COMP_X0 };

static void comp_insert(const uint16_t *t, int tn, int replace)
{
    int del = replace > caret ? caret : replace;
    for (int i = caret - del; i + del < clen; i++)
        comp[i] = comp[i + del];
    clen -= del;
    caret -= del;
    if (clen + tn > CMD_MAX)
        tn = CMD_MAX - clen;
    {                                  /* 欄からはみ出す分は入れない */
        int w = 0;
        for (int i = 0; i < clen; i++)
            w += glyph_w(comp[i]);
        int keep = 0;
        while (keep < tn && w + glyph_w(t[keep]) <= COMP_W_MAX) {
            w += glyph_w(t[keep]);
            keep++;
        }
        tn = keep;
    }
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

/* ---- 英語面の記号を Zork の辞書に合わせて差し替える ----
 * GP_ENG は GIME 汎用の英語 T9 で、**Zork が要る ',' を持たず**、使わない記号
 * ( ? ) ! @ # - _ ばかり並んでいる。zork1.z3 の辞書 684 語と実挙動を当たった結果:
 *   , = 要る。目的語の並列と呼びかけ("take leaflet, mailbox" → Taken.)
 *   . = 文の区切りだが**後半は捨てられる**("open mailbox. read leaflet" は前半だけ。
 *       つなぐのは then)。要らないが、他に置くものが無いので枠を埋めている
 *   " = 要らない("hello sailor" が素で通る。say "..." はむしろ弾かれる)
 *   - = 要らない(air-pump / hand-held / trap-door は door 等が単独でも引ける)
 *   数字 = pdp10 と h2o の 2 語のみ / # = デバッグ 4 語のみ
 * tables.h は gen_tables.py の生成物なので**ここで差し替える**。
 *   row 0(方向キーなし) = R1:1 □:. △:, ○:- ×:空白
 *   row 9(L1+↓)         = R1:0 のみ(面ボタンは空)
 * ★**元の字で 1 回だけ引く**。')' → '-' と '-' → 0 が同じ表に居るので、
 *   返り値をもう一度通すと row 0 の ○ まで消える。
 * ★0 = 「そのキーは空」。何も入れない(や行の い/え と同じ扱い)。 */
static uint16_t zork_sym(uint16_t c)
{
    switch (c) {
    case '(': return '.';
    case '?': return ',';
    case ')': return '-';
    case '!': return ' ';
    case '@': case '#': case '-': case '_': return 0;
    default:  return c;
    }
}

/* 図を描く側の入り口。★入力側(interactive_en)と**同じ zork_sym を通す**。
   日本語面は素通し。 */
static uint16_t grid_cell(int en, int row, int vowel)
{
    uint16_t c = gp_cell(en, row, vowel);
    return en ? zork_sym(c) : c;
}

/* 図に出す形。★空白は字として見えないので '_' を立てる(同梱フォントに '␣' が無い)。
   字面の言い換えはこの 1 箇所に閉じてある。 */
static uint16_t grid_show(int en, int row, int vowel)
{
    uint16_t c = grid_cell(en, row, vowel);
    return c == ' ' ? '_' : c;
}

/* T9 行の表示ラベル(英語)。★小文字。ゲームが打ち返す語は**必ず小文字で出る** ——
   gparser.zil の WORD-PRINT が入力バッファをそのまま印字し、mojozork.c の read が
   その手前でバッファを小文字化するため。指標だけ大文字にすると、未知語のときに
   同じ語が 2 つの字面で 2 行差に並ぶ。
   ★row 0 は記号の行なので中身を名乗る。row 9 は "0" しか無い(zork_sym を参照)。 */
static const char *ROW_LABEL[10] =
    {".,", "abc", "def", "ghi", "jkl", "mno", "pqrs", "tuv", "wxyz", "0"};

static void row_label(int row, uint16_t out[4], int *n)
{
    const char *s = ROW_LABEL[(row >= 0 && row < 10) ? row : 0];
    *n = 0;
    while (s[*n] && *n < 4) { out[*n] = (uint16_t)s[*n]; (*n)++; }
}

/* ---- パッド → 行 / 段 ----
 * ★**図を描く側と入力する側で同じ式を使う**。写すと、図に出ている字と実際に入る字が
 *   ずれても誰も気づけない(web 版で踏んだ事故)。
 * 行: 何も押さない=0 / ←=1 / ↑=2 / →=3 / ↓=4。L1 を押すと +5(はまやらわ)。
 * 段: R1=あ / □=い / △=う / ○=え / ×=お。-1 = まだ選んでいない。
 */
static int pad_row(int p)
{
    const int dir = (p & BTN_LEFT) ? 1 : (p & BTN_UP) ? 2 : (p & BTN_RIGHT) ? 3
                  : (p & BTN_DOWN) ? 4 : 0;
    return dir + ((p & BTN_L1) ? 5 : 0);
}

static int pad_vowel(int p)
{
    return (p & BTN_R1) ? 0 : (p & BTN_SQ) ? 1 : (p & BTN_TRI) ? 2
         : (p & BTN_CIR) ? 3 : (p & BTN_X) ? 4 : -1;
}

/* いま押されているパッドの状態を、入力の状態機械が食べる 1 フレームに写す。
 *
 * ★lt（L2 = 拗音シフト）は**呼ぶ側が決める**。英語面では渡さない —— 渡すと
 *   シフト系が生きて、500ms 以上握ると CapsLock が勝手にトグルし、短く押して離すと
 *   次の 1 文字だけ大文字になる。しかも L2 は履歴スクロールの修飾キーを兼ねているので
 *   遡るたびに踏む。Zork の辞書は全部小文字で、read が入力を小文字化するため用が無い。
 *   ★止めるのは呼び出し側。input.c は machine.ts と同一に保つ。 */
static GpFrame frame_of(int p, int ms, int lt)
{
    const int cc = !!(p & BTN_LEFT) + !!(p & BTN_UP) + !!(p & BTN_RIGHT)
                 + !!(p & BTN_DOWN) + !!(p & BTN_L1);
    const int vowel = pad_vowel(p);
    GpFrame f = { ms, pad_row(p), vowel, vowel >= 0,
                  lt ? !!(p & BTN_L2) : 0, !!(p & BTN_R2), cc };
    return f;
}

/* ★対話ループに入るときの引き継ぎ。**押されたままのものを「もう見た」ことにする**。
 *
 *   pad_prev … エッジ（Start / Select）用。0 で始めると、言語メニューを抜けた Start が
 *              そのまま最初のエッジになり、オプション画面が勝手に開く（実測）
 *   gp_sync_prev … ★**状態機械用**。こちらを忘れると、言語を**面ボタンで**選んだとき、
 *              その面ボタンが押されたままなので `!prevVowelPressed` が成立し、
 *              **選んだボタンに応じた字がコマンド欄に入った状態でゲームが始まる**
 *              （✕ なら「お」）。2026-08-29 に実機（R36H）で発覚。
 *              ★台本が**全部 Start で言語を選んでいた**ので、検査を素通りしていた。
 */
static void carry_over_pad(int lt)
{
    pad_prev = pad_read();
    if (pad_prev < 0)
        pad_prev = 0;
    GpFrame f = frame_of(pad_prev, 0, lt);
    gp_sync_prev(&gm, &f);
}

/* ★どのフェイスボタンでも決まる。「Start で開いて Start で閉じる」「開いた先で
   フェイスボタンを押せば決まる」は当時からの作法なので、画面に書かない */
#define BTN_FACE (BTN_CIR | BTN_X | BTN_TRI | BTN_SQ)

/* ---- 言語選択メニュー ---- */

/* 中央揃えの 1 行。mark = 1 で ＞ を付ける。
   ★**色と印を別々に受ける**。以前は selected が両方を兼ねていたので、
   「印は無いが強調したい」(= 起動メニューの `Zenmai`)が書けなかった。 */
static void menu_line(int y, const uint16_t *s, int n, uint16_t color, int mark)
{
    fill_rows(sbar, 0, STATUS_H, OPT_BG);   /* ★本文の外側は藍 */
    int w = 0;
    for (int i = 0; i < n; i++) w += glyph_w(s[i]);
    int x = (W - w) / 2;
    if (mark)
        draw24(sbar, STATUS_H, x - 40, 0, 0xFF1E, ACCENT);   /* ＞ */
    for (int i = 0; i < n; i++) {
        draw24(sbar, STATUS_H, x, 0, s[i], color);
        x += glyph_w(s[i]);
    }
    gp0_upload(0, y, W, STATUS_H, sbar[0]);
}

static int lang_menu(void)             /* 0 = 日本語 / 1 = ENGLISH */
{
    /* ★題・説明・選択肢はすべて gen_ui.py から引く。直書きすると「出す字がフォントに
       あるか」の照合(gen_ui.py の末尾)を通らない —— 無い字は**黙って空白で描かれる** */
    const UiStr *o_ja = &UI_LANG[0], *o_en = &UI_LANG[1];
    /* ★上段(名前 + 説明)を罫線で閉じ、その下に作品名 → 言語、と積む。
       説明は Zenmai に掛かるので**罫線の上**にいなければならない。
       説明と案内は控えめの色にして、選べる項目と見間違えさせない。 */
    enum { Y_TITLE = 112, Y_SUB = 148, Y_RULE = 180, Y_GAME = 212,
           Y_JA = 272, Y_EN = 312, Y_HINT = 384 };
    int sel = 0;
    menu_line(Y_TITLE, UI_TITLE.s, UI_TITLE.n, ACCENT, 0);
    menu_line(Y_SUB, UI_SUB.s, UI_SUB.n, DIM, 0);
    menu_line(Y_RULE, UI_RULE.s, UI_RULE.n, DIM, 0);
    menu_line(Y_GAME, UI_GAME.s, UI_GAME.n, INK, 0);
    menu_line(Y_JA, o_ja->s, o_ja->n, sel == 0 ? ACCENT : INK, sel == 0);
    menu_line(Y_EN, o_en->s, o_en->n, sel == 1 ? ACCENT : INK, sel == 1);
    /* ★メニューの入口を知らせるのはここだけ。本文にシステムの字は混ぜないし、
       いちばん助けが要る人ほど Start を試しに押さない(だから全員が通るここに置く) */
    menu_line(Y_HINT, UI_BOOT[sel].s, UI_BOOT[sel].n, DIM, 0);
    gpu_display_on();                  /* 表示オン(メニューが最初の画面) */
    /* ★いま押されているものを「押下済み」として引き継ぐ。0 で始めると、
       **「やめる」→「はい」を決めた Start がそのまま最初のエッジになり、
       起動メニューが出た瞬間に言語が決まってしまう**（interactive_en の
       pad_prev と同じ話。押しているものはエッジではない）。 */
    int prev = pad_read();
    if (prev < 0)
        prev = 0;
    for (;;) {
        wait_fields(1);
        int p = pad_read();
        if (p < 0) { prev = 0; continue; }
        int edge = p & ~prev;
        prev = p;
        if (edge & (BTN_UP | BTN_DOWN)) {
            sel ^= 1;
            menu_line(Y_JA, o_ja->s, o_ja->n, sel == 0 ? ACCENT : INK, sel == 0);
            menu_line(Y_EN, o_en->s, o_en->n, sel == 1 ? ACCENT : INK, sel == 1);
            menu_line(Y_HINT, UI_BOOT[sel].s, UI_BOOT[sel].n, DIM, 0);
        }
        if (edge & (BTN_START | BTN_FACE))   /* どのフェイスボタンでも決まる */
            return sel;
    }
}

/* ---- オプション ----
 *
 * ★入口は「コマンドを打っていないときの Start」。Start は入力中は確定なので、
 *   空のときだけが空いている(実測して確かめた)。Select は濁点 / 空白で埋まっている。
 * ★**メニューは本文に重ねる小さな板**、**行き先は全画面**。メニューは道しるべであって
 *   目的地ではないから軽い板で足り、行き先は読み物だから画面を占める。
 * ★ここに出る文字列は全部**こちらのもの**で、原作の文は 1 つも出ない。だから訳の表は
 *   通さず完成行で持つ(`gen_ui.py`)。「その文字列は誰のものか」がそのまま設計になる。
 * ★**画面に出す字は同梱フォントに入っていなければならない**。glyphs.h は「使う字だけ」
 *   なので、gen_ui.py が `ui_chars.txt` を書き出し gen_data.py がそれを読む。
 * ★**ボタンの案内は画面に書かない**。Start で開いたら Start で閉じる、開いた先で
 *   フェイスボタンを押せば決まる —— 当時から今まで浸透している作法なので、書くと
 *   画面がその分だけ狭くなるだけになる。だから**どのフェイスボタンでも決まる**
 *   (○/× を言語で入れ替える必要も消えた。作法に乗るほうが、地域差より強い)。
 */

enum { OPTM_MENU = 0, OPTM_PAGE };
enum { P_TYPING = 0, P_CMDS, P_LICENSE };   /* gen_ui.py の並びと同じ */
/* 重ねる板は**本文窓の中**(ステータス行の下)に置く。★部屋名が見えたままだと
   「まだゲームの中にいる」感が残るし、戻すのが render_window() だけで済む */
enum { OVL_X = 32, OVL_W = 336, OVL_Y = 56, OVL_ROW = 32, OVL_GAP = 8 };
/* 仕切り線用の置き場（縁取りは廃止 —— 地色だけで本文と区別が付く） */
enum { RULE_X = 96, RULE_W = 448 };
/* ★左右の余白は縁取りより内側にとる(MARGIN=32 だと枠に字が触る)。
   折り返し幅は gen_ui.py の TEXT_W と**必ず同じ値**にすること */
/* ★行間は**本文と同じ**にする。本文は BASE(24) + LEAD(8) = 32 の送りなので、
   ここも 32 —— 頁だけ 24(＝行間ゼロ)だと、漢字が続く行が上下でくっついて見えた。
   ★★行数は**画面に入るか**ではなく**実機で切れないか**で決める。12 行だと最下行が
   y=464 まで届き、CRT では下端に貼り付いて見えた（実機の指摘）。11 行なら 432 で終わり、
   下の余白 48px は「ひらがな入力方法」の欄の下端(HY_STRIP + CMD_H = 432)とも揃う。
   上 40px / 下 48px = ブラウン管のオーバースキャンの内側。 */
enum { PAGE_X = 48, PAGE_Y_TITLE = 40, PAGE_Y_TOP = 88, PAGE_ROW_H = 32, PAGE_ROWS = 11 };

static int opt_open, opt_sel, opt_mode, opt_page, page_top, page_count;
static short page_idx[UI_LINE_N];
static uint16_t ovl[OVL_ROW][OVL_W];   /* 板の転送用に詰め直す小バッファ */
static uint16_t frbuf[RULE_W * 2];     /* 仕切り線(横 448x2。縦の 2x192 も収まる) */

/* 板の帯を 1 本送る。★gp0_upload は矩形をリニアに読むので、幅 640 の sbar からは
   左上だけを送れない。ここで幅 OVL_W に詰め直すのが唯一の引っかかりだった。
   use_sbar=0 は地色だけの帯(行間と枠)。 */
static void ovl_band(int y, int h, int use_sbar, int top, int bot)
{
    for (int r = 0; r < h; r++) {
        for (int c = 0; c < OVL_W; c++)
            ovl[r][c] = use_sbar ? sbar[r][OVL_X + c] : OPT_BG;
        ovl[r][0] = ovl[r][1] = OPT_EDGE;
        ovl[r][OVL_W - 2] = ovl[r][OVL_W - 1] = OPT_EDGE;
    }
    if (top)
        for (int c = 0; c < OVL_W; c++)
            ovl[0][c] = ovl[1][c] = OPT_EDGE;
    if (bot)
        for (int c = 0; c < OVL_W; c++)
            ovl[h - 2][c] = ovl[h - 1][c] = OPT_EDGE;
    gp0_upload(OVL_X, y, OVL_W, h, ovl[0]);
}

/* 板の 1 行(左揃え)。mark=1 で ＞ を付ける */
static void ovl_row(int y, const UiStr *s, uint16_t color, int mark)
{
    fill_rows(sbar, 0, STATUS_H, OPT_BG);
    if (mark)
        draw24(sbar, STATUS_H, OVL_X + 16, 0, 0xFF1E, ACCENT);   /* ＞ */
    int x = OVL_X + 48;
    for (int i = 0; i < s->n; i++) {
        if (x + glyph_w(s->s[i]) > OVL_X + OVL_W - 16)
            break;
        draw24(sbar, STATUS_H, x, 0, s->s[i], color);
        x += glyph_w(s->s[i]);
    }
    ovl_band(y, STATUS_H, 1, 0, 0);
}

static void menu_draw(void)
{
    ovl_band(OVL_Y, OVL_GAP, 0, 1, 0);
    for (int i = 0; i < UI_PAGE_N; i++) {
        const int y = OVL_Y + OVL_GAP + i * OVL_ROW;
        ovl_row(y, &UI_ITEM[lang_en][i], i == opt_sel ? ACCENT : INK, i == opt_sel);
        ovl_band(y + STATUS_H, OVL_GAP, 0, 0, i == UI_PAGE_N - 1);
    }
}

/* 読み物の 1 行(全画面)。center=0 で左揃え(本文)、1 で中央(見出し・案内)。 */
static void page_row(int y, const uint16_t *s, int n, uint16_t color, int center)
{
    fill_rows(sbar, 0, STATUS_H, OPT_BG);
    int w = 0;
    for (int i = 0; i < n; i++)
        w += glyph_w(s[i]);
    int x = center ? (W - w) / 2 : PAGE_X;
    for (int i = 0; i < n; i++) {
        if (x + glyph_w(s[i]) > W - PAGE_X)
            break;
        draw24(sbar, STATUS_H, x, 0, s[i], color);
        x += glyph_w(s[i]);
    }
    gp0_upload(0, y, W, STATUS_H, sbar[0]);
}

/* 出す行だけを集める(言語と頁で出し分けるので番号が飛ぶ) */
static void page_index(void)
{
    const int want = lang_en ? 2 : 1;
    /* ★フォントの帰属は使っている実装のものだけを出す（lang とまったく同じ絞り方）。 */
    const int font = glyph_font_kind();
    page_count = 0;
    for (int i = 0; i < UI_LINE_N; i++)
        if (UI_LINES[i].page == opt_page &&
            (!UI_LINES[i].lang || UI_LINES[i].lang == want) &&
            (!UI_LINES[i].font || UI_LINES[i].font == font))
            page_idx[page_count++] = (short)i;
}

/* ---- 「ひらがな入力方法」= コントローラの図 ----
 *
 * ★本文を持たない。**押している状態がそのまま図に出る**のが説明になる(web 版と同じ)。
 * ★札は入力表から引く(`gp_cell`)。写さないので、表を直せば図も直る ——
 *   **図に出ている字と実際に入る字が違う**、が最悪の事故(web 版で踏んでいる)。
 * ★この頁だけは**面ボタンで戻らない**。面ボタンは字を出すボタンそのものだから、
 *   出口は Start(本文へ)の 1 つに絞る。
 */
/* ★並びは**実物のコントローラのまま**にする。上から L2 / L1 / 十字(面ボタン)。
   左右のボタンは中心へ寄せる(離しすぎると 1 行の字の並びに見える) */
enum { HLX = 168, HRX = 472, HDX = 56 };             /* 群の中心 x と左右のずれ */
/* ★群の見出し(「じゅうじキー = ぎょう」)は落とした —— 説明の一行が
   **左右の役をすでに言っている**ので、その分を消す操作の説明に回す */
enum { HY_L2 = 80, HY_L1 = 112,                      /* 肩(外側が上) */
       HY0 = 156, HY1 = 196, HY2 = 236,              /* 上 / 中 / 下 */
       HY5 = 284 };                                  /* 機能キーの札 */
enum { HDIV_X = 318, HDIV_Y = 72, HDIV_H = 192 };    /* 左右の群を分ける縦線 */
/* 試し打ち: 横線 → 説明 → 消し方 → コマンド欄を模した行 */
enum { HRULE_Y = 322, HY_TIP = 336, HY_BS = 372, HY_STRIP = 408 };
static int help_prev, help_gate;

/* sbar に中央揃えで 1 語置く(帯はあとでまとめて送る)。
   ★押しているものは**面を塗って字を抜く**。字の色だけを変えても、藍の上では
     金と白の差が小さくて分からなかった(実機の指摘)。 */
static void help_put(int cx, const uint16_t *s, int n, uint16_t color, int on)
{
    int w = 0;
    for (int i = 0; i < n; i++)
        w += glyph_w(s[i]);
    int x = cx - w / 2;
    if (on) {
        for (int r = 0; r < STATUS_H; r++)
            for (int c = x - 8; c < x + w + 8; c++)
                sbar[r][c] = OPT_EDGE;
        color = OPT_BG;                  /* 面の上は地色で抜く */
    }
    for (int i = 0; i < n; i++) {
        draw24(sbar, STATUS_H, x, 0, s[i], color);
        x += glyph_w(s[i]);
    }
}

static void help_put1(int cx, uint16_t ch, uint16_t color, int on)
{
    if (ch)                              /* 表に穴がある(や行の い/え)ときは何も置かない */
        help_put(cx, &ch, 1, color, on);
}

/* R1 の札だけは動く = その行の「あ段」そのもの。
   ★字は**面ボタンと同じ色**にする —— 役が同じ(字を出すボタン)だから。
     「R1」の側は他の肩の札と同じ控えめのまま。 */
static void help_put_r1(int cx, uint16_t ch, int on)
{
    static const uint16_t pre[3] = {'R', '1', ' '};
    int w = 0;
    for (int i = 0; i < 3; i++)
        w += glyph_w(pre[i]);
    if (ch)
        w += glyph_w(ch);
    int x = cx - w / 2;
    if (on) {
        for (int r = 0; r < STATUS_H; r++)
            for (int c = x - 8; c < x + w + 8; c++)
                sbar[r][c] = OPT_EDGE;
    }
    for (int i = 0; i < 3; i++) {
        draw24(sbar, STATUS_H, x, 0, pre[i], on ? OPT_BG : OPT_TEXT);
        x += glyph_w(pre[i]);
    }
    if (ch)
        draw24(sbar, STATUS_H, x, 0, ch, on ? OPT_BG : INK);
}

/* 帯を**縦の仕切りごと**送る。★仕切りを毎回引き直すと、ボタンを押すたびにちらつく
   (縁でそれを踏んだ)。帯が仕切りを持てば消えないので、引き直す必要がなくなる。 */
static void help_band(int y, int div)
{
    if (div)
        for (int r = 0; r < STATUS_H; r++)
            sbar[r][HDIV_X] = sbar[r][HDIV_X + 1] = OPT_TEXT;
    gp0_upload(0, y, W, STATUS_H, sbar[0]);
}

static void help_row_head(int cx, int row, int on)
{
    help_put1(cx, lang_en ? gp_row_char_en(row) : gp_row_char(row), INK, on);
}

static void help_draw(int p)
{
    const int base = (p & BTN_L1) ? 5 : 0;
    const int row = pad_row(p), dir = row - base;
    const int vowel = pad_vowel(p);
    const int en = lang_en;
    const UiStr *lbl = UI_HELP[en];

    /* 肩(外側): L2 / R2 */
    fill_rows(sbar, 0, STATUS_H, OPT_BG);
    help_put(HLX, lbl[1].s, lbl[1].n, OPT_TEXT, (p & BTN_L2) != 0);
    help_put(HRX, lbl[2].s, lbl[2].n, OPT_TEXT, (p & BTN_R2) != 0);
    help_band(HY_L2, 1);

    /* 肩(内側): L1 / R1。R1 の札は動く = その行の「あ段」そのもの */
    fill_rows(sbar, 0, STATUS_H, OPT_BG);
    help_put(HLX, lbl[0].s, lbl[0].n, OPT_TEXT, (p & BTN_L1) != 0);
    help_put_r1(HRX, en ? gp_row_char_en(row) : gp_row_char(row), (p & BTN_R1) != 0);
    help_band(HY_L1, 1);

    /* 上: ↑ の行 / △ の字 */
    fill_rows(sbar, 0, STATUS_H, OPT_BG);
    help_row_head(HLX, base + 2, dir == 2);
    help_put1(HRX, grid_show(en, row, 2), INK, vowel == 2);
    help_band(HY0, 1);

    /* 中: ← 中 → の行 / □ ○ の字。★中央 = どの向きも押していないとき */
    fill_rows(sbar, 0, STATUS_H, OPT_BG);
    help_row_head(HLX - HDX, base + 1, dir == 1);
    help_row_head(HLX,       base + 0, dir == 0);
    help_row_head(HLX + HDX, base + 3, dir == 3);
    help_put1(HRX - HDX, grid_show(en, row, 1), INK, vowel == 1);
    help_put1(HRX + HDX, grid_show(en, row, 3), INK, vowel == 3);
    help_band(HY1, 1);

    /* 下: ↓ の行 / × の字 */
    fill_rows(sbar, 0, STATUS_H, OPT_BG);
    help_row_head(HLX, base + 4, dir == 4);
    help_put1(HRX, grid_show(en, row, 4), INK, vowel == 4);
    help_band(HY2, 1);

    /* 機能キー: SELECT / START。★ここでの Start は「本文へ戻る」だが、
       札が説明しているのは**本文での**役 */
    fill_rows(sbar, 0, STATUS_H, OPT_BG);
    help_put(HLX, lbl[3].s, lbl[3].n, OPT_TEXT, (p & BTN_SELECT) != 0);
    help_put(HRX, lbl[4].s, lbl[4].n, OPT_TEXT, 0);
    help_band(HY5, 0);
}

/* 試し打ちの行。★本物のコマンド欄と**同じ絵**を使う(build_strip)。地色と y だけ違う */
static void help_strip(const uint16_t *cmd, int len, int car, const uint16_t *ind, int ilen)
{
    build_strip(OPT_BG, cmd, len, car, ind, ilen);
    gp0_upload(0, HY_STRIP, W, CMD_H, strip[0]);
}

static void help_open(int p)
{
    const UiStr *t = &UI_ITEM[lang_en][P_TYPING];
    paint_screen(OPT_BG);
    page_row(PAGE_Y_TITLE, t->s, t->n, ACCENT, 1);
    /* ★縦の仕切りはここで 1 回だけ。以降は帯が運ぶ */
    for (int i = 0; i < HDIV_H * 2; i++)
        frbuf[i] = OPT_TEXT;
    gp0_upload(HDIV_X, HDIV_Y, 2, HDIV_H, frbuf);
    /* 試し打ちの仕切りと説明 */
    for (int i = 0; i < RULE_W * 2; i++)
        frbuf[i] = OPT_TEXT;
    gp0_upload(RULE_X, HRULE_Y, RULE_W, 2, frbuf);
    fill_rows(sbar, 0, STATUS_H, OPT_BG);
    help_put(W / 2, UI_HELP[lang_en][5].s, UI_HELP[lang_en][5].n, OPT_TEXT, 0);
    help_band(HY_TIP, 0);
    fill_rows(sbar, 0, STATUS_H, OPT_BG);
    help_put(W / 2, UI_HELP[lang_en][6].s, UI_HELP[lang_en][6].n, OPT_TEXT, 0);
    help_band(HY_BS, 0);
    /* ★試し打ちは**本文と同じ入力の道**を使う。だから器も本物と同じ comp を空にするだけ
       (別の器を持つと、片方でしか通らない値が必ず出る) */
    clen = 0;
    caret = 0;
    /* ★頁を開けたボタンを離すまでは打たせない。離さないと**開けた ○ がそのまま
       「え」になる**(実測。エッジではなく押している状態で字が出る仕組みなので、
       pad_prev を潰すだけでは止まらない) */
    help_gate = 1;
    help_prev = p;
    help_draw(p);
    /* ★行インジケータは**開いた時点から**出す(ボタンを触るまで空だった) */
    uint16_t ind[4];
    int in_ = 1;
    if (lang_en)
        row_label(pad_row(p), ind, &in_);
    else
        ind[0] = gp_row_char(pad_row(p));
    help_strip(comp, 0, 0, ind, in_);
}

static void page_draw(void)
{
    paint_screen(OPT_BG);
    const UiStr *t = &UI_ITEM[lang_en][opt_page];
    page_row(PAGE_Y_TITLE, t->s, t->n, ACCENT, 1);
    for (int r = 0; r < PAGE_ROWS; r++) {
        const int i = page_top + r;
        if (i >= page_count)
            break;
        const UiLine *l = &UI_LINES[page_idx[i]];
        page_row(PAGE_Y_TOP + r * PAGE_ROW_H, UI_POOL + l->off, l->len,
                 l->dim ? OPT_TEXT : INK, 0);
    }
}

/* 本文の画面へ戻す。★重ねた板と全画面の頁で**戻す範囲が違う**。板は本文窓の中しか
   汚していないので、full=0 なら本文窓を描き直すだけでいい。 */
static void restore_main(int full)
{
    if (full) {
        paint_screen(BG);
        draw_status();
        draw_strip(comp, 0, 0, comp, 0);   /* 開くのは clen==0 のときだけ */
    }
    render_window();
}

/* ★オプションは**別ループを回さない**。開いている間の 1 フレーム分をここで処理し、
   パッドを読むのは対話ループの 1 箇所だけに保つ。
   ★**同じ入力に道が 2 本あると、片方でしか通らない値が必ず出る**（別プロジェクトで
   実際に踏んだ。半年ぶん片方の経路だけキー種別が欠けていた）。
   戻り値 0 = オプションを閉じて本文へ戻る。 */
static int options_step(int p, int edge)
{
    if (opt_mode == OPTM_PAGE && opt_page == P_TYPING) {
        if (edge & BTN_START)            /* ★出口は Start だけ(面ボタンは字を出す側) */
            return 0;
        if (p != help_prev) {            /* 押している状態が変わったときだけ描き直す */
            help_prev = p;
            help_draw(p);
        }
        if (help_gate) {                 /* 開けたボタンを離すまで打たせない */
            if (pad_vowel(p) < 0)
                help_gate = 0;
            return 1;
        }
        return 2;                        /* ★試し打ち = このあと**本文と同じ入力処理**へ落とす */
    }
    if (opt_mode == OPTM_PAGE) {
        const int maxtop = page_count > PAGE_ROWS ? page_count - PAGE_ROWS : 0;
        int moved = 0;
        /* ★上下も**頁送り**にする。1 行ずつ送っても読み進まない
           （ライセンスを腰を据えて読む人はいない、という実機の判断） */
        if ((edge & (BTN_UP | BTN_L1)) && page_top > 0) {
            page_top -= PAGE_ROWS;
            if (page_top < 0)
                page_top = 0;
            moved = 1;
        }
        if ((edge & (BTN_DOWN | BTN_R1)) && page_top < maxtop) {
            page_top += PAGE_ROWS;
            if (page_top > maxtop)
                page_top = maxtop;
            moved = 1;
        }
        if (moved)
            page_draw();
        if (edge & BTN_START)            /* ★頁からは一足で本文へ戻る */
            return 0;
        if (edge & BTN_FACE) {           /* 一段もどってメニューへ */
            opt_mode = OPTM_MENU;
            restore_main(1);
            menu_draw();
        }
        return 1;
    }
    if (edge & (BTN_UP | BTN_DOWN)) {
        opt_sel = (opt_sel + (edge & BTN_UP ? UI_PAGE_N - 1 : 1)) % UI_PAGE_N;
        menu_draw();
    }
    if (edge & BTN_FACE) {               /* ★どのフェイスボタンでも決まる */
        opt_page = opt_sel;
        opt_mode = OPTM_PAGE;
        page_top = 0;
        if (opt_page == P_TYPING) {
            help_open(p);
        } else {
            page_index();
            page_draw();
        }
        return 1;
    }
    if (edge & BTN_START)                /* Start で開いたら Start で閉じる */
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
    menu_draw();
}

static void options_close(void)
{
    /* ★試し打ちの字を本文のコマンド欄へ持ち出さない(器は同じものを使っている) */
    clen = 0;
    caret = 0;
    gm.eagerSet = 0;
    restore_main(opt_mode == OPTM_PAGE);
    opt_open = 0;
    opt_mode = OPTM_MENU;
}

/* ---- 対話ループ(英語) ----
 * ★**quit で戻る**。以前は抜けた先で空回りしていて、「やめる」→「はい」が
 *   そのままフリーズに見えた（実機の指摘）。戻り先は _start の外側のループ = 起動メニュー。
 */

static void interactive_en(void)
{
    int fields = 0, rt_hold = 0, dirty = 1, prev_row = -1;
    int ls_prev = 0, ls_rep = 0;
    gp_init(&gm);
    clen = 0;
    caret = 0;
    carry_over_pad(0 /* 英語面は L2 を渡さない */);
    for (;;) {
        wait_fields(1);
        fields++;
        uint8_t axes[4];
        int p = pad_read_ex(axes);
        if (p < 0) { pad_prev = 0; continue; }
        int edge = p & ~pad_prev;
        pad_prev = p;

        if (opt_open) {                 /* オプションを開いている間は入力を横取りする */
            const int r = options_step(p, edge);
            if (!r) {
                options_close();
                dirty = 1;
                continue;
            }
            /* ★2 = 試し打ち。**横取りせず、下の入力処理をそのまま通す** ——
               打つ道を分けると、片方でしか通らない値が必ず出る */
            if (r != 2)
                continue;
        }

        GpFrame f = frame_of(p, fields * 17, 0 /* 英語面は L2 を渡さない */);
        const int row = f.row;
        GpAction a[4];
        int an = gp_step(&gm, 1 /* english */, &f, a);
        for (int i = 0; i < an; i++) {
            if (a[i].type != GPA_KANA)
                continue;
            uint16_t t[2];
            int tn = 0;
            for (int k = 0; k < a[i].tlen && k < 2; k++) {
                uint16_t ch = zork_sym(a[i].text[k]);
                if (ch) t[tn++] = ch;
            }
            if (!tn)                   /* 空のキー: 前の字も消さずに何もしない */
                continue;
            comp_insert(t, tn, a[i].replace);
            dirty = 1;
        }
        gp_sync_prev(&gm, &f);

        /* 右スティック = GIME 由来のフリック(↓確定 / ←BS)。★日本語面と同じ意味 */
        int rs = pad_rstick_flick(axes);
        if (rs)
            gp_break_rt_cycle(&gm);

        if (edge & BTN_SELECT) {       /* Select = 空白(row 0 の × にも同じ字がある) */
            uint16_t sp = ' ';
            comp_insert(&sp, 1, 0);
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
        if (((edge & (BTN_START | BTN_L3)) || rs == 4) && clen && !opt_open) {
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
        /* L2+↑↓ / 左スティック縦 = 履歴スクロール(試し打ち中は本文を触らない) */
        if (fields % 3 == 0 && !opt_open) {
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
            if (opt_open)
                help_strip(comp, clen, caret, ind, in_);
            else
                draw_strip(comp, clen, caret, ind, in_);
            prev_row = row;
            dirty = 0;
        }
    }
}

/* ---- 対話ループ(日本語。quit で戻るのは英語面と同じ) ---- */

static void interactive_ja(void)
{
    int fields = 0, rt_hold = 0, dirty = 1;
    int ls_prev = 0, ls_rep = 0;
    int pending_verb = -1;
    uint16_t prev_rowchar = 0;
    gp_init(&gm);
    clen = 0;
    caret = 0;
    carry_over_pad(1);
    for (;;) {
        wait_fields(1);
        fields++;
        uint8_t axes[4];
        int p = pad_read_ex(axes);
        if (p < 0) { pad_prev = 0; continue; }
        int edge = p & ~pad_prev;
        pad_prev = p;

        if (opt_open) {                 /* オプションを開いている間は入力を横取りする */
            const int r = options_step(p, edge);
            if (!r) {
                options_close();
                dirty = 1;
                continue;
            }
            /* ★2 = 試し打ち。**横取りせず、下の入力処理をそのまま通す** ——
               打つ道を分けると、片方でしか通らない値が必ず出る */
            if (r != 2)
                continue;
        }

        GpFrame f = frame_of(p, fields * 17, 1);
        const int row = f.row;
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
        if (((edge & (BTN_START | BTN_L3)) || rs == 4) && clen && !opt_open) {
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
            /* ★{SAID} は打った物の表示形。物を打っていないときは空を渡して訳語辞書へ落とす
               (echo のように打鍵で埋めてはいけない —— 字として画面に出る穴なので) */
            tr_set_said16(cr.said, cr.said_len);
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
        /* L2+↑↓ / 左スティック縦 = 履歴スクロール(試し打ち中は本文を触らない) */
        if (fields % 3 == 0 && !opt_open) {
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
            /* ★同じ絵を、試し打ちのときだけ別の場所に出す */
            if (opt_open)
                help_strip(comp, clen, caret, &rowchar, 1);
            else
                draw_strip(comp, clen, caret, &rowchar, 1);
            prev_rowchar = rowchar;
            dirty = 0;
        }
    }
}

/* 1 周ぶん —— 言語を選び、ゲームを起こし、対話ループへ入る。
 * 対話ループは quit で**普通に return する**ので、ここも普通に返ってくる。 */
static void boot_once(void)
{
    gpu_init();
    paint_screen(OPT_BG);              /* ★言語メニューも本文の外側 = 藍 */
    render_init();
    jp_text_init();                    /* 描画器は共通(ASCII 行にはルビ帯が付かない) */
    body_top = STATUS_Y + STATUS_H + 8;
    body_h = CMD_Y - 8 - body_top;

    pad_try_analog();
    lang_en = lang_menu();
    paint_screen(BG);                  /* メニューを消す */

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
    } else {
        draw_strip(0, 0, 0, 0, 0);
        interactive_ja();
    }
}

#ifdef ZM_SDL

/* ★SDL 版の「やめる」は**プロセスを終える**。PS1 と違って .bss を潰す道が使えない
 *   （glibc と SDL の状態まで消えてしまう）。PortMaster ではポートを抜けると
 *   ランチャのメニューへ戻るので、作法としてもこちらが正しい。
 *   ★言語の選び直しは「もう一度起動する」で足りる。 */
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    boot_once();
    return 0;
}

#else

/* ★**quit は「起動しなおす」**。対話ループが戻ってきたら、そのまま頭から回る。
 *
 * ★戻るときに **.bss をもう一度潰す**のが要点。「何を消すか」を数え上げる形にすると
 *   **いずれ数え漏らす** —— 本文の履歴・入力欄・オプションの開閉・ゲームパッドの
 *   状態機械に加えて、`lib.c` の LIFO アリーナ(`heap_top`)まで巻き戻す必要がある。
 *   .bss を潰せば「初期化されるもの」の定義がリンカ側と一致するので、漏れようがない。
 *   ★スタックは 0x801FFF00（.bss の遥か上）にあり、ここでは巻き込まない。
 * ★対話ループは**普通に return する**ので、スタックも一緒に巻き戻る。
 */
__attribute__((section(".text.start"), noreturn)) void _start(void)
{
    for (;;) {
        for (char *p = __bss_start; p < __bss_end; p++)
            *p = 0;
        boot_once();
    }
}

#endif
