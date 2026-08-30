/* 日本語行の描画器: 実行時ルビ分節(src/ruby.js の C 移植)。
 * jp_text_init() で line_render に登録する(JA・JP デモビルドが呼ぶ)。
 * 規則: キーは長さ降順・漢字の連なりを丸ごと覆うときだけ振る。
 * キー外の連続は push_text へ(ASCII の語折返しが効く)。
 */
#include "render.h"
#include "ruby_data.h"

static int is_kanji(uint16_t c)
{
    return (c >= 0x4E00 && c <= 0x9FA5) || c == 0x3005;
}

static const RbKey *key_at(const uint16_t *s, int n, int i)
{
    for (int k = 0; k < RB_KEY_N; k++) {
        const RbKey *kk = &rb_keys[k];
        if (i + kk->kl > n)
            continue;
        int ok = 1;
        for (int t = 0; t < kk->kl && ok; t++)
            ok = s[i + t] == rb_pool[kk->ko + t];
        if (!ok)
            continue;
        uint16_t prev = i > 0 ? s[i - 1] : 0;
        uint16_t last = rb_pool[kk->ko + kk->kl - 1];
        uint16_t next = i + kk->kl < n ? s[i + kk->kl] : 0;
        if (is_kanji(s[i]) && is_kanji(prev))
            continue;
        if (is_kanji(last) && is_kanji(next))
            continue;
        return kk;
    }
    return 0;
}

static void jp_render(const uint16_t *s, int n, uint16_t color)
{
    int i = 0;
    while (i < n) {
        const RbKey *hit = key_at(s, n, i);
        if (!hit) {
            int j = i + 1;
            while (j < n && !key_at(s, n, j))
                j++;
            push_text(s + i, j - i, color);
            i = j;
            continue;
        }
        for (int t = 0; t < hit->seg_n; t++) {
            const RbSeg *sg = &rb_segs[hit->seg_off + t];
            if (sg->yl)
                push_ruby(rb_pool + sg->bo, sg->bl, rb_pool + sg->yo, sg->yl, color);
            else
                push_text(rb_pool + sg->bo, sg->bl, color);
        }
        i += hit->kl;
    }
    flush_vline(color);
}

void jp_text_init(void)
{
    line_render = jp_render;
}
