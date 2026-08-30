/* Zenmai セーブ — ファイル版（SDL2 ビルド。card.c の対）。
 *
 * card.h の 3 本をそのまま実装する。上の層（main.c の zm_save / zm_restore）は
 * 「メモリーカード」だとは思っておらず、★**バイト列を預けて取り戻すだけ**なので、
 * ここを差し替えると保存先が変わる。
 *
 * ★**枠は 1 つだけ**という判断も引き継ぐ（card.h の注記）——
 *   名前やスロットを訊く画面を出すと、コントローラだけで遊べるという芯が壊れる。
 *
 * 置き場所: 環境変数 ZENMAI_SAVE があればそのパス。無ければ SDL の設定ディレクトリ
 * （Linux なら ~/.local/share/msonrm/zenmai/zenmai.sav）。
 * ★PortMaster の起動スクリプトは ZENMAI_SAVE を $GAMEDIR に向ける ——
 *   ポートのフォルダごと持ち運べる方が、あの環境では作法に合う。
 */
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include "card.h"

static const char *save_path(void)
{
    static char path[1024];
    if (path[0])
        return path;
    const char *env = SDL_getenv("ZENMAI_SAVE");
    if (env && *env) {
        SDL_snprintf(path, sizeof path, "%s", env);
        return path;
    }
    char *pref = SDL_GetPrefPath("msonrm", "zenmai");
    if (pref) {
        SDL_snprintf(path, sizeof path, "%szenmai.sav", pref);
        SDL_free(pref);
    } else {
        SDL_snprintf(path, sizeof path, "zenmai.sav");
    }
    return path;
}

int card_save(const unsigned char *data, int len)
{
    if (len <= 0 || len > CARD_DATA_MAX)
        return 0;
    FILE *f = fopen(save_path(), "wb");
    if (!f)
        return 0;
    /* ★書けた長さで判定する。fclose の失敗まで見るのは、SD カードが抜かれた
       ような場合に「保存できた」と嘘をつかないため。 */
    size_t n = fwrite(data, 1, (size_t)len, f);
    int ok = (n == (size_t)len) && (fclose(f) == 0);
    return ok ? 1 : 0;
}

int card_load(unsigned char *out, int max)
{
    FILE *f = fopen(save_path(), "rb");
    if (!f)
        return -1;
    size_t n = fread(out, 1, (size_t)max, f);
    fclose(f);
    return n > 0 ? (int)n : -1;
}

int card_have(void)
{
    FILE *f = fopen(save_path(), "rb");
    if (!f)
        return 0;
    int c = fgetc(f);
    fclose(f);
    return c != EOF;
}
