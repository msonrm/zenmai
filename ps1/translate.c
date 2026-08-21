/* 訳の層(src/translate.js の C 移植)。
 *
 * 照合は行単位。テンプレートはビルド時に「文字列と穴」へ落ちている(translate_data)。
 * 穴の照合 = 次のリテラルの最左出現を探す(JS の怠惰量指定子と同値)。
 * 通常の穴は文末記号 [.!?] を含まない・1 字以上。引用の穴は ["] 以外・0 字以上。
 */
#include "translate.h"
#include "translate_data.h"

typedef unsigned short u16;
typedef unsigned int u32;

enum { KEY_MAX = 1024, CAP_MAX = 4, OUT_MAX = 2048 };

static u16 echo_word[32];
static int echo_len;

void tr_set_echo(const char *word)
{
    echo_len = 0;
    if (!word)
        return;
    while (word[echo_len] && echo_len < 31) {
        echo_word[echo_len] = (u16)(unsigned char)word[echo_len];
        echo_len++;
    }
}

void tr_set_echo16(const unsigned short *word, int n)
{
    echo_len = 0;
    if (!word)
        return;
    while (echo_len < n && echo_len < 31) {
        echo_word[echo_len] = word[echo_len];
        echo_len++;
    }
}

/* ---- 小物 ---- */

static int is_sp(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

/* 空白の連続を 1 つの空白へ潰し、前後を刈る(JS の norm) */
static int norm_str(const char *s, int n, char *dst)
{
    int o = 0;
    int i = 0;
    while (i < n) {
        if (is_sp(s[i])) {
            while (i < n && is_sp(s[i]))
                i++;
            if (o > 0 && i < n)
                dst[o++] = ' ';
        } else {
            dst[o++] = s[i++];
        }
    }
    dst[o] = '\0';
    return o;
}

static int str_len(const char *s)
{
    int n = 0;
    while (s[n]) n++;
    return n;
}

static int bytes_eq(const char *a, const char *b, int n)
{
    for (int i = 0; i < n; i++)
        if (a[i] != b[i]) return 0;
    return 1;
}

/* (len, bytes) の辞書式比較で二分探索 */
static const TrPair *find_pair(const TrPair *tab, int n, const char *key, int klen)
{
    int lo = 0, hi = n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const char *e = tr_en_pool + tab[mid].eo;
        int el = tab[mid].el;
        int m = el < klen ? el : klen;
        int c = 0;
        for (int i = 0; i < m && !c; i++)
            c = (unsigned char)e[i] - (unsigned char)key[i];
        if (!c)
            c = el - klen;
        if (c == 0)
            return &tab[mid];
        if (c < 0) lo = mid + 1; else hi = mid - 1;
    }
    return 0;
}

static int put_ja(u16 *out, int o, int outmax, const TrPair *p)
{
    for (int i = 0; i < p->jl && o < outmax; i++)
        out[o++] = tr_ja_pool[p->jo + i];
    return o;
}

static int put_ascii(u16 *out, int o, int outmax, const char *s, int n)
{
    for (int i = 0; i < n && o < outmax; i++)
        out[o++] = (u16)(unsigned char)s[i];
    return o;
}

/* ---- word(): 1 語(名詞句・並び)を日本語へ ---- */

static const TrPair *tr_one(const char *k, int kl)
{
    const TrPair *p;
    if ((p = find_pair(tr_props, TR_PROPS_N, k, kl))) return p;
    if ((p = find_pair(tr_exact, TR_EXACT_N, k, kl))) return p;
    if ((p = find_pair(tr_words, TR_WORDS_N, k, kl))) return p;
    /* 冠詞を剥がして再試行 */
    int skip = 0;
    if (kl > 2 && (k[0] == 'a' || k[0] == 'A') && k[1] == ' ') skip = 2;
    else if (kl > 3 && (k[0] == 'a' || k[0] == 'A') && (k[1] == 'n' || k[1] == 'N') && k[2] == ' ') skip = 3;
    else if (kl > 4 && (k[0] == 't' || k[0] == 'T') && (k[1] == 'h' || k[1] == 'H')
             && (k[2] == 'e' || k[2] == 'E') && k[3] == ' ') skip = 4;
    if (skip) {
        const char *b = k + skip;
        int bl = kl - skip;
        if ((p = find_pair(tr_props, TR_PROPS_N, b, bl))) return p;
        if ((p = find_pair(tr_exact, TR_EXACT_N, b, bl))) return p;
        if ((p = find_pair(tr_words, TR_WORDS_N, b, bl))) return p;
    }
    return 0;
}

static const char *const PREPS[] = {
    "with", "in", "on", "at", "to", "from", "under", "behind", "over", "through", "around", 0
};

static int lower_eq(const char *s, int n, const char *word)
{
    int i = 0;
    for (; i < n; i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') c += 32;
        if (!word[i] || c != word[i]) return 0;
    }
    return word[i] == 0;
}

/* en(生のキャプチャ)→ 日本語。引けなければ en をそのまま(JS word() と同じ) */
static int tr_word(const char *en, int en_len, u16 *out, int o, int outmax)
{
    char k[KEY_MAX];
    int kl = norm_str(en, en_len, k);
    const TrPair *p = tr_one(k, kl);
    if (p)
        return put_ja(out, o, outmax, p);

    /* 前置詞つきの句(with the sword)→ 訳語 + 助詞 */
    int sp = 0;
    while (sp < kl && k[sp] != ' ') sp++;
    if (sp < kl) {
        for (int i = 0; PREPS[i]; i++) {
            if (lower_eq(k, sp, PREPS[i])) {
                const TrPair *obj = tr_one(k + sp + 1, kl - sp - 1);
                const TrPair *par = find_pair(tr_words, TR_WORDS_N, PREPS[i], str_len(PREPS[i]));
                if (obj && par) {
                    o = put_ja(out, o, outmax, obj);
                    return put_ja(out, o, outmax, par);
                }
                break;
            }
        }
    }

    /* 並び(a, b, and c)→ 全部引けたときだけ「と」で繋ぐ */
    {
        int parts[8][2];
        int np = 0;
        int i = 0;
        while (i < kl && np < 8) {
            int start = i;
            int end = -1;
            for (int j = i; j < kl; j++) {
                /* ", and " / " and " / ", " の最初の出現 */
                if (k[j] == ',' ) {
                    int t = j + 1;
                    while (t < kl && k[t] == ' ') t++;
                    if (t + 3 < kl && bytes_eq(k + t, "and ", 4)) { end = j; i = t + 4; }
                    else if (t < kl) { end = j; i = t; }
                    else { end = j; i = kl; }
                    break;
                }
                if (k[j] == ' ' && j + 4 < kl && bytes_eq(k + j, " and ", 5)) {
                    end = j;
                    i = j + 5;
                    break;
                }
            }
            if (end < 0) { end = kl; i = kl; }
            /* trim */
            int a = start, b = end;
            while (a < b && k[a] == ' ') a++;
            while (b > a && k[b - 1] == ' ') b--;
            if (b > a) { parts[np][0] = a; parts[np][1] = b - a; np++; }
        }
        if (np > 1) {
            const TrPair *ps[8];
            int ok = 1;
            for (int t = 0; t < np && ok; t++)
                ok = (ps[t] = tr_one(k + parts[t][0], parts[t][1])) != 0;
            if (ok) {
                for (int t = 0; t < np; t++) {
                    if (t) { if (o < outmax) out[o++] = 0x3068; /* と */ }
                    o = put_ja(out, o, outmax, ps[t]);
                }
                return o;
            }
        }
    }
    /* 引けなかった → 原文のまま */
    return put_ascii(out, o, outmax, en, en_len);
}

/* ---- パターン照合(文字列と穴) ---- */

static int is_term(char c) { return c == '.' || c == '!' || c == '?'; }

/* key に pat を当てる。成功なら 1、captures に (off,len) を入れる */
static int match_pat(const TrPat *pat, const char *key, int klen,
                     int cap_off[CAP_MAX], int cap_len[CAP_MAX])
{
    int pos = 0, ncap = 0;
    const TrSeg *segs = tr_segs + pat->seg_off;
    for (int si = 0; si < pat->en_n; si++) {
        const TrSeg *sg = &segs[si];
        if (sg->kind == TRK_LIT) {
            if (pos + sg->len > klen || !bytes_eq(key + pos, tr_en_pool + sg->off, sg->len))
                return 0;
            pos += sg->len;
        } else {
            /* 穴: 次のリテラルの最左出現まで(無ければ行末まで) */
            int quoted = sg->kind == TRK_QHOLE;
            const TrSeg *nx = 0;
            int nx_i = -1;
            for (int t = si + 1; t < pat->en_n; t++)
                if (segs[t].kind == TRK_LIT) { nx = &segs[t]; nx_i = t; break; }
            /* ★区切りが空白 1 個で、その先も穴なら**最右**を採る。最左だと左の穴が
               1 語で止まり、残りが右の穴へ流れ込む(`the nasty knives in?` が
               OBJ=`nasty` / PREP=`knives in` に割れて `何knives innastyを入れる？`)。
               空白は語の中にも現れるので、区切りとして位置を決められない。
               ※ JS 側(src/translate.js)は同じ形を正規表現の貪欲/非貪欲で書いてある。
                 ただし向こうはバックトラックするのでこちらより粘る —— 最右の空白で
                 右の穴が空になる骨格があれば、そこだけ挙動が割れる(原作には無い) */
            int rightmost = nx && nx->len == 1 && tr_en_pool[nx->off] == ' '
                            && nx_i + 1 < pat->en_n && segs[nx_i + 1].kind != TRK_LIT;
            int minlen = quoted ? 0 : 1;
            int start = pos;
            int end = -1;
            if (nx) {
                if (rightmost) {
                    for (int e = klen - nx->len; e >= start + minlen; e--)
                        if (bytes_eq(key + e, tr_en_pool + nx->off, nx->len)) { end = e; break; }
                } else {
                    for (int e = start + minlen; e + nx->len <= klen; e++) {
                        if (bytes_eq(key + e, tr_en_pool + nx->off, nx->len)) { end = e; break; }
                        /* 穴に入れられない字が来たら打ち切り */
                        char c = key[e];
                        if (quoted ? (c == '"') : is_term(c))
                            return 0;
                    }
                }
                if (end < 0)
                    return 0;
            } else {
                end = klen;                    /* 行末まで */
            }
            for (int e = start; e < end; e++) {
                char c = key[e];
                if (quoted ? (c == '"') : is_term(c))
                    return 0;
            }
            if (end - start < minlen)
                return 0;
            if (ncap < CAP_MAX) {
                cap_off[ncap] = start;
                cap_len[ncap] = end - start;
            }
            ncap++;
            /* 次のリテラルを消費(あれば) */
            if (nx) {
                pos = end + nx->len;
                si = (int)(nx - segs);          /* nx まで飛ぶ */
            } else {
                pos = end;
            }
        }
    }
    return pos == klen;
}

static int subst_ja(const TrPat *pat, const char *key,
                    const int cap_off[CAP_MAX], const int cap_len[CAP_MAX],
                    int force_echo, u16 *out, int o, int outmax)
{
    const TrSeg *segs = tr_segs + pat->seg_off;
    /* EN 穴のフラグを控える(capture index 順) */
    unsigned char hflags[CAP_MAX];
    unsigned char hquoted[CAP_MAX];
    int hn = 0;
    for (int si = 0; si < pat->en_n; si++)
        if (segs[si].kind != TRK_LIT && hn < CAP_MAX) {
            hflags[hn] = segs[si].slot;
            hquoted[hn] = segs[si].kind == TRK_QHOLE;
            hn++;
        }
    for (int si = pat->en_n; si < pat->en_n + pat->ja_n; si++) {
        const TrSeg *sg = &segs[si];
        if (sg->kind == TRK_JLIT) {
            for (int i = 0; i < sg->len && o < outmax; i++)
                out[o++] = tr_ja_pool[sg->off + i];
        } else {
            int ci = sg->slot;
            if (force_echo || ((hflags[ci] & TRF_ECHO) && echo_len)) {
                for (int i = 0; i < echo_len && o < outmax; i++)
                    out[o++] = echo_word[i];
            }
            else if (hquoted[ci] && !(hflags[ci] & TRF_VERB))
                o = put_ascii(out, o, outmax, key + cap_off[ci], cap_len[ci]);
            else
                o = tr_word(key + cap_off[ci], cap_len[ci], out, o, outmax);
        }
    }
    return o;
}

/* ---- lookup(): 1 単位を引く。-1 = 引けず ---- */

static int tr_lookup(const char *key, int klen, u16 *out, int outmax)
{
    int cap_off[CAP_MAX], cap_len[CAP_MAX];
    /* 反響は完全一致より先(「はんきょう」と打った人に「こだま」を返さない) */
    if (echo_len) {
        for (int pi = 0; pi < TR_PATS_N; pi++) {
            if (!tr_pats[pi].has_echo)
                continue;
            if (match_pat(&tr_pats[pi], key, klen, cap_off, cap_len))
                return subst_ja(&tr_pats[pi], key, cap_off, cap_len, 1, out, 0, outmax);
        }
    }
    const TrPair *p = find_pair(tr_exact, TR_EXACT_N, key, klen);
    if (!p)
        p = find_pair(tr_props, TR_PROPS_N, key, klen);
    if (p)
        return put_ja(out, 0, outmax, p);
    for (int pi = 0; pi < TR_PATS_N; pi++)
        if (match_pat(&tr_pats[pi], key, klen, cap_off, cap_len))
            return subst_ja(&tr_pats[pi], key, cap_off, cap_len, 0, out, 0, outmax);
    return -1;
}

/* ---- greedy(): 行の中を前方から貪欲に食う ---- */

static int tr_greedy(const char *key, int klen, u16 *out, int outmax)
{
    /* 文の切れ目で分割: [^.!?]*[.!?]+["')\]]*\s* | 残り */
    int po[24], pl[24];
    int np = 0;
    int i = 0;
    while (i < klen && np < 24) {
        int s = i;
        while (i < klen && !is_term(key[i])) i++;
        if (i < klen) {
            while (i < klen && is_term(key[i])) i++;
            while (i < klen && (key[i] == '"' || key[i] == '\'' || key[i] == ')' || key[i] == ']')) i++;
            while (i < klen && key[i] == ' ') i++;
        }
        po[np] = s;
        pl[np] = i - s;
        np++;
    }
    if (np < 2)
        return -1;
    int o = 0, hit = 0;
    char cand[KEY_MAX];
    int idx = 0;
    while (idx < np) {
        int ja_len = -1, span = 0;
        for (int j = np; j > idx; j--) {
            int tot = po[j - 1] + pl[j - 1] - po[idx];
            int cl = norm_str(key + po[idx], tot, cand);
            static u16 tmp[OUT_MAX];
            int r = tr_lookup(cand, cl, tmp, OUT_MAX);
            if (r >= 0) {
                for (int t = 0; t < r && o < outmax; t++)
                    out[o++] = tmp[t];
                ja_len = r;
                span = j - idx;
                break;
            }
        }
        if (ja_len < 0) {
            /* 引けない部分は trim して原文 */
            int a = po[idx], b = po[idx] + pl[idx];
            while (a < b && is_sp(key[a])) a++;
            while (b > a && is_sp(key[b - 1])) b--;
            o = put_ascii(out, o, outmax, key + a, b - a);
            idx++;
        } else {
            hit = 1;
            idx += span;
        }
    }
    return hit ? o : -1;
}

/* ---- line() ---- */

static int tr_line_inner(const char *raw, int rn, u16 *out, int outmax)
{
    /* 行頭のプロンプト(\s*>+\s*)を剥がして再帰 */
    {
        int i = 0;
        while (i < rn && is_sp(raw[i])) i++;
        int gs = i;
        while (i < rn && raw[i] == '>') i++;
        if (i > gs) {
            while (i < rn && is_sp(raw[i])) i++;
            int rest_nonempty = 0;
            for (int t = i; t < rn; t++)
                if (!is_sp(raw[t])) { rest_nonempty = 1; break; }
            if (rest_nonempty) {
                static u16 tmpL[OUT_MAX];   /* 行頭枝専用(再帰の別枝と共有しない) */
                u16 *tmp = tmpL;
                int r = tr_line_inner(raw + i, rn - i, tmp, OUT_MAX);
                if (r < 0)
                    return -1;
                int o = put_ascii(out, 0, outmax, raw, i);
                for (int t = 0; t < r && o < outmax; t++)
                    out[o++] = tmp[t];
                return o;
            }
        }
    }
    /* 行末のプロンプト */
    {
        int e = rn;
        while (e > 0 && is_sp(raw[e - 1])) e--;
        int ge = e;
        while (e > 0 && raw[e - 1] == '>') e--;
        if (e < ge) {
            while (e > 0 && is_sp(raw[e - 1])) e--;
            int head_nonempty = 0;
            for (int t = 0; t < e; t++)
                if (!is_sp(raw[t])) { head_nonempty = 1; break; }
            if (head_nonempty) {
                static u16 tmpT[OUT_MAX];   /* 行末枝専用 */
                u16 *tmp = tmpT;
                int r = tr_line_inner(raw, e, tmp, OUT_MAX);
                if (r < 0)
                    return -1;
                int o = 0;
                for (int t = 0; t < r && o < outmax; t++)
                    out[o++] = tmp[t];
                return put_ascii(out, o, outmax, raw + e, rn - e);
            }
        }
    }

    static char key[KEY_MAX];
    int klen = norm_str(raw, rn, key);
    /* 空 or プロンプトだけの行は素通し(原文を返す) */
    {
        int only = 1;
        for (int i = 0; i < klen; i++)
            if (key[i] != '>') { only = 0; break; }
        if (klen == 0 || only)
            return put_ascii(out, 0, outmax, raw, rn);
    }
    /* 字下げ: 半角 2 つ → 全角 1 つ(0.5 は切り上げ) */
    int sp = 0;
    while (sp < rn && (raw[sp] == ' ' || raw[sp] == '\t')) sp++;
    int ind = (sp + 1) / 2;
    int o = 0;
    for (int i = 0; i < ind && o < outmax; i++)
        out[o++] = 0x3000;             /* 全角空白 */

    static u16 tmp[OUT_MAX];
    int r = tr_lookup(key, klen, tmp, OUT_MAX);
    if (r < 0)
        r = tr_greedy(key, klen, tmp, OUT_MAX);
    if (r < 0) {
        /* 複数対象(名前: 本文)。両方引けたときだけ成立 */
        int col = -1;
        for (int i = 0; i + 1 < klen; i++)
            if (key[i] == ':' && key[i + 1] == ' ') { col = i; break; }
        if (col > 0 && col + 2 < klen) {
            const TrPair *name = tr_one(key, col);
            if (name) {
                int b = tr_lookup(key + col + 2, klen - col - 2, tmp, OUT_MAX);
                if (b >= 0) {
                    o = put_ja(out, o, outmax, name);
                    if (o < outmax) out[o++] = 0xFF1A;  /* ： */
                    for (int t = 0; t < b && o < outmax; t++)
                        out[o++] = tmp[t];
                    return o;
                }
            }
        }
        return -1;                     /* notrans も未訳も「原文のまま」(呼び出し側) */
    }
    for (int t = 0; t < r && o < outmax; t++)
        out[o++] = tmp[t];
    return o;
}

int tr_line(const char *raw, u16 *out, int outmax)
{
    return tr_line_inner(raw, str_len(raw), out, outmax);
}

int tr_word_str(const char *en, int len, u16 *out, int outmax)
{
    return tr_word(en, len, out, 0, outmax);
}
