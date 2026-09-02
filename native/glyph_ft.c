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

/* ★整形（HarfBuzz）は **-DZM_HARFBUZZ** を渡したビルドにだけ入る。
   ★Zenmai（Zork）は日本語と英語しか出さないので要らない —— 要らないものを
   リンクさせない。PS1 版には**そもそも載らない**（freestanding / 2MB RAM）。 */
#ifdef ZM_HARFBUZZ
#include <hb.h>
#include <hb-ft.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "glyph.h"

static FT_Library lib;
static int clip_y0, clip_y1;

/* ---- 面（フォント）----
 *
 * ★★**2 本目が要る理由** —— Noto Sans CJK は **CFF**、Noto Sans Devanagari は
 *   **glyf** で、1 つのフォントに両方の outline 形式は入らない（実測）。
 *   ★韓国語が「同じ 1 本に範囲を 3 行足すだけ」で済んだのと決定的に違うところ。
 * ★**2 本目は無くてもよい** —— その席を使わなければぶつからないので、起動は止めない
 *   （使えば豆腐になるが、glyph_init が開いた面を 1 行出すのでそこで気づける）。
 */
enum { FACE_N = 2, FACE_MAIN = 0, FACE_DEVA = 1 };
static FT_Face faces[FACE_N];

/* この字はどの面から出すか。★デーヴァナーガリーのブロックだけが 2 本目。 */
static int face_of(uint16_t code)
{
    return (code >= 0x0900 && code <= 0x097F && faces[FACE_DEVA]) ? FACE_DEVA : FACE_MAIN;
}

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

/* 2 本目（デーヴァナーガリー）。★無ければ NULL —— その席を使わなければ困らない。 */
static const char *deva_path(void)
{
    static char buf[1024];

    const char *env = getenv("ZENMAI_FONT_DEVA");
    if (env && *env && access(env, R_OK) == 0)
        return env;

    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 32);
    if (n > 0) {
        buf[n] = 0;
        char *slash = strrchr(buf, '/');
        if (slash) {
            strcpy(slash + 1, "zenmai-deva.ttf");
            if (access(buf, R_OK) == 0)
                return buf;
        }
    }
    /* ★システムからは拾わない —— 開発機に 1 本も無いことがあり（実測）、
       あったとしても機種で字形が変わると絵が変わる。同梱したものだけを使う。 */
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

/* ★**グリフ番号で引く。** 面が 2 つになったので、鍵は (px, 面, グリフ番号) の 3 つ組。
   ★bit の割り当ては px が 24-31 / 面が 16-23 / グリフ番号が 0-15 ——
     グリフ総数は Noto Sans Devanagari で **845**（実測）なので 16bit に収まる。
   ★`key == 0` が「空き」を意味するので、**px >= 1** であることに依存している。 */
static Cell *lookup_gid(int fi, uint16_t gid, int px)
{
    const uint32_t key = ((uint32_t)px << 24) | ((uint32_t)fi << 16) | gid;
    const uint32_t h = (key * 2654435761u) % CACHE_N;

    Cell *c = NULL;
    for (int probe = 0; probe < 8; probe++) {
        Cell *t = &cache[(h + probe) % CACHE_N];
        if (t->key == key)
            return t;
        if (t->key == 0) {
            c = t;
            break;
        }
    }
    /* ★★**8 連続で埋まっていたら、最初の枠を追い出す。**
       ★以前はここで NULL を返していた —— それは「**黙って空白で描く**」ということ。
       頭書きには「追い出しは上書き」と書いてあったのに、実装は諦めていた。
       ★2026-09-02 に実際に踏んだ: 1,388 字を順に引くと 1 字だけ描かれなくなり、
       しかも**フォントの欠字と見分けがつかなかった**（検査の文言は
       「sh make_font.sh で焼き直すこと」で、字が無いほうを疑わせる）。
       ★単独で描くと出る、というのが切り分けの決め手だった。
       描かないより、古いものを捨てて描くほうがよい。 */
    if (!c) {
        c = &cache[h % CACHE_N];
        free(c->bm);
        c->bm = NULL;
        c->key = 0;
    }

    /* 焼く */
    if (!faces[fi] || FT_Set_Pixel_Sizes(faces[fi], 0, (FT_UInt)px) != 0)
        return NULL;
    /* ★小さい字は 1bit で焼く。12px のふりがなをグレースケールの AA で描くと、
       画数のある字が灰色ににじんで**かえって読みにくくなる**（実機の指摘。
       焼いた版の 12px ドットフォントの方が読めた）。字の大きさに対して
       画素が足りないときは、にじませるより潰した方が形が残る。 */
    const int mono = px <= RUBY_MONO_MAX;
    FT_Int32 flags = FT_LOAD_RENDER | (mono ? FT_LOAD_TARGET_MONO : 0);
    if (FT_Load_Glyph(faces[fi], gid, flags) != 0)
        return NULL;
    FT_GlyphSlot g = faces[fi]->glyph;
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

/* 字コードで引く。★cmap を通してグリフ番号に直してから**同じ表**を引く
   （面ごと・引き方ごとに表を持つと、どれが効いているのか追えなくなる）。
   整形の要らない 1 字を描く口（ルビの親字・記号・行インジケータ）が使う。 */
static Cell *lookup(uint16_t code, int px)
{
    const int fi = face_of(code);
    return lookup_gid(fi, (uint16_t)FT_Get_Char_Index(faces[fi], code), px);
}

#ifdef ZM_HARFBUZZ
/* ★大きさを引数で受ける整形。**`size24` がまだ決まっていない段（fit_px）から
   呼ぶため**に分けてある —— 器に入る大きさを決めるのに整形の結果が要る、
   という順序の縛りがここにある。 */
static int shape_run_px(const uint16_t *s, int n, Shaped *out, int max, int px);
#endif

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
        /* ★デーヴァナーガリーは**上下に母音記号が付く**ので、ここを足さないと
           器の見積もりが甘くなる（22px で ink が **29 行**ある ―― 実測）。
           ★**合字は整形しないと出ないが、それでよい** —— 高さを作っているのは
           母音記号のほうで、`क्ष`（結合）は 17 行、`हूँ` は 29 行（実測）。
           ★2 本目が無いときは測らない。CJK 面で引くと **.notdef（豆腐）の寸法**を
           測ってしまい、器の見積もりが理由なく変わる。 */
        int top = 1, bot = 0;
        if (FT_Set_Pixel_Sizes(faces[FACE_MAIN], 0, (FT_UInt)px) == 0) {
            for (unsigned i = 0; i < sizeof PROBE / sizeof *PROBE; i++) {
                if (FT_Load_Char(faces[FACE_MAIN], PROBE[i], FT_LOAD_RENDER) != 0)
                    continue;
                int t = faces[FACE_MAIN]->glyph->bitmap_top;
                int b = t - (int)faces[FACE_MAIN]->glyph->bitmap.rows;
                if (t > top) top = t;
                if (b < bot) bot = b;
            }
        }
#ifdef ZM_HARFBUZZ
        /* ★★**2 本目は「代表的な字」ではなく「代表的な並び」を、整形してから測る。**
           デーヴァナーガリーの母音記号は `y_offset` で持ち上げられるので、
           ★**単字の寄せ集めでは 2 行足りない**（実測: 24px で単字 29 / 整形後 31）。
           甘い見積もりで px を選ぶと、器に入ったつもりで**実機で上下が切れる**。
           ★並びは `हूँ हैं`（be 動詞の活用）—— DOCTOR の応答にほぼ毎行出る、
           いちばん高い形（綴りを anusvara に替えても逃げられないことは実測済み）。 */
        if (faces[FACE_DEVA]) {
            static const uint16_t PROBE_DEVA[] = {
                0x0939, 0x0942, 0x0901,        /* हूँ */
                0x0020,
                0x0939, 0x0948, 0x0902,        /* हैं */
            };
            Shaped sh[32];
            const int m = shape_run_px(PROBE_DEVA,
                                       (int)(sizeof PROBE_DEVA / sizeof *PROBE_DEVA),
                                       sh, 32, px);
            for (int k = 0; k < m; k++) {
                Cell *c = lookup_gid(sh[k].face, sh[k].gid, px);
                if (!c || c->h == 0)
                    continue;
                /* ★`dy` は**下向きが正**なので、ベースラインからの上端は引き算。 */
                const int t = c->top - sh[k].dy;
                const int b = t - c->h;
                if (t > top) top = t;
                if (b < bot) bot = b;
            }
        }
#endif
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
/* ★**一番きつい器の高さ。** `render.h` の `BASE` と**同じ数**でなければならない
   （どちらも `glyph.h` の `ZM_BASE` から取る）。 */
enum { BOX24 = ZM_BASE, BOX12 = 14 };
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
    if (FT_New_Face(lib, path, 0, &faces[FACE_MAIN]) != 0) {
        fprintf(stderr, "フォントを開けない: %s\n", path);
        exit(1);
    }
    /* ★2 本目は**開けなくても止めない**（その席を使わなければ困らない）。 */
    const char *dpath = deva_path();
    if (dpath && FT_New_Face(lib, dpath, 0, &faces[FACE_DEVA]) != 0)
        faces[FACE_DEVA] = NULL;
    size24 = fit_px(24, BOX24);
    size12 = fit_px(12, BOX12);

    /* ★どれを開いたかを 1 行残す。同梱したものではなくシステムのものを拾っていると、
       ★開発機では動くのに**別の機種で豆腐だらけになる**（そして原因が見えない）。
       PortMaster では Zenmai.sh が log.txt に流すので、ここに出れば追える。 */
    fprintf(stderr, "font: %s (本文 %dpx / ふりがな %dpx)\n", path, size24, size12);
    if (faces[FACE_DEVA])
        fprintf(stderr, "font: %s (デーヴァナーガリー)\n", dpath);
    else if (dpath)
        fprintf(stderr, "font: ★デーヴァナーガリーを開けなかった: %s\n", dpath);
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
static void draw_at_gid(uint16_t (*buf)[W], int rows, int x, int y,
                        int fi, uint16_t gid, uint16_t color, int px)
{
    if (rows - y <= 0)
        return;
    Cell *c = lookup_gid(fi, gid, px);
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

/* 字コードで描く。★面の決定と cmap 引きをここで済ませる。 */
static void draw_at(uint16_t (*buf)[W], int rows, int x, int y,
                    uint16_t code, uint16_t color, int px)
{
    const int fi = face_of(code);
    draw_at_gid(buf, rows, x, y, fi,
                (uint16_t)FT_Get_Char_Index(faces[fi], code), color, px);
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
/* 整形しない 1 字をそのまま置く。
   ★★**cmap を通してグリフ番号にする。** 面が 2 つになってからは描く側
   （draw24_gid）が**グリフ番号で**引くので、字コードのまま渡すと別の字が出る。 */
static void put_plain(Shaped *o, const uint16_t *s, int i)
{
    const int fi = face_of(s[i]);
    o->gid = (uint16_t)FT_Get_Char_Index(faces[fi], s[i]);
    o->cluster = (uint16_t)i;
    o->adv = (int16_t)glyph_w(s[i]);
    o->dx = 0;
    o->dy = 0;
    o->face = (uint8_t)fi;
}

#ifndef ZM_HARFBUZZ

int shape_run(const uint16_t *s, int n, Shaped *out, int max)
{
    const int m = n < max ? n : max;
    for (int i = 0; i < m; i++)
        put_plain(&out[i], s, i);
    return m;
}

#else

/* ★px ごとに作り直す（HarfBuzz の font は大きさを覚えている）。
   面ごとに 1 つ持てば足りるので、表にはしない。 */
static hb_font_t *hbf[FACE_N];
static int hbf_px[FACE_N];

static hb_font_t *hb_for(int fi, int px)
{
    if (hbf[fi] && hbf_px[fi] == px)
        return hbf[fi];
    if (hbf[fi])
        hb_font_destroy(hbf[fi]);
    FT_Set_Pixel_Sizes(faces[fi], 0, (FT_UInt)px);
    hbf[fi] = hb_ft_font_create_referenced(faces[fi]);
    hbf_px[fi] = px;
    return hbf[fi];
}

/* ★`dx` / `dy` は 1 バイトしかない。22px なら母音記号の寄せは ±10 程度に収まるが、
   壊れた値が来たときに**別の場所へ描く**より、端で止めたほうが気づける。 */
static int8_t clamp8(int v)
{
    return v > 127 ? (int8_t)127 : (v < -128 ? (int8_t)-128 : (int8_t)v);
}

int shape_run(const uint16_t *s, int n, Shaped *out, int max)
{
    return shape_run_px(s, n, out, max, size24);
}

static int shape_run_px(const uint16_t *s, int n, Shaped *out, int max, int px)
{
    int o = 0, i = 0;
    while (i < n && o < max) {
        /* ★**面ごとに run を割る**（itemization）—— 日本語とデーヴァナーガリーは
           同じ行に混ざりうるし、HarfBuzz は 1 回に 1 つの font しか受けない。 */
        const int fi = face_of(s[i]);
        int j = i + 1;
        while (j < n && face_of(s[j]) == fi)
            j++;

        if (fi == FACE_MAIN) {
            /* ★★**主面は整形を通さない。** 「通しても同じ絵になるはず」ではない ——
               Noto Sans の合字（`fi` など）とカーニングが効いて**いまの絵が変わる**。
               日本語・韓国語・英語は字の並びと画の並びが 1 対 1 なので、通す用が無い。 */
            for (; i < j && o < max; i++, o++)
                put_plain(&out[o], s, i);
            continue;
        }

        hb_font_t *hf = hb_for(fi, px);
        hb_buffer_t *buf = hb_buffer_create();
        hb_buffer_add_utf16(buf, s + i, j - i, 0, j - i);
        hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
        hb_buffer_set_script(buf, HB_SCRIPT_DEVANAGARI);
        hb_buffer_set_language(buf, hb_language_from_string("hi", -1));
        hb_shape(hf, buf, NULL, 0);

        unsigned gn = 0;
        hb_glyph_info_t *gi = hb_buffer_get_glyph_infos(buf, &gn);
        hb_glyph_position_t *gp = hb_buffer_get_glyph_positions(buf, &gn);
        for (unsigned k = 0; k < gn && o < max; k++, o++) {
            out[o].gid = (uint16_t)gi[k].codepoint;
            /* ★cluster は**この run の中での位置**で返るので、run の頭を足して
               元の列の位置に直す（下線とキャレットがこれを見る）。 */
            out[o].cluster = (uint16_t)(i + gi[k].cluster);
            out[o].adv = (int16_t)(gp[k].x_advance >> 6);
            out[o].dx = clamp8(gp[k].x_offset >> 6);
            /* ★★**y の向きが逆。** HarfBuzz は上が正、画面は下が正。 */
            out[o].dy = clamp8(-(gp[k].y_offset >> 6));
            out[o].face = (uint8_t)fi;
        }
        hb_buffer_destroy(buf);
        i = j;
    }
    return o;
}

#endif

void draw24_gid(uint16_t (*buf)[W], int rows, int x, int y,
                uint16_t gid, int face, uint16_t color)
{
    draw_at_gid(buf, rows, x, y, face, gid, color, size24);
}
