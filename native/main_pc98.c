/* Zenmai PC-98 版の入口（計画 = docs/pc98-port-plan.md）。
 *
 * ★芯（VM・訳・語彙・セーブ）は session.c で、PS1 / SDL と共有している。
 *   ここに在るのは起動画面・PC-98 の画面の外枠（上の帯・装飾・入力欄）と、入力の取り方だけ。
 *   本文は render_pc98.c（render.h の文字列の口）。
 *
 * ★**Zenmai は Z-machine で、Zork I はその上で動く見本の作品**という形を取る（2026-09-28・msonrm の判断。
 *   商標の「ZORK」を前に出さない）。だから起動するとまず `Zenmai` の起動画面で、そこで作品と言語を選ぶ
 *   —— PS1 版の起動メニューと同じ形（題 → 説明 → 罫線 → 作品名 → ENGLISH / 日本語）。
 *
 * 画面（1 行 24 ラスタ × 16 行。寸法と色は試作 pc98-mock/gen_screen.py と同じ・色は仮）:
 *   上の帯      縦 0〜31    0 行目に場所（左・黄）と得点（右）
 *   本文の上    縦 32〜44   装飾と同じ色
 *   左右の装飾  幅 40（DECO_W）
 *   本文        2〜13 行目・横 64〜575（全角 32 字 × 12 行）
 *   入力欄の枠  縦 352〜399 15 行目。全体を 1 色（縁は無い）
 *
 * 入力は 2 通り:
 *   ZENMAI              … キーボード（BIOS から直に読む）。ローマ字 / カナキーでかな、CAPS で英字
 *                         （kana_input.h）。ROLL DOWN / ↑ で本文を遡り、ROLL UP / ↓ で戻る。
 *                         やめるのはゲームの「やめる」（quit）
 *   ZENMAI /S 台本.TXT  … 台本（UTF-8・1 行 1 コマンド）を流し、ZENMAI.LOG に記録する。
 *                         `#!keys` の後の行は**打鍵として**キーボードと同じ道（ki_key）を通す
 *                         （`#!text` で戻る。半角カナはカナキーの打鍵になる）。
 *                         `#!line 文` は文をそのまま本文に流す（組み方を画面で確かめる用。VM は回さない）。
 *                         頭の `#!english` で英語面、`#!work 名前` で作品（ZORK2 など。無ければ一覧の最初）。
 *                         ★同じものをホストでも建てて（build-pc98.sh）記録を突き合わせる（test-pc98.sh）
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc98_text.h"
#include "pc98_gfx.h"
#include "pc98_music.h"
#include "render.h"
#include "render_pc98.h"
#include "session.h"
#include "pack.h"
#include "save_dos.h"
#include "translate.h"
#include "kana_input.h"
#include "pc98_version.h"

#ifndef PC98_HOST
#include <i86.h>
#endif

/* ★作品は焼き込まない。起動画面でカレントディレクトリの作品（パック + story・story だけ）を並べ、
 *   選んだものを開く（pack.h・段 8 の C） */
static ZmPack works[WORKS_MAX];
static int nworks;

enum { ROW_STATUS = 0, ROW_INPUT = 15, COL_L = BODY_COL0, COL_R = BODY_COL0 + BODY_CELLS };
static KiLine line;                    /* 入力欄 */
static int lang_en;                    /* 1 = ENGLISH（訳さない・英字で打つ）/ 0 = 日本語 */

/* ---- 画面の外枠 ---- */

enum { C_BG, C_BAND, C_DECO, C_INPUT, C_CARET = MARK_COLOR };   /* パレットの番号（8 = ふりがな） */

static void draw_chrome(void)
{
    gfx_palette(C_BG, 0, 0, 0);
    gfx_palette(C_BAND, 3, 2, 7);      /* 上の帯（紺） */
    gfx_palette(C_DECO, 4, 7, 3);      /* 左右の装飾（焦げ茶） */
    gfx_palette(C_INPUT, 5, 2, 5);     /* 入力欄の枠 */
    gfx_palette(C_CARET, 15, 15, 15);  /* キャレットと ▼（コマンド文字色 = 白）*/
    gfx_palette(RUBY_COLOR, 10, 10, 10);
    enum { TOP_H = 32, SIDE = DECO_W, IN_Y = 352,
           TOP_DECO = BODY_ROW0 * TXT_RASTERS + RUBY_DY - 4 };   /* 本文 1 行目のふりがなの 4 ラスタ上 */
    gfx_rect(0, 0, GFX_W, GFX_H, C_BG);      /* ★起動画面の地を消してから（残ると本文の地が縞になる） */
    gfx_rect(0, 0, GFX_W, TOP_H, C_BAND);
    gfx_rect(0, TOP_H, SIDE, IN_Y, C_DECO);
    gfx_rect(GFX_W - SIDE, TOP_H, GFX_W, IN_Y, C_DECO);
    gfx_rect(0, TOP_H, GFX_W, TOP_DECO, C_DECO);
    gfx_rect(0, IN_Y, GFX_W, GFX_H, C_INPUT);
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
    int nn;
    if (lang_en) {
        nn = name_end < 64 ? name_end : 64;
        for (int i = 0; i < nn; i++) name[i] = (uint8_t)sb[i];
    } else {
        nn = tr_word_str(sb, name_end, name, 64);
    }
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
    /* ★曲・絵の割り当て（INI）。状態行の英語の部屋名で決める。替わったときだけ記録に書く */
    const int ch = music_room(sb, name_end);
    if (render_log) {
        if (ch & MUSIC_CHANGED) fprintf(render_log, "# music: %s\n", *music_current() ? music_current() : "-");
        if (ch & PICTURE_CHANGED) fprintf(render_log, "# picture: %s\n", *picture_current() ? picture_current() : "-");
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
#endif

/* キャレット: グラフィックの細い縦線（幅 2px・字の高さ 16 ラスタ）。コマンド文字色。
   ★反転の空白だと 24 ラスタ全部（ふりがなの帯まで）が塗られて長く太かった（msonrm の指摘・2026-10-01） */
enum { CARET_W = 2, CARET_BLINK = 28 };    /* 点滅の半周期 = 垂直帰線の回数（約 0.5 秒） */
static int caret_col;
static void caret_show(int on)
{
    const int x = caret_col * 8, y = ROW_INPUT * TXT_RASTERS + (TXT_RASTERS - 16);
    gfx_fill(x, y, x + CARET_W, y + 16, on ? C_CARET : C_INPUT);
}

/* 入力欄: ＞ + 確定した字 + 組み立て途中のローマ字 + カーソル（反転の空白）。
   ★右端の打ち方の表示（かな / 英字）はやめた —— 打てば分かる（msonrm の判断・2026-09-29） */
static void draw_input(int caret)
{
    txt_clear(ROW_INPUT, ROW_INPUT, TA_WHITE);
    int col = COL_L;
    col += txt_put(ROW_INPUT, col, 0xFF1E, TA_CYAN);     /* ＞ */
    for (int i = 0; i < line.n; i++)
        col += txt_put(ROW_INPUT, col, line.buf[i], TA_WHITE);
    for (int i = 0; i < line.np; i++)
        col += txt_put(ROW_INPUT, col, (uint8_t)line.pend[i], TA_WHITE);
    caret_col = col;
    caret_show(caret);                 /* 打つたびに点き直す（点滅の位相を戻す） */
}

#ifndef PC98_HOST
enum { K_ROLLUP = 0x36, K_ROLLDOWN = 0x37, K_UP = 0x3A, K_LEFT = 0x3B, K_RIGHT = 0x3C, K_DOWN = 0x3D };

/* キーボードから 1 行（空でない行を Enter で送るまで戻らない） */
static void key_line(void)
{
    ki_clear(&line);
    draw_input(1);
    for (int t = 0, on = 1; ; ) {
        if (!kbd_hit()) {              /* 待つ間にキャレットを点滅させる */
            txt_vsync();
            if (++t >= CARET_BLINK) { t = 0; on = !on; caret_show(on); }
            continue;
        }
        t = 0; on = 1;
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
            ki_key(&line, c, kbd_caps() || lang_en);   /* ★CAPS は打ったときに読む。英語面は表を通さない */
        draw_input(1);
    }
    draw_input(0);
    if (!body_at_bottom()) {           /* 遡り中の確定は、まず下端へ跳んでひと呼吸置く */
        body_show(0);
        for (int i = 0; i < 18; i++) txt_vsync();
    }
}
#endif

#ifndef PC98_HOST
/* ---- 起動画面（PS1 版の起動メニューと同じ形）----
 * ★並びは **日本語が上・既定の選択も日本語**（PC-98 なら日本語が主。msonrm の判断・2026-09-29。
 *   それまでは PS1 版に揃えて ENGLISH が上・既定だった）。
 * ★2 つの項目は**左端をそろえる**（選ぶ印 ＞ が左にあるので。中央揃えだと字の幅の違いで左端がずれた）。 */

/* 1 行（UTF-8）を置く。left < 0 なら中央揃え、そうでなければ left 桁から。
   mark = 1 なら字の 4 桁左に ＞。戻り値 = 字の幅（桁） */
static int put_at(int row, int left, const char *s, uint8_t attr, int mark)
{
    uint16_t u[64];
    int n = 0, w = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p && n < 64) {             /* UTF-8 → UTF-16 */
        unsigned c = *p++;
        if (c >= 0xE0) { c = (c & 0x0F) << 12 | (p[0] & 0x3F) << 6 | (p[1] & 0x3F); p += 2; }
        else if (c >= 0xC0) { c = (c & 0x1F) << 6 | (p[0] & 0x3F); p += 1; }
        u[n++] = (uint16_t)c;
        w += txt_cells((uint16_t)c);
    }
    txt_clear(row, row, TA_WHITE);
    int col = left < 0 ? (TXT_COLS - w) / 2 : left;
    if (mark)
        txt_put(row, col - 4, 0xFF1E, TA_YELLOW);    /* ＞ */
    for (int i = 0; i < n; i++)
        col += txt_put(row, col, u[i], attr);
    return w;
}

static int center(int row, const char *s, uint8_t attr, int mark) { return put_at(row, -1, s, attr, mark); }

/* 起動画面。*wi = 選んだ作品（入るときは初めの選択）。戻り値 1 = ENGLISH / 0 = 日本語。
 * ★作品が 1 つなら今までと同じ見た目（題 → 日本語 / ENGLISH）。2 つ以上なら題の左右に ← → を出し、←→ で替える。
 * ★訳の無い作品は ENGLISH だけ。パックはあるが story が無い作品は、置くべき story を言って始めさせない */
static int title_menu(int *wi)
{
    enum { R_TITLE = 3, R_SUB = 4, R_GAME = 7, R_JA = 9, R_EN = 10, R_HINT = 13, R_VER = 15 };
    /* ★PC-98 のキーは RETURN（Enter ではない） */
    static const char *hint[2][2] = {
        { "↑↓ で選び、Return キーでゲームスタート", "UP / DOWN TO CHOOSE, RETURN TO START" },
        { "←→ で作品、↑↓ で言語を選び、Return キーでゲームスタート", "LEFT / RIGHT: GAME, UP / DOWN: LANGUAGE, RETURN TO START" } };
    const int menu_l = (TXT_COLS - 7) / 2;     /* 長いほう（ENGLISH = 7 桁）を中央に置いた左端 */
    gfx_palette(0, 0, 0, 0);
    gfx_palette(1, 3, 2, 7);           /* 地（上の帯と同じ紺） */
    gfx_palette(2, 7, 7, 9);           /* 罫線 */
    gfx_rect(0, 0, GFX_W, GFX_H, 1);
    center(R_TITLE, "Zenmai", TA_YELLOW, 0);
    const int w = center(R_SUB, "a Z-machine for Japanese, on the PC-9801", TA_WHITE, 0);
    /* ★罫線は説明の幅（PS1 版と同じ）。塗りは 8px 単位なので桁の境目に合う */
    const int y = (R_SUB + 1) * TXT_RASTERS + 12;
    gfx_rect((TXT_COLS - w) / 2 * 8, y, ((TXT_COLS - w) / 2 + w) * 8, y + 1, 2);
    center(R_VER, "ver. " ZM98_VERSION, TA_CYAN, 0);
    const int many = nworks > 1;
    int sel = 0, cur = *wi;
    if (*music_title_file()) music_start(music_title_file());   /* 起動画面の曲（既定は Bach の謎カノン・PMD が鳴らす）*/
    for (;;) {
        const ZmPack *p = &works[cur];
        const int ja_ok = p->has_ja && p->story[0];
        const int s = ja_ok ? sel : 1;
        char t[80];
        snprintf(t, sizeof t, many ? "←　%s　→" : "%s", p->title);
        center(R_GAME, t, TA_WHITE, 0);
        if (!p->story[0]) {
            snprintf(t, sizeof t, "story が見つかりません（release %u / serial %s）", (unsigned)p->release, p->serial);
            center(R_JA, t, TA_CYAN, 0);
            txt_clear(R_EN, R_EN, TA_WHITE);
        } else {
            put_at(R_JA, menu_l, ja_ok ? "日本語" : "日本語（この作品はまだ訳がありません）",
                   !ja_ok ? TA_CYAN : s ? TA_WHITE : TA_YELLOW, !s);
            put_at(R_EN, menu_l, "ENGLISH", s ? TA_YELLOW : TA_WHITE, s);
        }
        center(R_HINT, hint[many][s], TA_CYAN, 0);
        while (!kbd_hit()) { }         /* ★曲は PMD が割り込みで鳴らす（pc98_music.h） */
        const int k = kbd_get(), scan = k >> 8 & 0x7F, c = k & 0xFF;
        if (scan == K_UP || scan == K_DOWN)
            sel = ja_ok ? sel ^ 1 : sel;
        else if (scan == K_LEFT && many)
            cur = (cur + nworks - 1) % nworks;
        else if (scan == K_RIGHT && many)
            cur = (cur + 1) % nworks;
        else if ((c == '\r' || c == ' ') && p->story[0]) {
            sel = s;
            break;
        }
    }
    *wi = cur;
    music_stop();
    txt_clear(0, TXT_ROWS - 1, TA_WHITE);
    return sel;
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
    /* ★画面を切り替える前に作品を並べる（無ければ DOS の画面のまま言って返る） */
    nworks = pack_list(works, WORKS_MAX);
    if (!nworks) {
        printf("zenmai: no game here (put a story file .Z3 and/or a Zenmai pack .ZMP)\n");
        return 1;
    }
    int wi = 0;
    if (script) {
        render_log = fopen("ZENMAI.LOG", "wb");
        /* ★台本の頭の `#!english`（英語面）と `#!work 名前`（作品。無ければ一覧の最初）。起動画面は出さない */
        char head[64];
        long at = ftell(script);
        while (fgets(head, sizeof head, script)) {
            if (!strncmp(head, "#!english", 9)) {
                lang_en = 1;
            } else if (!strncmp(head, "#!work ", 7)) {
                strtok(head + 7, "\r\n");
                wi = -1;
                for (int i = 0; i < nworks; i++)
                    if (!strcmp(works[i].base, head + 7)) wi = i;
                if (wi < 0) {
                    printf("zenmai: no such game: %s\n", head + 7);
                    return 1;
                }
            } else {
                break;
            }
            at = ftell(script);
        }
        fseek(script, at, SEEK_SET);
    }
    music_config();                    /* ZENMAI.INI（曲の入り切り・起動画面の曲） */
    txt_init();
    gfx_init();
#ifndef PC98_HOST
    if (!script)
        lang_en = title_menu(&wi);
#endif
    ZmPack *pack = &works[wi];
    /* ★大きなもの（story・表 = ひと続きで取る）を先に、本文の環（20KB の塊）を後に確保する。
     *   逆にすると、拡張 1MB の機械では塊が拡張メモリの残りを食って、大きな表が取れなくなる（段 8） */
    const char *err = pack_open(pack, !lang_en);
    if (!err && !body_init())
        err = "not enough memory";
    if (err) {
        gfx_fini();
        txt_fini();
        printf("zenmai: %s: %s\n", pack->pack[0] ? pack->pack : pack->story, err);
        return 1;
    }
    save_dos_name(pack->base);         /* ZORK1.ZMP → ZORK1.SAV */
    music_work(pack->base);            /* ZORK1.INI（部屋ごとの曲・絵） */
    draw_chrome();
    jp_text_init();                    /* ふりがなを分ける描画器（jp_text.c）を本文に登録する */
    sess_start(lang_en, pack->ram, pack->len, pack->init, die);
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
        int no_turn = 0;
        if (lang_en)
            sess_submit_en(line.buf, line.n);
        else
            no_turn = sess_submit_ja(line.buf, line.n, &pending_verb);
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
