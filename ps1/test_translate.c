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
    return 0;
}
