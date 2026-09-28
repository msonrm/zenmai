/* PC-98 版の本文の寸法と記録（実装 = render_pc98.c。口そのものは render.h）。 */
#ifndef RENDER_PC98_H
#define RENDER_PC98_H
#include <stdio.h>

/* 段 2 の仮の画面: 0 行目 = 状態 / 2〜22 行目 = 本文 / 24 行目 = 入力欄 */
enum { PC98_BODY_TOP = 2, PC98_BODY_BOT = 22,
       PC98_BODY_L = 2, PC98_BODY_R = 78 };   /* 本文は [L, R) の 76 桁 */

extern FILE *render_log;               /* 開いておくと積んだ論理行を UTF-8 で書く */

#endif
