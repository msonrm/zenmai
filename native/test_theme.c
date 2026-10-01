/* pc98_theme の INI の読みと場面の照合の検査（ホスト）。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pc98_theme.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("NG  %s:%d  %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main(void)
{
    FILE *f = fopen("ZENMAI.INI", "w");
    fputs("[theme]\nband = #123 ; 上の帯\ntext = green\necho = nonsense\nside = 456\ninput_fg = Yellow\n", f);
    fclose(f);
    f = fopen("T.INI", "w");
    fputs("[theme]\nbody = #0A0\n[scene]\n"
          "Kitchen =\nForest* = band=#ABC text=red input_fg=cyan side=#zzz\nDam = top=#F00 ruby=#111\n* = band=#001\n", f);
    fclose(f);

    theme_config();
    CHECK(theme_rgb(TH_BAND) == 0x123);         /* 読めた */
    CHECK(theme_rgb(TH_SIDE) == 0x743);         /* # が無い → 既定のまま */
    CHECK(theme_attr(TH_TEXT) == 0x81);         /* green */
    CHECK(theme_attr(TH_ECHO) == 0xA1);     /* 読めない名前 → 既定 */
    CHECK(theme_attr(TH_INPUT_FG) == 0xC1);     /* 大文字小文字は区別しない */

    theme_work("T");
    CHECK(theme_rgb(TH_BODY) == 0x0A0);         /* 作品の [theme] が後から上書き */
    CHECK(theme_room("Kitchen  ", 9) == 0);     /* 書いたキーが無い行 = 全体のまま（変化なし） */
    CHECK(theme_room("Forest Path", 11) == 1);  /* 前方一致 */
    CHECK(theme_rgb(TH_BAND) == 0xABC);
    CHECK(theme_attr(TH_INPUT_FG) == 0xA1);
    CHECK(theme_rgb(TH_SIDE) == 0x743);         /* 読めない値は無視 */
    CHECK(theme_attr(TH_TEXT) == 0x81);         /* text は場面で替えない */
    CHECK(theme_room("Forest", 6) == 0);        /* 同じ装い → 替わらない */
    CHECK(theme_room("Dam", 3) == 1);
    CHECK(theme_rgb(TH_BAND) == 0x123);         /* 前の場面の色は残らない */
    CHECK(theme_rgb(TH_TOP) == 0xF00 && theme_rgb(TH_RUBY) == 0x111);
    CHECK(theme_room("Dam Lobby", 9) == 1);     /* 完全一致の Dam には合わず * へ */
    CHECK(theme_rgb(TH_BAND) == 0x001);
    CHECK(theme_room("Kitchen", 7) == 1);       /* * より先の空の行 → 全体に戻る */
    CHECK(theme_rgb(TH_BAND) == 0x123);

    remove("ZENMAI.INI");
    remove("T.INI");
    printf(fails ? "テーマ: %d 件 NG\n" : "テーマ: 全件一致\n", fails);
    return fails != 0;
}
