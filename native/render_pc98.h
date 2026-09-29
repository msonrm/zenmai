/* PC-98 版の本文の寸法と窓（実装 = render_pc98.c。積む口そのものは render.h）。 */
#ifndef RENDER_PC98_H
#define RENDER_PC98_H
#include <stdio.h>

/* 画面（24 ラスタの行）: 0 行目 = 状態 / 2〜13 行目 = 本文 / 15 行目 = 入力欄 */
enum {
    BODY_ROW0 = 2, BODY_ROWS = 12,
    BODY_COL0 = 8,                     /* 横 64 から */
    BODY_CELLS = 64,                   /* 全角 32 字 */
    HANG_CELLS = 2,                    /* 行頭禁則のぶら下げ（全角 1 字）= 72〜73 桁目 */
    RUBY_W = 8, RUBY_DY = 1,           /* ふりがな: 8×8 の枠。帯の中で 1 ラスタ下げる（本文に接してよい） */
    RUBY_DX = 1,                       /* ★描くときだけ 1px 右へ。漢字 ROM の字は左端の 1 列が空き、美咲ゴシックは右端の 1 列が空く。
                                          枠のままだと読みの墨が親字の墨より 1px 左に出る（msonrm の指摘・2026-09-28） */
    RUBY_BAND = 8,                     /* 行の頭の 8 ラスタ */
    RUBY_COLOR = 8,                    /* 灰（グラフィックのパレット 8） */
    DECO_W = 40,                       /* 左右の装飾の幅。★48 だと、ぶら下げた句読点・音引きの右が詰まって見えた（QuuBee で遊んでの判断・2026-09-28）。
                                          ★8 の倍数に保つ（グラフィックの塗りはバイト = 8px 単位） */
    RUBY_X_MIN = 56, RUBY_X_MAX = 640 - DECO_W,   /* 読みがはみ出してよい範囲（左右の装飾の内側） */
    MARK_COL = 75,                     /* 窓の外に続きがある印（▲▼）の桁 = 横 600〜615（右の装飾の上） */
};

extern FILE *render_log;               /* 開いておくと積んだ論理行を UTF-8 で書く */

int  body_init(void);                  /* 本文の環を確保する（積む前に 1 回）。1 = 確保できた */

/* 窓を下端へ寄せて描く。animate = 1 なら 1 行ずつ送って見せる（キーボードで遊ぶとき） */
void body_show(int animate);
int  body_scroll(int d);               /* 遡る（負）/ 進む（正）。動いたら 1 */
int  body_at_bottom(void);

#endif
