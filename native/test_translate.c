/* 訳の層のホスト照合テスト: walkthrough 全行の入出力対(JS が正典)と突き合わせる。
 *   gcc -std=c11 -O1 -Wall test_translate.c translate.c translate_data.c -o test_translate
 */
#include <stdio.h>
#include <string.h>
#include "translate.h"
#include "pairs.h"

int main(void)
{
    int fails = 0;
    for (int i = 0; i < PAIR_N; i++) {
        const Pair *p = &PAIRS[i];
        tr_set_echo(p->echo);
        unsigned short out[2048];
        int r = tr_line((const char *)p->raw, out, 2048);
        int ok;
        if (p->ja_len < 0)
            ok = r < 0;
        else
            ok = r == p->ja_len && !memcmp(out, p->ja, (size_t)r * 2);
        if (!ok) {
            fails++;
            if (fails <= 8) {
                printf("NG %d: raw=[%s]\n  期待:", i, p->raw);
                if (p->ja_len < 0) printf(" (素通し)");
                for (int t = 0; t < p->ja_len; t++) printf(" %04X", p->ja[t]);
                printf("\n  実際:");
                if (r < 0) printf(" (素通し)");
                for (int t = 0; t < r; t++) printf(" %04X", out[t]);
                printf("\n");
            }
        }
    }
    if (fails) {
        printf("不一致 %d / %d 行\n", fails, PAIR_N);
        return 1;
    }
    printf("全 %d 行一致\n", PAIR_N);

    /* ★かなの反響（轟音の部屋）。pairs.h の echo は ASCII なので**この形は corpus に
       載らない** —— 実際、貪欲な穴の探索を入れたとき `{ECHO} {ECHO} ...` が
       引けなくなったのに 711 行は全部通っていた（実プレイで見つかった）。
       固定表として持つ。 */
    static const unsigned short KITA[] = {0x304D, 0x305F};                 /* きた */
    static const unsigned short WANT[] = {0x304D, 0x305F, ' ', 0x304D, 0x305F,
                                          ' ', 0x2026, 0x2026};            /* きた きた …… */
    tr_set_echo16(KITA, 2);
    unsigned short eo[64];
    int er = tr_line("north north ...", eo, 64);
    int eok = er == (int)(sizeof WANT / sizeof *WANT) && !memcmp(eo, WANT, sizeof WANT);
    printf("%s 反響: north north ... → きた きた ……\n", eok ? "✓" : "✗");
    if (!eok) {
        printf("  実際:");
        if (er < 0) printf(" (素通し)");
        for (int t = 0; t < er; t++) printf(" %04X", eo[t]);
        printf("\n");
        return 1;
    }

    /* ★{SAID}: 原作が**入力バッファをそのまま印字**する行(You can't see any X here! など)は、
       訳語辞書ではなく**打った語**を返す。語を共有する物では辞書が別の物の名前を返すため
       (実プレイ 2026-08-27: 「ビニールの塊」を指したのに「穴の空いた舟」が出た)。
       ★原作にも一意な指し方が無い(舟が 3 つ)ので、送る英語を変えても直らない。 */
    tr_set_echo16(0, 0);
    static const unsigned short BEN[] = {0x5F01};   /* 弁 */
    static const unsigned short SW1[] = {0x5F01, 0x306A, 0x3069, 0x3001, 0x3053, 0x3053, 0x306B, 0x306F, 0x898B, 0x5F53, 0x305F, 0x3089, 0x306A, 0x3044, 0x3002};   /* 弁など、ここには見当たらない。 */
    static const unsigned short SW0[] = {0x7A74, 0x306E, 0x7A7A, 0x3044, 0x305F, 0x821F, 0x306A, 0x3069, 0x3001, 0x3053, 0x3053, 0x306B, 0x306F, 0x898B, 0x5F53, 0x305F, 0x3089, 0x306A, 0x3044, 0x3002};   /* 穴の空いた舟など、ここには見当たらない。 */
    static const char NOT_HERE[] = "You can't see any plastic boat here!";
    unsigned short so[64];
    tr_set_said16(BEN, 1);
    int sr = tr_line(NOT_HERE, so, 64);
    int sok = sr == (int)(sizeof SW1 / sizeof *SW1) && !memcmp(so, SW1, sizeof SW1);
    printf("%s 打った語: %s → 弁など、ここには見当たらない。\n", sok ? "✓" : "✗", NOT_HERE);
    if (!sok) {
        printf("  実際:");
        for (int t = 0; t < sr; t++) printf(" %04X", so[t]);
        printf("\n");
        return 1;
    }
    /* 打った語が無ければ訳語辞書へ落とす(setSaid を呼ばない器でも壊れない) */
    tr_set_said16(0, 0);
    sr = tr_line(NOT_HERE, so, 64);
    sok = sr == (int)(sizeof SW0 / sizeof *SW0) && !memcmp(so, SW0, sizeof SW0);
    printf("%s 打った語なし(辞書へ落とす) → 穴の空いた舟など、ここには見当たらない。\n", sok ? "✓" : "✗");
    if (!sok) {
        printf("  実際:");
        for (int t = 0; t < sr; t++) printf(" %04X", so[t]);
        printf("\n");
        return 1;
    }
    return 0;
}
