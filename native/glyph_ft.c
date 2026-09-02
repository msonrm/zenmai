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

/* ---- 寸法をフォント自身に測らせる ----
 *
 * ★**推定しない。** どれだけ上下へ出るかはフォント次第で、Higgins が別のフォント
 *   （ハングル・デーヴァナーガリー込み）を積めば当然変わる。代表的な字を実際に
 *   焼いて ink の上下を測り、大きさごとに覚える。
 *
 * ★これで**ベースラインも決まる** —— 一番高く出る字の頭が器の上端に来る位置に置けば、
 *   上も下も切れない。24px の実測は 上 21 / 下 -6 = 高さ 27 で、
 *   ★**24 行の器（状態行 sbar / コマンド欄 strip）には入らない**。22px なら 24 で入る。
 */
enum { MAX_PX = 64 };
typedef struct { short top, bot; char probed; } Metrics;
static Metrics met[MAX_PX + 1];

static const Metrics *metrics(int px)
{
    if (px < 1 || px > MAX_PX)
        px = 24;
    Metrics *m = &met[px];
    if (!m->probed) {
        /* 上へ一番出る字（CJK・l）と、下へ一番出る字（g j p q y 、）を混ぜる */
        static const uint16_t PROBE[] = {
            'g', 'j', 'p', 'q', 'y', ',', 'l', 'M',
            0x6F22 /* 漢 */, 0x3042 /* あ */, 0x3001 /* 、 */, 0xFF2D /* Ｍ */,
        };
        int top = 1, bot = 0;
        if (FT_Set_Pixel_Sizes(face, 0, (FT_UInt)px) == 0) {
            for (unsigned i = 0; i < sizeof PROBE / sizeof *PROBE; i++) {
                if (FT_Load_Char(face, PROBE[i], FT_LOAD_RENDER) != 0)
                    continue;
                int t = face->glyph->bitmap_top;
                int b = t - (int)face->glyph->bitmap.rows;
                if (t > top) top = t;
                if (b < bot) bot = b;
            }
        }
        m->top = (short)top;
        m->bot = (short)bot;
        m->probed = 1;
    }
    return m;
}

/* 高さ avail の器に収まる、px 以下で一番大きい大きさ。 */
static int fit_px(int px, int avail)
{
    while (px > 8) {
        const Metrics *m = metrics(px);
        if (m->top - m->bot <= avail)
            break;
        px--;
    }
    return px;
}

/* ★実際に使う大きさ。**起動時に 1 度だけ決めて、以後どこでも同じ**。
 *
 * ★これを呼ぶたびに器の高さから決める作りにしたら壊れた（2026-08-30・実機）——
 *   送り幅（glyph_w）は 24px のままなのに、24 行の器では 22px で描かれるので、
 *   **`─`（U+2500）を並べた罫線が破線になり**、字間も広がって見えた。
 *   ★**送り幅と描く大きさは常に一致していなければならない。**
 *   だから「器ごとに縮める」のではなく「一番きつい器に全部を合わせる」。
 *
 *   一番きつい器 = 24 行（状態行 sbar / コマンド欄 strip）。Noto は 24px だと
 *   ink が 27 行あるので入らず、22px で 24 行にちょうど収まる。
 *   ★焼いたビットマップ版（PS1）は 24 行に収まる字形なので影響を受けない。 */
/* ルビ帯は canvas（448 行）の中なので窮屈ではない。RUBY_ZONE = 14（空き 1 + ルビ 12 + 空き 1）*/
enum { BOX24 = 24, BOX12 = 14 };
static int size24, size12;

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
    size24 = fit_px(24, BOX24);
    size12 = fit_px(12, BOX12);

    /* ★どれを開いたかを 1 行残す。同梱したものではなくシステムのものを拾っていると、
       ★開発機では動くのに**別の機種で豆腐だらけになる**（そして原因が見えない）。
       PortMaster では Zenmai.sh が log.txt に流すので、ここに出れば追える。 */
    fprintf(stderr, "font: %s (本文 %dpx / ふりがな %dpx)\n", path, size24, size12);
}

int glyph_font_kind(void) { return 2; }   /* 同梱のアウトラインフォント（Noto） */

void glyph_clip(int y0, int y1)
{
    clip_y0 = y0;
    clip_y1 = y1;
}

int glyph_w(uint16_t code)
{
    Cell *c = lookup(code, size24);
    /* ★描くときと**同じ大きさ**の送りを返す。ここがずれると罫線が破線になる。 */
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

/* ★器に入る大きさを選び、測った ink の上端にベースラインを合わせる。
 *
 *   avail = rows - y ＝ この器で y から下に使える行数。
 *   本文（canvas は 448 行・行送り 32）では 24px がそのまま通る。
 *   ★24 行しかない器（状態行 sbar / コマンド欄 strip）では 22px へ落ちる ——
 *     落とさないと descender が器から出て、**.bss の隣を踏み潰していた**。
 *
 * ★送り幅（glyph_w）は 24px のまま返す。器の中で字が 8% 細くなるだけで、
 *   ★**字の開始位置はずれない**（累積誤差にならない）ので、キャレットも合う。
 */
static void draw_at(uint16_t (*buf)[W], int rows, int x, int y,
                    uint16_t code, uint16_t color, int px)
{
    if (rows - y <= 0)
        return;
    Cell *c = lookup(code, px);
    if (!c || !c->bm)
        return;
    int baseline = y + metrics(px)->top;
    int gx = x + c->left;
    int gy = baseline - c->top;
    for (int r = 0; r < c->h; r++) {
        int py = gy + r;
        /* ★rows で止める。ここを H（480）で見ていたのが、24 行の器
           （strip / sbar）に descender を書き込んで .bss を壊していた原因。 */
        if (py < clip_y0 || py >= clip_y1 || py < 0 || py >= rows)
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

void draw24(uint16_t (*buf)[W], int rows, int x, int y, uint16_t code, uint16_t color)
{
    draw_at(buf, rows, x, y, code, color, size24);
}

void draw12(uint16_t (*buf)[W], int rows, int x, int y, uint16_t code, uint16_t color)
{
    draw_at(buf, rows, x, y, code, color, size12);
}

/* ---- 整形 —— いまは 1 対 1 ----
 *
 * ★★**デーヴァナーガリーを積むときに置き換わるのは、この 1 本だけ**（HarfBuzz 版）。
 *   いまは字の並びと画の並びが一致する言語（日本語 / 韓国語 / 英語）しか載っていないので、
 *   code をそのままグリフ番号として扱い、送りは glyph_w に訊く。
 *
 * ★**この既定が「挙動不変」の錨になる** —— 呼ぶ側（render.c）を先に整形の経路へ
 *   移しても絵が 1 画素も変わらないので、**移し替えだけを画素一致で検査できる**。
 *   置き換えと移し替えを同時にやると、赤が出たときどちらのせいか分からなくなる。
 */
int shape_run(const uint16_t *s, int n, Shaped *out, int max)
{
    const int m = n < max ? n : max;
    for (int i = 0; i < m; i++) {
        out[i].gid = s[i];
        out[i].cluster = (uint16_t)i;
        out[i].adv = (int16_t)glyph_w(s[i]);
        out[i].dx = 0;
        out[i].dy = 0;
    }
    return m;
}

void draw24_gid(uint16_t (*buf)[W], int rows, int x, int y, uint16_t gid, uint16_t color)
{
    draw_at(buf, rows, x, y, gid, color, size24);
}
