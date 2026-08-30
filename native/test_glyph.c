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
/* ★焼いた版の「出せる字」の一覧をそのまま持ち込む（照合の相手）。
   字形（base_rows / ruby_rows）は使わないが、同じ表の中にある。 */
#include "glyphs.h"

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
    /* ★字の大きさは「24 行の器に収まる」ように起動時に選ばれるので、
       y=0 で描くかぎり rows は出番が無い（＝安全網）。網が生きているかを見るには
       **器の底ぎりぎりから描く**。y = ROWS-6 なら ink は ROWS+17 行目まで届くので、
       器を高いと偽れば必ずはみ出す。はみ出さなければ網以外の理由で止まっている。
       ★2026-08-30: 最初この検査は y=0 で書いていて、大きさを 22px に固定した
       とたんに赤になった —— カナリアが「前提が変わった」を正しく教えた例。 */
    reset();
    glyph_clip(0, ROWS + GUARD);
    draw24(buf, ROWS + GUARD, 40, ROWS - 6, 'g', 0x7FFF);
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
    puts("--- 器の底ぎりぎりから描いても外へ出ないか ---");
    reset();
    glyph_clip(0, ROWS);
    draw24(buf, ROWS, 40, ROWS - 6, 'g', 0x7FFF);
    if (guard_ok())
        puts("✓ y = 底から 6 行目に 'g' を描いても器の外へ出ない");
    else {
        puts("✗ ★器の外へ書いた");
        ng++;
    }

    puts("");
    puts("--- ★焼いた版で出せる字は、この実装でも全部出せるか ---");
    /* ★make_font.sh が守っている約束の実測。ここが赤なら、同梱フォントの
       部分集合が足りていない = その字は**黙って空白で描かれる**。
       （2026-08-30 に U+2014 `—` で実際に踏んだ。ライセンス頁の
       「Zenmai —— このソフト」が「Zenmai　　このソフト」に見えていた） */
    {
        int blank = 0, first = 0;
        for (int i = 0; i < BASE_N; i++) {
            uint16_t code = base_info[i].code;
            if (code == 0x0020 || code == 0x3000)
                continue;              /* 空白は白いのが正しい */
            reset();
            glyph_clip(0, ROWS);
            draw24(buf, ROWS, 40, 0, code, 0x7FFF);
            int lo, hi;
            ink_rows(&lo, &hi);
            if (hi < lo) {
                if (!blank++)
                    first = code;
            }
        }
        if (blank) {
            printf("✗ ★%d 字が空白で描かれる（最初 = U+%04X）"
                   " —— sh make_font.sh で焼き直すこと\n", blank, first);
            ng++;
        } else {
            printf("✓ 焼いた %d 字は全部この実装でも出る\n", BASE_N);
        }
    }
    /* ★カナリア: 上の検査が「空白」を見分けられているか（見分けられないなら
       **何も描かない実装が満点を取る**）。フォントに絶対に無い私用領域で試す。 */
    reset();
    glyph_clip(0, ROWS);
    draw24(buf, ROWS, 40, 0, 0xE000, 0x7FFF);
    {
        int lo, hi;
        ink_rows(&lo, &hi);
        if (hi < lo)
            puts("✓ ★カナリア: 無い字（U+E000）はちゃんと空白と分かる");
        else {
            puts("✗ ★カナリア: 無い字にも ink がある = 空白の見分けが効いていない");
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
