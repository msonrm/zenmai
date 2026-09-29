/* Zenmai の芯。規則と境界は session.h。
 *
 * ★2026-09-28 に main.c から抜き出した（中身は 1 行も変えていない。変えたのは入口の形だけ）。
 *   PS1 / SDL の画素一致（test-sdl.sh ほか）と、PC-98 版の記録の一致（test-pc98.sh）で確かめてある。
 */
#include <stdint.h>
#include "session.h"
#include "render.h"            /* 流す先: draw_plain / draw_echo / hist_blank と INK */
#include "translate.h"
#include "cmd.h"
#include "cmd_data.h"
#include "card.h"

#define main mojozork_main_unused
#include "vendor/mojozork.c"
#undef main

static const uint8_t *story_init;      /* 初期イメージ（セーブの差分の相手）。★動的領域だけ見る */
static ZMachineState zm;
static char statusbuf[49];
static int lang_en;                    /* 1 = 訳さない */
static void (*die_hook)(const char *msg);

int sess_quit(void) { return GState->quit; }
const char *sess_status(void) { return statusbuf; }

/* ---- VM 側(MojoZork 埋め込み) ---- */
enum { OBUF_MAX = 12288 };
static char obuf[OBUF_MAX];
static int olen;

static void zm_writestr(const char *str, const uintptr slen)
{
    for (uintptr i = 0; i < slen && olen < OBUF_MAX - 1; i++)
        obuf[olen++] = str[i];
}

#if defined(__GNUC__)
__attribute__((noreturn))
#endif
static void zm_die(const char *fmt, ...)
{
    if (die_hook)
        die_hook(fmt);
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
    const int n = cmem_pack(GState->story, story_init, dyn, out + o, outmax - o);
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
    cmem_unpack(in + o, len - o, story_init, GState->story, dyn);
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

static void vm_init(uint8_t *ram, uint32_t len, const uint8_t *init)
{
    story_init = init;
    GState = &zm;
    GState->die = zm_die;
    GState->writestr = zm_writestr;
    initStory(0, ram, len);
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

static void put_frag16(uint16_t *dst, int *o, int max, int fi)
{
    for (int i = 0; i < cm_frags[fi].len && *o < max; i++)
        dst[(*o)++] = cm_jpool[cm_frags[fi].off + i];
}

/* ---- 入口 ---- */

void sess_start(int en, uint8_t *ram, uint32_t len, const uint8_t *init, void (*die)(const char *msg))
{
    lang_en = en;
    die_hook = die;
    vm_init(ram, len, init);
    run_until_read();
    render_output();
}

void sess_submit_en(const uint16_t *typed, int n)
{
    char cmd[160 + 1];
    if (n > 160) n = 160;
    for (int i = 0; i < n; i++)
        cmd[i] = (char)typed[i];
    cmd[n] = '\0';
    draw_echo(typed, n);
    feed_cmd(cmd);
    run_until_read();
    render_output();
}

int sess_submit_ja(const uint16_t *typed, int n, int *pending_verb)
{
    static CmdRes cr;
    cmd_run(typed, n, *pending_verb, &cr);
    draw_echo(typed, n);                 /* 打った通りを反響 */
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
            /* ★1 字の断片は名指ししない（走査で余った「ゅう」のような雑音）。
               ただし**原簿が宣言した語は 1 字でも名指しする** ——
               `滝` を落として「（読み取れなかった）」になっていた（実機の指摘）。 */
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
                draw_plain(msg, ml, INK);
                return 1;
            }
            ml = 0;
            put_frag16(msg, &ml, 256, 11);       /* （読み取れなかった） */
        }
        if (cr.note_len)
            put_frag16(msg, &ml, 256, 14);       /* ） */
        draw_plain(msg, ml, INK);
        return 1;
    }
    if (cr.needs_object) {
        *pending_verb = cr.verb_idx;
        draw_plain(cr.ask, cr.ask_len, INK);
        return 1;
    }
    *pending_verb = -1;
    if (cr.echo_word_len)
        tr_set_echo16(cr.echo_word, cr.echo_word_len);
    else
        tr_set_echo16(typed, n);
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
    return 0;
}
