/* Zenmai PS1 描画層 + パッド(main.c の JP デモと zm_main.c の EN 版で共有)。
 * 規則の正典は gen_mock.py(Python ゴールデン)。挙動を変えるときは両方を見ること。 */
#ifndef RENDER_H
#define RENDER_H
#include <stdint.h>
#include "content.h"

enum {
    W = 640, H = 480, MARGIN = 32, TEXT_W = W - 2 * MARGIN,
    TOP = 24, BODY_H = 400,            /* 本文窓 y=24..424 */
    CMD_Y = 432, CMD_H = 24,           /* コマンド欄(下セーフ 24px) */
    RUBY_ZONE = 14, BASE = 24, LEAD = 8, BLANK = 24,
    WIN_H = 448,                       /* 表示窓の描画バンド(本文窓 + 余白) */
    SCROLL_STEP = 19,
    WAIT_TYPE = 75, WAIT_READ = 150,
};

#define BG     0x0442                  /* web 版ダークテーマの紙色を RGB555 化 */
#define INK    0x6B9D
#define ACCENT 0x36B9
#define DIM    0x31CF

#define GP0 (*(volatile uint32_t *)0x1F801810)
#define GP1 (*(volatile uint32_t *)0x1F801814)
#define GPUSTAT (*(volatile uint32_t *)0x1F801814)

#define BTN_SELECT (1 << 0)
#define BTN_L3     (1 << 1)
#define BTN_R3     (1 << 2)
#define BTN_START  (1 << 3)
#define BTN_UP     (1 << 4)
#define BTN_RIGHT  (1 << 5)
#define BTN_DOWN   (1 << 6)
#define BTN_LEFT   (1 << 7)
#define BTN_L2     (1 << 8)
#define BTN_R2     (1 << 9)
#define BTN_L1     (1 << 10)
#define BTN_R1     (1 << 11)
#define BTN_TRI    (1 << 12)
#define BTN_CIR    (1 << 13)
#define BTN_X      (1 << 14)
#define BTN_SQ     (1 << 15)

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
void gpu_init(void);
void gp0_fill(int x, int y, int w, int h, uint32_t rgb24);
void gp0_upload(int x, int y, int w, int h, const uint16_t *src);
void wait_fields(int n);
int pad_read(void);

int glyph_w(uint16_t code);
void draw24(uint16_t (*buf)[W], int x, int y, uint16_t code, uint16_t color);
void draw12(uint16_t (*buf)[W], int x, int y, uint16_t code, uint16_t color);

void draw_logical(const Line *ln, uint16_t color);
/* 低水準の押し込み(draw_jp = jp_text.c が使う) */
void push_char(uint16_t ch, uint16_t color);
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
void pad_try_analog(void);
void jp_text_init(void);   /* JA ビルド: ルビ付き描画器を登録 */
int pad_read_ex(uint8_t axes[4]);
/* 右スティックのフリック検出: 倒し込みで 1=左 2=上 3=右 4=下 を 1 回だけ返す */
int pad_rstick_flick(const uint8_t axes[4]);

void fill_rows(uint16_t (*buf)[W], int y, int h, uint16_t color);

#endif
