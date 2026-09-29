#ifndef TABLOAD_H
#define TABLOAD_H
#include <stdio.h>
#include <stdint.h>

/* パックの節から表を読む（PC-98 版）。節の書式と、登録（<mod>_tab.c）を作るのは ctab.py。
 *
 * ★節の 1 行は欄を宣言の順に詰めたもの（構造体の詰め物は入れない）。ここで欄ごとに offsetof の位置へ
 *   展開する —— 構造体の詰め方がコンパイラで違っても（Watcom の -zp4 と gcc）同じに読める。
 * ★表は起動後に確保する（本体の像を小さく保つ・docs/pc98-port-plan.md の「段 8」）。 */

enum { TL_FIELDS_MAX = 20 };

typedef struct { unsigned short off, size; } TlField;

typedef struct {
    const char *name;                  /* 節の索引の名前（= C の変数名） */
    const void **ptr;                  /* 読んだ表を指させる先 */
    unsigned *count;                   /* 行の数を入れる先 */
    unsigned elem;                     /* sizeof（C の 1 行） */
    int nf;
    TlField f[TL_FIELDS_MAX];
} TlTab;

/* f の off から len バイトの節を読み、tabs の表をすべて確保して埋める。
 * 読めたら 0。読めなければ理由（ASCII —— DOS の画面に出すので）。 */
const char *tab_load(FILE *f, uint32_t off, uint32_t len,
                     const TlTab *tabs, int n, unsigned long schema);

#endif
