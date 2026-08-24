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
    return 0;
}
