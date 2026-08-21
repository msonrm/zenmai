/* card.c のホストではなく **sim 上での** 検証。
 *
 * メモリーカードは JOY ポートの装置なので、ホスト(gcc)では動かせない。
 * PS-EXE として sim.py で走らせ、結果を RAM の決まった場所へ置いて `--dump` で読む。
 *
 *   sh build-card-test.sh
 *   python3 gen_card.py /tmp/card.mcd            # フォーマット済みの像を作る
 *   python3 sim.py card-test.psexe --card /tmp/card.mcd --dump 80010000,20 --max 40000000
 *
 * 期待: [0]=1(保存できた) [1]=長さ [2]=1(読み戻して一致) [3]=5A5A5A5Ah(ここまで来た印)
 */
#include "card.h"

extern char __bss_start[], __bss_end[];

#define N 3000

static unsigned char src[N], back[N];

/* ★結果は .data に置く。.bss だと「零化した直後の値」と区別が付かず、
   何も動かなくても 0 が並んで**失敗を成功と読み違える**余地が残る。 */
volatile unsigned int result[4] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};

__attribute__((section(".text.start"), noreturn)) void _start(void)
{
    for (char *p = __bss_start; p < __bss_end; p++) *p = 0;

    for (int i = 0; i < N; i++) src[i] = (unsigned char)(i * 7 + 3);

    result[0] = (unsigned int) card_save(src, N);
    const int n = card_load(back, sizeof back);
    result[1] = (unsigned int) n;

    int ok = (n == N);
    for (int i = 0; ok && i < N; i++)
        if (back[i] != src[i]) ok = 0;
    result[2] = (unsigned int) ok;
    result[3] = 0x5A5A5A5Au;

    for (;;) { }
}
