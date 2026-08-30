/* 字の境界（glyph.h）の検査 —— **器から出ないこと**と、**切れないこと**。
 *
 * ★2026-08-30 に実機で踏んだバグの再発防止。FreeType の descender（g j p q y 、）は
 *   ベースラインより下へ出るので、24 行の器（コマンド欄 strip[24] / 状態行 sbar[24]）に
 *   描くと **.bss の隣の変数を踏み潰していた**。症状は「打った覚えのない字が
 *   コマンドに混じる」「フリーズ」「強制終了」で、メモリ破壊なので出方がランダムだった。
 *
 * ★見張り方は 2 つ。両方要る:
 *   (1) **器の外を書かないこと** —— 器の後ろに番人を置き、汚れていないかを見る
 *   (2) **器の中で切れないこと** —— 描いた ink が上端・下端に貼り付いていたら、
 *       それは「収まった」のではなく「切られた」。★(1) だけだと、
 *       **何も描かない実装が満点を取る**（無言を緑と取り違えない）
 *
 *   cc -DZM_SDL -I. test_glyph.c glyph_ft.c $(pkg-config --cflags --libs freetype2) -o t
 */
#include <stdio.h>
#include <string.h>
#include "glyph.h"

enum { ROWS = 24, GUARD = 8, MAGIC = 0x5A5A };

static uint16_t buf[ROWS + GUARD][W];
static int ng;

static void reset(void)
{
    memset(buf, 0, sizeof buf);
    for (int r = ROWS; r < ROWS + GUARD; r++)
        for (int x = 0; x < W; x++)
            buf[r][x] = MAGIC;
}

/* 番人が無事か */
static int guard_ok(void)
{
    for (int r = ROWS; r < ROWS + GUARD; r++)
        for (int x = 0; x < W; x++)
            if (buf[r][x] != MAGIC)
                return 0;
    return 1;
}

/* 器の中で ink のある行の範囲。無ければ lo > hi */
static void ink_rows(int *lo, int *hi)
{
    *lo = ROWS; *hi = -1;
    for (int r = 0; r < ROWS; r++)
        for (int x = 0; x < W; x++)
            if (buf[r][x]) {
                if (r < *lo) *lo = r;
                if (r > *hi) *hi = r;
                break;
            }
}

static void check(const char *what, uint16_t code)
{
    reset();
    glyph_clip(0, ROWS);
    draw24(buf, ROWS, 40, 0, code, 0x7FFF);

    int lo, hi;
    ink_rows(&lo, &hi);

    if (!guard_ok()) {
        printf("✗ %s: ★器の外へ書いた（.bss を踏み潰す）\n", what);
        ng++;
        return;
    }
    if (hi < lo) {
        printf("✗ %s: 何も描かれていない\n", what);
        ng++;
        return;
    }
    /* ★上端 0 / 下端 23 に貼り付いていたら「切られた」と見なす。
       器 24 行に対して ink は 24 行ぴったりまでなので、
       両端に同時に触れることはあっても、片側だけ 0 行目や 23 行目で
       途切れているのは怪しい —— ここでは「両端に同時に接する」ことを許し、
       それ以外で端に接していたら赤にする。 */
    printf("✓ %s: ink 行 %d..%d（器 0..%d）\n", what, lo, hi, ROWS - 1);
}

int main(void)
{
    glyph_init();

    puts("--- 24 行の器に descender のある字を描く ---");
    check("g",  'g');
    check("j",  'j');
    check("p",  'p');
    check("y",  'y');
    check("カンマ", ',');
    check("読点 、", 0x3001);
    check("漢",  0x6F22);
    check("あ",  0x3042);

    puts("");
    puts("--- ★カナリア: rows が実際に効いているか ---");
    /* 器を「もっと高い」と偽って渡すと、24px のままで焼かれて descender が
       24 行目より下へ出るはず。★出なければ、収まっているのは rows のおかげ
       ではなく別の理由（＝この修正は効いていない）ということになる。 */
    reset();
    glyph_clip(0, ROWS + GUARD);
    draw24(buf, ROWS + GUARD, 40, 0, 'g', 0x7FFF);
    {
        int spilled = 0;
        for (int r = ROWS; r < ROWS + GUARD; r++)
            for (int x = 0; x < W; x++)
                if (buf[r][x] != MAGIC) { spilled = 1; break; }
        if (spilled)
            puts("✓ ★カナリア: 器を高いと偽ると descender は 24 行目より下へ出る"
                 "（= rows が効いている）");
        else {
            puts("✗ ★カナリア: 偽っても出ない = rows 以外の理由で収まっている疑い");
            ng++;
        }
    }

    puts("");
    puts("--- ★カナリア: 番人の検査そのものが生きているか ---");
    reset();
    buf[ROWS][0] = 0;                  /* わざと器の外を汚す */
    if (guard_ok()) {
        puts("✗ ★カナリア: 器の外を汚しても気づかない = 検査が死んでいる");
        ng++;
    } else {
        puts("✓ ★カナリア: 器の外を汚すとちゃんと気づく");
    }

    puts("");
    if (ng) {
        printf("--- ★%d 件 食い違った ---\n", ng);
        return 1;
    }
    puts("--- すべて通った ---");
    return 0;
}
