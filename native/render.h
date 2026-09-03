/* Zenmai 描画層（本体 main.c と demo_main.c のデモで共有）。機械に触る部分は plat.h。
 * 規則の正典は gen_mock.py(Python ゴールデン)。挙動を変えるときは両方を見ること。 */
#ifndef RENDER_H
#define RENDER_H
#include <stdint.h>
#include "plat.h"          /* ★機械に触る 9 本（画面 / パッド）と W・H・BTN_* はここ */
#include "glyph.h"         /* ★字の 5 本（幅と描画）はここ */
#include "content.h"

/* ★**行の高さ（器）は `glyph.h` の `ZM_BASE`。** ★字の側（`glyph_ft.c` の `BOX24`）と
 *   ここが**同じ数を見ていなければならない** —— 器を広げても字の側が 24 のままだと、
 *   `fit_px` が古い器で大きさを決め、**広げた意味が無いまま px だけ落ちる**
 *   （2026-09-02 に踏んだ。器 30 にしたのに本文が 19px になった）。
 *
 * ★**下の 3 つは導出にしてある。** もとは 24 を前提に手で計算した数
 *   （BODY_H = 400 / CMD_Y = 432 / WIN_H = 448）が並んでいて、器を変えると
 *   ★**どれか 1 つを直し忘れて、本文とコマンド欄が重なる**。ZM_BASE = 24 では
 *   もとの値と一致する（400 / 432 / 448）。 */
enum {
    MARGIN = 32, TEXT_W = W - 2 * MARGIN,
    TOP = 24,
    RUBY_ZONE = 14, BASE = ZM_BASE, LEAD = 8, BLANK = ZM_BASE,
    CMD_H = ZM_BASE,                   /* コマンド欄は 1 行 */
    CMD_Y = H - 24 - CMD_H,            /* コマンド欄(下セーフ 24px) */
    BODY_H = CMD_Y - 8 - TOP,          /* 本文窓(欄との隙間 8px) */
    WIN_H = BODY_H + 48,               /* 表示窓の描画バンド(本文窓 + 余白) */
    SCROLL_STEP = 19,
    WAIT_TYPE = 75, WAIT_READ = 150,
};

#define BG     0x0442                  /* web 版ダークテーマの紙色を RGB555 化 */
#define INK    0x6B9D
#define ACCENT 0x36B9
#define DIM    0x31CF

extern uint16_t canvas[WIN_H][W];
extern uint16_t strip[CMD_H][W];
extern int cursor, first_line;
extern int pad_prev, pad_seen;
/* 本文窓の位置と高さ(既定 = TOP/BODY_H。ステータス行を持つ画面は狭めて使う) */
extern int body_top, body_h;

/* ---- テキスト履歴と表示窓 ---- */
extern int view_px;                    /* 窓上端(履歴座標 px) */
void hist_line(const uint16_t *s, int n, uint16_t color);   /* 1 論理行を積む */
void hist_blank(void);
int hist_total(void);
int hist_min_view(void);               /* 遡れる上限(捨てた分の境界) */
void render_window(void);              /* 窓を描いて VRAM へ */
void view_bottom(void);
int view_scroll(int d);                /* クランプ付き。動いたら 1 */
void scroll_new(void);                 /* 新しい内容を半行送りで見せる */
/* 行の描画器(既定 = ASCII 語折返しの素描画。JA は jp_text.c が登録) */
extern void (*line_render)(const uint16_t *s, int n, uint16_t color);

void render_init(void);                /* 内部状態のゼロ化(.bss は非ゼロで来る) */


void draw_logical(const Line *ln, uint16_t color);
/* 低水準の押し込み(draw_jp = jp_text.c が使う) */
void push_char(uint16_t ch, uint16_t color);
/* ★**語を作る字か**（＝空白で語を切る言語の字か。規則の正典は render.c の頭書き）。
   ★自前で割り付ける側に**同じ規則を貸す**ための口。ハングルとデーヴァナーガリーは
     `> 0x7F` なのに空白で語を切るので、「ASCII でなければどこでも折ってよい」は
     成り立たない —— それを 2 か所に書くとずれる。 */
int word_char(uint16_t c);
void push_text(const uint16_t *s, int n, uint16_t color);
void push_ruby(const uint16_t *base, int blen, const uint16_t *ruby, int rlen, uint16_t color);
void flush_vline(uint16_t color);
/* 日本語行: 実行時にルビを分節して描く(実装 = jp_text.c。JA ビルドのみリンク) */
void draw_jp(const uint16_t *s, int n, uint16_t color);
void draw_plain(const uint16_t *s, int n, uint16_t color);
void draw_echo(const uint16_t *s, int n);
void draw_strip(const uint16_t *cmd, int len, int caret, const uint16_t *ind, int ilen);
/* コマンド欄を strip[][] に組むだけ(地色を選べる)。送るのは呼んだ側 */
void build_strip(uint16_t bg, const uint16_t *cmd, int len, int caret,
                 const uint16_t *ind, int ilen);
void jp_text_init(void);   /* JA ビルド: ルビ付き描画器を登録 */
/* 右スティックのフリック検出: 倒し込みで 1=左 2=上 3=右 4=下 を 1 回だけ返す */
int pad_rstick_flick(const uint8_t axes[4]);

void fill_rows(uint16_t (*buf)[W], int y, int h, uint16_t color);

#endif
