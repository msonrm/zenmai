/* Zenmai セーブ —— DOS 版（PC-98。card.c / save_file.c の対）。
 * ★置き場はカレントディレクトリの ZENMAI.SAV。書式は PS1 / SDL と同じ（session.c が作る）ので、
 *   ファイルを持ち運べば版をまたいで続きから遊べる。 */
#include <stdio.h>
#include "card.h"

static const char SAVE_PATH[] = "ZENMAI.SAV";

int card_save(const unsigned char *data, int len)
{
    if (len <= 0 || len > CARD_DATA_MAX)
        return 0;
    FILE *f = fopen(SAVE_PATH, "wb");
    if (!f)
        return 0;
    /* ★書けた長さと fclose の両方で判定する（フロッピーが一杯でも「保存できた」と言わない） */
    const size_t n = fwrite(data, 1, (size_t)len, f);
    const int ok = (n == (size_t)len) && (fclose(f) == 0);
    return ok ? 1 : 0;
}

int card_load(unsigned char *out, int max)
{
    FILE *f = fopen(SAVE_PATH, "rb");
    if (!f)
        return -1;
    const size_t n = fread(out, 1, (size_t)max, f);
    fclose(f);
    return n > 0 ? (int)n : -1;
}

int card_have(void)
{
    FILE *f = fopen(SAVE_PATH, "rb");
    if (!f)
        return 0;
    const int c = fgetc(f);
    fclose(f);
    return c != EOF;
}
