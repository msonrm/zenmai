/* Zenmai 字の実装 — FreeType。
 *
 * glyph_baked.c の対。境界は glyph.h の 5 本だけで、組版・折返し・履歴・表示窓
 * （render.c）は**どちらを繋いでも無改造で動く**。
 *
 * ★これを選ぶと何が変わるか:
 *   - **字が増える**。焼いた版は「使う字だけ」（1,388 字）だが、こちらはフォントの
 *     被覆そのもの。★かな漢字変換を載せると**出る漢字が無限**になるので、
 *     Higgins ではこちらしか選べない
 *   - **プロポーショナルになる**。幅は `glyph_w` がフォントの送りを返すだけ ——
 *     ★元々 `char_w = f24.getlength(ch)` だったので、これは設計変更ではなく
 *     「等幅フォントを使っていたから等幅に見えていた」が解けるだけ
 *   - **アンチエイリアスが乗る**。24px の漢字は 1bit だと潰れる画が多い
 *
 * ★PS1 では使えない（freestanding / mini-libc）。PS1 と、PS1 との画素一致を
 *   確かめる SDL ビルドは glyph_baked.c を使う（test-sdl.sh）。
 *
 * ★**背景色は受け取らない。** アンチエイリアスは**描画先の画素に混ぜて**解決する
 *   ので、本文・ルビ帯・オプションの藍板のどこに描いても正しく乗る。
 */
#include <ft2build.h>
#include FT_FREETYPE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "glyph.h"

static FT_Library lib;
static FT_Face face;
static int clip_y0, clip_y1;

/* ---- フォントの在処 ---- */

/* ★探す順序: 環境変数 → 実行ファイルの隣 → システム。
   ★PortMaster では**同梱したものを使う**（機種によってはシステムに CJK フォントが
   無い）。起動スクリプトが ZENMAI_FONT を指すか、実行ファイルの隣に置く。 */
static const char *font_path(void)
{
    static char buf[1024];

    const char *env = getenv("ZENMAI_FONT");
    if (env && *env && access(env, R_OK) == 0)
        return env;

    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 32);
    if (n > 0) {
        buf[n] = 0;
        char *slash = strrchr(buf, '/');
        if (slash) {
            strcpy(slash + 1, "zenmai.otf");
            if (access(buf, R_OK) == 0)
                return buf;
        }
    }

    static const char *const SYS[] = {
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/opentype/noto/NotoSerifCJK-Regular.ttc",
        "/usr/share/fonts/truetype/fonts-japanese-gothic.ttf",
    };
    for (unsigned i = 0; i < sizeof SYS / sizeof *SYS; i++)
        if (access(SYS[i], R_OK) == 0) {
            snprintf(buf, sizeof buf, "%s", SYS[i]);
            return buf;
        }
    return NULL;
}

/* ---- グリフの入れ物 ----
 *
 * ★キャッシュする。窓を描き直すたびに 600 字ほど引くので（スクロール中は毎フレーム）、
 *   毎回ラスタライズすると実機の CPU では追いつかない。
 *   直接写像 + 線形探索の素朴な表で足りる（追い出しは上書き）。 */
enum { CACHE_N = 4096 };

/* ★この大きさ以下は 1bit で焼く。0 にすれば全部アンチエイリアスになる
   （ZENMAI_RUBY_AA=1 で実行時にも外せる ―― 見比べるため）。 */
#define RUBY_MONO_MAX (ruby_mono_max())
static int ruby_mono_max(void)
{
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("ZENMAI_RUBY_AA");
        v = (e && *e && *e != '0') ? 0 : 12;
    }
    return v;
}

typedef struct {
    uint32_t key;                      /* (px << 24) | code。0 = 空 */
    uint8_t  w, h, adv;
    int8_t   left, top;                /* ベースラインからの相対位置 */
    uint8_t *bm;                       /* w*h の 8bit アルファ */
} Cell;

static Cell cache[CACHE_N];

static Cell *lookup(uint16_t code, int px)
{
    uint32_t key = ((uint32_t)px << 24) | code;
    uint32_t h = (key * 2654435761u) % CACHE_N;
    for (int probe = 0; probe < 8; probe++) {
        Cell *c = &cache[(h + probe) % CACHE_N];
        if (c->key == key)
            return c;
        if (c->key == 0) {
            /* 空きに焼く */
            if (FT_Set_Pixel_Sizes(face, 0, (FT_UInt)px) != 0)
                return NULL;
            /* ★小さい字は 1bit で焼く。12px のふりがなをグレースケールの AA で描くと、
               画数のある字が灰色ににじんで**かえって読みにくくなる**（実機の指摘。
               焼いた版の 12px ドットフォントの方が読めた）。字の大きさに対して
               画素が足りないときは、にじませるより潰した方が形が残る。 */
            const int mono = px <= RUBY_MONO_MAX;
            FT_Int32 flags = FT_LOAD_RENDER | (mono ? FT_LOAD_TARGET_MONO : 0);
            if (FT_Load_Char(face, code, flags) != 0)
                return NULL;
            FT_GlyphSlot g = face->glyph;
            unsigned w = g->bitmap.width, hh = g->bitmap.rows;
            if (w > 255 || hh > 255)
                return NULL;
            size_t bytes = (size_t)w * hh;
            c->bm = malloc(bytes ? bytes : 1);
            if (!c->bm)
                return NULL;
            if (g->bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
                /* 1bit を 8bit のアルファへ展開（0 か 255）*/
                for (unsigned r = 0; r < hh; r++) {
                    const unsigned char *src = g->bitmap.buffer + (size_t)r * g->bitmap.pitch;
                    uint8_t *dst = c->bm + (size_t)r * w;
                    for (unsigned col = 0; col < w; col++)
                        dst[col] = (src[col >> 3] & (0x80 >> (col & 7))) ? 255 : 0;
                }
            } else {
                for (unsigned r = 0; r < hh; r++)
                    memcpy(c->bm + r * w, g->bitmap.buffer + r * g->bitmap.pitch, w);
            }
            c->w = (uint8_t)w;
            c->h = (uint8_t)hh;
            c->adv = (uint8_t)(g->advance.x >> 6);
            c->left = (int8_t)g->bitmap_left;
            c->top = (int8_t)g->bitmap_top;
            c->key = key;
            return c;
        }
    }
    return NULL;                       /* 8 連続で埋まっていたら諦める（描かない） */
}

/* ---- 境界の 5 本 ---- */

void glyph_init(void)
{
    const char *path = font_path();
    if (!path) {
        fprintf(stderr, "フォントが見つからない。ZENMAI_FONT で指すか、"
                        "実行ファイルの隣に zenmai.otf を置いてください\n");
        exit(1);
    }
    if (FT_Init_FreeType(&lib) != 0) {
        fprintf(stderr, "FreeType を初期化できない\n");
        exit(1);
    }
    if (FT_New_Face(lib, path, 0, &face) != 0) {
        fprintf(stderr, "フォントを開けない: %s\n", path);
        exit(1);
    }
    /* ★どれを開いたかを 1 行残す。同梱したものではなくシステムのものを拾っていると、
       ★開発機では動くのに**別の機種で豆腐だらけになる**（そして原因が見えない）。
       PortMaster では Zenmai.sh が log.txt に流すので、ここに出れば追える。 */
    fprintf(stderr, "font: %s\n", path);
}

void glyph_clip(int y0, int y1)
{
    clip_y0 = y0;
    clip_y1 = y1;
}

int glyph_w(uint16_t code)
{
    Cell *c = lookup(code, 24);
    return c ? c->adv : 24;
}

/* RGB555（下位 5bit = R / 中 5bit = G / 上位 5bit = B）にアルファで混ぜる。 */
static uint16_t blend555(uint16_t dst, uint16_t src, unsigned a)
{
    if (a == 0) return dst;
    if (a >= 255) return src;
    unsigned dr = dst & 31, dg = (dst >> 5) & 31, db = (dst >> 10) & 31;
    unsigned sr = src & 31, sg = (src >> 5) & 31, sb = (src >> 10) & 31;
    unsigned ia = 255 - a;
    unsigned r = (dr * ia + sr * a + 127) / 255;
    unsigned g = (dg * ia + sg * a + 127) / 255;
    unsigned b = (db * ia + sb * a + 127) / 255;
    return (uint16_t)(r | (g << 5) | (b << 10));
}

/* ★ベースラインの置き方。呼ぶ側は「高さ px の枠の上端 y」を渡してくるので、
   その枠の中に**表意文字の em 枠**が収まるようにする。CJK は em の 88% ほどを
   占めるのが慣例なので、ベースラインは上端から px*7/8 のあたりに置く。
   （フォントの ascender をそのまま使うと、CJK は asc-desc が em を超えるので
     枠から溢れる） */
static void draw_at(uint16_t (*buf)[W], int x, int y, uint16_t code, uint16_t color, int px)
{
    Cell *c = lookup(code, px);
    if (!c || !c->bm)
        return;
    int baseline = y + (px * 7 + 4) / 8;
    int gx = x + c->left;
    int gy = baseline - c->top;
    for (int r = 0; r < c->h; r++) {
        int py = gy + r;
        if (py < clip_y0 || py >= clip_y1 || py < 0 || py >= H)
            continue;
        const uint8_t *row = c->bm + (size_t)r * c->w;
        for (int col = 0; col < c->w; col++) {
            int pxx = gx + col;
            if (pxx < 0 || pxx >= W)
                continue;
            unsigned a = row[col];
            if (a)
                buf[py][pxx] = blend555(buf[py][pxx], color, a);
        }
    }
}

void draw24(uint16_t (*buf)[W], int x, int y, uint16_t code, uint16_t color)
{
    draw_at(buf, x, y, code, color, 24);
}

void draw12(uint16_t (*buf)[W], int x, int y, uint16_t code, uint16_t color)
{
    draw_at(buf, x, y, code, color, 12);
}
