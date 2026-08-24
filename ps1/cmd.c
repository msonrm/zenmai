/* かな → 英語コマンド(src/command.js toCommand の C 移植)。 */
#include "cmd.h"
#include "cmd_data.h"

typedef unsigned short u16;

enum { IN_MAX = 128, OBJ_MAX = 12, OUT_W = 24 };

/* ---- 小物 ---- */

static int is_ws(u16 c) { return c == ' ' || c == '\t' || c == 0x3000; }

static u16 kana1(u16 c)
{
    if (c >= 0x30A1 && c <= 0x30F6) c = (u16)(c - 0x60);      /* カタカナ→ひらがな */
    else if (c >= 0xFF01 && c <= 0xFF5E) c = (u16)(c - 0xFEE0); /* 全角英数→半角 */
    if (c == 0xFF0D || c == 0x2014) c = 0x30FC;                /* －— → ー */
    if (c >= 'A' && c <= 'Z') c = (u16)(c + 32);
    return c;
}

static int u16_starts(const u16 *s, int n, int at, const u16 *p, int pl)
{
    if (at + pl > n) return 0;
    for (int i = 0; i < pl; i++)
        if (s[at + i] != p[i]) return 0;
    return 1;
}

static int is_small(u16 c)
{
    switch (c) {
    case 0x3041: case 0x3043: case 0x3045: case 0x3047: case 0x3049:
    case 0x3063: case 0x3083: case 0x3085: case 0x3087: case 0x308E:
    case 0x3095: case 0x3096: case 0x30FC:
        return 1;
    }
    return 0;
}

static void put_j(u16 *dst, int *o, int max, const u16 *s, int n)
{
    for (int i = 0; i < n && *o < max; i++)
        dst[(*o)++] = s[i];
}

static void put_pool(u16 *dst, int *o, int max, unsigned int off, int n)
{
    put_j(dst, o, max, cm_jpool + off, n);
}

static void put_frag(u16 *dst, int *o, int max, int fi)
{
    put_pool(dst, o, max, cm_frags[fi].off, cm_frags[fi].len);
}

/* ---- 語彙 ---- */

static const CmLex *word_at(const u16 *s, int n, int at)
{
    for (int i = 0; i < CM_LEX_N; i++) {
        const CmLex *e = &cm_lex[i];
        if (!u16_starts(s, n, at, cm_jpool + e->kn, e->knl))
            continue;
        if (at + e->knl < n && is_small(s[at + e->knl]))
            continue;
        return e;
    }
    return 0;
}

static int map_find(const CmMap *tab, int n, const u16 *s, int sn)
{
    for (int i = 0; i < n; i++)
        if (tab[i].kl == sn && u16_starts(s, sn, 0, cm_jpool + tab[i].ko, tab[i].kl))
            return i;
    return -1;
}

static int has_tok(const CmVerb *v, int tok) { return (v->mask & tok) != 0; }

static int role_tok(int role)
{
    switch (role) {
    case CMR_IN: return CMT_IN;
    case CMR_ON: return CMT_ON;
    case CMR_UNDER: return CMT_UNDER;
    case CMR_BEHIND: return CMT_BEHIND;
    case CMR_FROM: return CMT_FROM;
    case CMR_TO: return CMT_TO;
    case CMR_WITH: return CMT_WITH;
    }
    return 0;
}

static int is_spatial(int role)
{
    return role == CMR_IN || role == CMR_ON || role == CMR_UNDER ||
           role == CMR_BEHIND || role == CMR_FROM;
}

/* jpool 部分文字列に needle(u16 1 字)が含まれるか */
static int jhas_char(unsigned int off, int len, u16 c)
{
    for (int i = 0; i < len; i++)
        if (cm_jpool[off + i] == c) return 1;
    return 0;
}

static int jhas_sub(unsigned int off, int len, const u16 *p, int pl)
{
    for (int i = 0; i + pl <= len; i++) {
        int ok = 1;
        for (int t = 0; t < pl && ok; t++)
            ok = cm_jpool[off + i + t] == p[t];
        if (ok) return 1;
    }
    return 0;
}

/* ---- 本体 ---- */

typedef struct {
    const CmLex *e;
    int role;                          /* CMR_* / CMR_NONE */
    unsigned int jo; int jl;           /* 打った形(ja) */
    unsigned int dp; int dpl;          /* 代表形(disp) */
    unsigned int wo; int wl;           /* 英単語 */
    int vehicle, is_all;
    unsigned short ooff, on;
} Tok;

static void unknown_push(CmdRes *r, const u16 *s, int n)
{
    if (r->unknown_n >= 8 || n <= 0) return;
    int m = n > 32 ? 32 : n;
    for (int i = 0; i < m; i++)
        r->unknown[r->unknown_n][i] = s[i];
    r->unknown_lens[r->unknown_n] = m;
    r->unknown_n++;
}

static void note_guide(CmdRes *r)
{
    for (int i = 0; i < r->unknown_n; i++) {
        u16 k[32];
        for (int t = 0; t < r->unknown_lens[i]; t++)
            k[t] = kana1(r->unknown[i][t]);
        int g = map_find(cm_guide, CM_GUIDE_N, k, r->unknown_lens[i]);
        if (g >= 0) {
            r->note_len = 0;
            put_pool(r->note, &r->note_len, 160, cm_guide[g].vo, cm_guide[g].vl);
            return;
        }
    }
}

void cmd_run(const u16 *in, int inlen, int pending_verb, CmdRes *r)
{
    static const u16 NEG1[] = {0x306A, 0x3044};                          /* ない */
    static const u16 NEG2[] = {0x306A, 0x3044, 0x3067};                  /* ないで */
    static const u16 NEG3[] = {0x306C};                                  /* ぬ */
    static const u16 NEG4[] = {0x307E, 0x305B, 0x3093};                  /* ません */
    static const u16 NEG5[] = {0x306A, 0x304B, 0x3063, 0x305F};          /* なかった */

    for (int i = 0; i < (int)sizeof(*r) / 4; i++)
        ((int *)r)[i] = 0;
    r->verb_idx = -1;

    /* strip: 句読点と空白を除く(原文の字種は保つ) */
    u16 raw[IN_MAX];
    int rn = 0;
    for (int i = 0; i < inlen && rn < IN_MAX; i++) {
        u16 c = in[i];
        if (c == 0x3002 || c == 0x3001 || c == 0xFF0E || c == 0xFF0C ||
            c == 0xFF01 || c == 0xFF1F || is_ws(c))
            continue;
        raw[rn++] = c;
    }
    if (rn == 0) { r->trace = CMD_TR_EMPTY; return; }

    /* 英語のまま(trim して全部 ASCII 印字) */
    {
        int a = 0, b = inlen;
        while (a < b && is_ws(in[a])) a++;
        while (b > a && is_ws(in[b - 1])) b--;
        int all_ascii = b > a;
        for (int i = a; i < b && all_ascii; i++)
            all_ascii = in[i] >= 0x20 && in[i] <= 0x7E;
        if (all_ascii) {
            for (int i = a; i < b && r->command_len < 159; i++)
                r->command[r->command_len++] = (char)in[i];
            r->command[r->command_len] = 0;
            r->has_command = 1;
            put_j(r->echo, &r->echo_len, 96, in + a, b - a);
            r->trace = CMD_TR_ENGLISH;
            return;
        }
    }

    u16 kraw[IN_MAX];
    for (int i = 0; i < rn; i++)
        kraw[i] = kana1(raw[i]);

    /* パーサの語 / はい・いいえ(raw か kana(raw) の完全一致) */
    {
        int m = map_find(cm_pwords, CM_PW_N, raw, rn);
        if (m < 0) m = map_find(cm_pwords, CM_PW_N, kraw, rn);
        if (m >= 0) {
            for (int i = 0; i < cm_pwords[m].vl && r->command_len < 159; i++)
                r->command[r->command_len++] = cm_apool[cm_pwords[m].vo + i];
            r->command[r->command_len] = 0;
            r->has_command = 1;
            put_j(r->echo, &r->echo_len, 96, raw, rn);
            r->trace = CMD_TR_PARSER;
            return;
        }
        m = map_find(cm_yesno, CM_YN_N, raw, rn);
        if (m < 0) m = map_find(cm_yesno, CM_YN_N, kraw, rn);
        if (m >= 0) {
            for (int i = 0; i < cm_yesno[m].vl && r->command_len < 159; i++)
                r->command[r->command_len++] = cm_apool[cm_yesno[m].vo + i];
            r->command[r->command_len] = 0;
            r->has_command = 1;
            put_j(r->echo, &r->echo_len, 96, raw, rn);
            r->trace = CMD_TR_YESNO;
            return;
        }
    }

    /* 否定は扱えない */
    {
        int neg = (rn >= 2 && u16_starts(raw, rn, rn - 2, NEG1, 2)) ||
                  (rn >= 3 && u16_starts(raw, rn, rn - 3, NEG2, 3)) ||
                  (rn >= 1 && u16_starts(raw, rn, rn - 1, NEG3, 1)) ||
                  (rn >= 3 && u16_starts(raw, rn, rn - 3, NEG4, 3)) ||
                  (rn >= 4 && u16_starts(raw, rn, rn - 4, NEG5, 4));
        if (neg) {
            put_j(r->echo, &r->echo_len, 96, raw, rn);
            r->trace = CMD_TR_NEG;
            return;
        }
    }

    const u16 *s = kraw;
    int n = rn;
    Tok found[24];
    int nf = 0;
    int hard_stop = 0;
    int i = 0;
    while (i < n) {
        const CmLex *hit = word_at(s, n, i);
        if (hit) {
            i += hit->knl;
            put_pool(r->echo, &r->echo_len, 96, hit->dp, hit->dpl);
            int role = CMR_NONE;
            for (int p = 0; p < CM_PART_N; p++) {
                const CmPart *pt = &cm_parts[p];
                if (!u16_starts(s, n, i, cm_jpool + pt->po, pt->pl))
                    continue;
                int next = i + pt->pl;
                int goes = next >= n || word_at(s, n, next);
                for (int q = 0; q < CM_PART_N && !goes; q++)
                    goes = u16_starts(s, n, next, cm_jpool + cm_parts[q].po, cm_parts[q].pl);
                if (!goes)
                    continue;
                role = pt->role;
                i = next;
                put_pool(r->echo, &r->echo_len, 96, pt->dp, pt->dpl);
                if (is_spatial(role) && i < n && (s[i] == 0x306B || s[i] == 0x3078)) {
                    put_j(r->echo, &r->echo_len, 96, s + i, 1);
                    i++;
                }
                break;
            }
            if (nf < 24) {
                Tok *t = &found[nf++];
                t->e = hit;
                t->role = role;
                t->jo = hit->jo; t->jl = hit->jl;
                t->dp = hit->dp; t->dpl = hit->dpl;
                t->wo = hit->wo; t->wl = hit->wl;
                t->vehicle = hit->vehicle;
                t->is_all = hit->is_all;
                t->ooff = hit->ooff;
                t->on = hit->on_;
            }
            continue;
        }
        int j = i + 1;
        while (j < n && !word_at(s, n, j)) j++;
        /* 未知語。を/は/が を背負っていたら「物のつもり」= 送らない */
        int cut = -1;
        for (int t = i; t < j && cut < 0; t++)
            if (s[t] == 0x3092 || s[t] == 0x306F || s[t] == 0x304C) cut = t;
        if (cut < 0) {
            if (j > i) unknown_push(r, s + i, j - i);
        } else {
            if (cut > i) { unknown_push(r, s + i, cut - i); hard_stop = 1; }
            if (cut + 1 < j) unknown_push(r, s + cut + 1, j - cut - 1);
        }
        put_j(r->echo, &r->echo_len, 96, s + i, j - i);
        i = j;
    }

    if (hard_stop) {
        note_guide(r);
        r->trace = CMD_TR_UNKNOWN;
        return;
    }

    /* 動詞・方角・物 */
    Tok *verb = 0;
    Tok pverb;
    for (int t = 0; t < nf && !verb; t++)
        if (found[t].e->kind == CMK_VERB) verb = &found[t];
    if (!verb && pending_verb >= 0 && pending_verb < CM_VERB_N) {
        const CmVerb *v = &cm_verbs[pending_verb];
        pverb.e = 0;
        pverb.role = CMR_NONE;
        pverb.jo = v->jo; pverb.jl = v->jl;
        pverb.dp = v->jo; pverb.dpl = v->jl;
        pverb.wo = v->ko; pverb.wl = v->kl;
        pverb.vehicle = 0; pverb.is_all = 0; pverb.ooff = 0; pverb.on = 0;
        verb = &pverb;
    }
    int vidx = verb ? (verb->e ? verb->e->vidx : pending_verb) : -1;
    const CmVerb *vv = vidx >= 0 ? &cm_verbs[vidx] : 0;

    Tok *dirs[8];
    int nd = 0;
    Tok *objs[OBJ_MAX];
    int no = 0;
    for (int t = 0; t < nf; t++) {
        if (found[t].e->kind == CMK_DIR && nd < 8) dirs[nd++] = &found[t];
        if (found[t].e->kind == CMK_OBJ && no < OBJ_MAX) objs[no++] = &found[t];
    }

    /* 方角だけ(WALK / 物なしの CLIMB・DISEMBARK も) */
    if (nd && (!verb || vidx == VK_WALK ||
               ((vidx == VK_CLIMB || vidx == VK_DISEMBARK) && !no))) {
        for (int t = 0; t < dirs[0]->wl && r->command_len < 159; t++)
            r->command[r->command_len++] = cm_apool[dirs[0]->wo + t];
        r->command[r->command_len] = 0;
        r->has_command = 1;
        r->trace = CMD_TR_DIR;
        return;
    }
    /* 中/外は ENTER/EXIT/LEAVE の動詞に含まれる */
    Tok *bare_dirs[8];
    int nbd = 0;
    for (int t = 0; t < nd; t++) {
        int inout = dirs[t]->wl == 2
            ? cm_apool[dirs[t]->wo] == 'i' && cm_apool[dirs[t]->wo + 1] == 'n'
            : dirs[t]->wl == 3 && cm_apool[dirs[t]->wo] == 'o';
        if (verb && (vidx == VK_ENTER || vidx == VK_EXIT || vidx == VK_LEAVE) && inout)
            continue;
        bare_dirs[nbd++] = dirs[t];
    }

    if (!verb) {
        if (r->unknown_n) {
            note_guide(r);
            r->trace = CMD_TR_UNKNOWN;
            return;
        }
        if (no) {
            for (int t = 0; t < objs[0]->wl && r->command_len < 159; t++)
                r->command[r->command_len++] = cm_apool[objs[0]->wo + t];
            r->command[r->command_len] = 0;
            r->has_command = 1;
            r->trace = CMD_TR_NOUN;
            for (int a = 0; a < objs[0]->on && r->alts_n < 4; a++) {
                const CmStr *w = &cm_others[objs[0]->ooff + a];
                for (int t = 0; t < w->len && t < 95; t++)
                    r->alts[r->alts_n][t] = cm_apool[w->off + t];
                r->alts_lens[r->alts_n] = w->len < 95 ? w->len : 95;
                r->alts_n++;
            }
            put_pool(r->obj_disp, &r->obj_disp_len, 24, objs[0]->dp, objs[0]->dpl);
            return;
        }
        r->trace = CMD_TR_NOVERB;
        return;
    }

    /* 「家の扉」の家は修飾語(後ろに名詞が続くときだけ外す) */
    {
        int w = 0;
        for (int t = 0; t < no; t++)
            if (!(objs[t]->role == CMR_MOD && t + 1 < no))
                objs[w++] = objs[t];
        no = w;
    }

    Tok *prso = 0;
    for (int t = 0; t < no && !prso; t++) if (objs[t]->role == CMR_O) prso = objs[t];
    for (int t = 0; t < no && !prso; t++) if (objs[t]->role == CMR_NONE) prso = objs[t];
    for (int t = 0; t < no && !prso; t++) if (objs[t]->role == CMR_MOD) prso = objs[t];
    Tok *tool = 0;
    for (int t = 0; t < no && !tool; t++) if (objs[t]->role == CMR_WITH) tool = objs[t];
    Tok *dest = 0;
    for (int t = 0; t < no && !dest; t++) {
        int ro = objs[t]->role;
        if (ro == CMR_TO || ro == CMR_IN || ro == CMR_ON || ro == CMR_UNDER ||
            ro == CMR_BEHIND || ro == CMR_FROM)
            dest = objs[t];
    }
    if (!prso && dest && no == 1) prso = dest;

    /* 「〜と〜以外」は連鎖して除く側へ */
    for (int t = no - 1; t > 0; t--)
        if (objs[t]->role == CMR_EXCEPT && objs[t - 1]->role == CMR_AND)
            objs[t - 1]->role = CMR_EXCEPT;
    Tok *ands[OBJ_MAX];
    int na = 0;
    Tok *excepts[OBJ_MAX];
    int ne = 0;
    for (int t = 0; t < no; t++) {
        if (objs[t]->role == CMR_AND && objs[t] != prso && na < OBJ_MAX) ands[na++] = objs[t];
        if (objs[t]->role == CMR_EXCEPT && ne < OBJ_MAX) excepts[ne++] = objs[t];
    }
    Tok all_tok;
    if (!prso && ne) {
        /* 「◯◯以外をとる」= 残り全部 */
        const CmLex *alx = &cm_lex[CM_ALL_LEX];   /* ぜんぶ(ALL_WORDS[0]) */
        all_tok.e = alx;
        all_tok.role = CMR_NONE;
        all_tok.jo = alx->jo; all_tok.jl = alx->jl;
        all_tok.dp = alx->dp; all_tok.dpl = alx->dpl;
        all_tok.wo = alx->wo; all_tok.wl = alx->wl;
        all_tok.vehicle = 0; all_tok.is_all = 1; all_tok.ooff = 0; all_tok.on = 0;
        prso = &all_tok;
    }

    /* 降りる: 乗り物でなければ climb down */
    int fixed_down = 0;
    if (vidx == VK_DISEMBARK && prso && !prso->vehicle) {
        vidx = VK_CLIMB;
        vv = &cm_verbs[vidx];
        fixed_down = 1;
    }

    /* out の組み立て(単語列) */
    const char *ow[16];
    int owl[16], now_ = 0;
    static char wordbuf[16][OUT_W];
    #define PUSH_W(off, len) do { \
        int L = (len) < OUT_W - 1 ? (len) : OUT_W - 1; \
        for (int _t = 0; _t < L; _t++) wordbuf[now_][_t] = cm_apool[(off) + _t]; \
        ow[now_] = wordbuf[now_]; owl[now_] = L; now_++; } while (0)
    #define PUSH_S(lit) do { ow[now_] = (lit); owl[now_] = (int)sizeof(lit) - 1; now_++; } while (0)

    PUSH_W(vv->ko, vv->kl);
    if (fixed_down && has_tok(vv, CMT_DOWN)) PUSH_S("down");

    /* 動詞が取れない空間の役 → 断る */
    for (int t = 0; t < no; t++) {
        Tok *o = objs[t];
        if (!is_spatial(o->role) || has_tok(vv, role_tok(o->role)))
            continue;
        if (o->role == CMR_FROM && vidx == VK_ENTER && has_tok(vv, CMT_OBJ))
            continue;
        r->note_len = 0;
        put_frag(r->note, &r->note_len, 160, 0);              /* 「 */
        put_pool(r->note, &r->note_len, 160, o->dp, o->dpl);
        put_pool(r->note, &r->note_len, 160, cm_role_ja[o->role].off, cm_role_ja[o->role].len);
        put_frag(r->note, &r->note_len, 160, 1);              /* 」を「 */
        put_pool(r->note, &r->note_len, 160, verb->dp, verb->dpl);
        put_frag(r->note, &r->note_len, 160, 2);              /* 」では受けられない */
        if (vidx == VK_WALK || vidx == VK_ENTER || vidx == VK_EXIT)
            put_frag(r->note, &r->note_len, 160, 3);          /* 移動は方角で */
        r->trace = CMD_TR_NOSHAPE;
        return;
    }
    if (tool && !has_tok(vv, CMT_WITH)) {
        r->note_len = 0;
        put_frag(r->note, &r->note_len, 160, 0);              /* 「 */
        put_pool(r->note, &r->note_len, 160, verb->dp, verb->dpl);
        put_frag(r->note, &r->note_len, 160, 4);              /* 」に道具（ */
        put_pool(r->note, &r->note_len, 160, tool->dp, tool->dpl);
        put_frag(r->note, &r->note_len, 160, 5);              /* で）は付けられない */
        r->trace = CMD_TR_NOSHAPE;
        return;
    }

    int prso_at = -1, tool_at = -1, dest_at = -1;
    static const u16 NAKA[] = {0x4E2D};
    static const u16 NAKA2[] = {0x306A, 0x304B};
    static const u16 NOZO[] = {0x306E, 0x305E};
    if (prso && !(dest == prso && has_tok(vv, role_tok(dest->role)))) {
        if (!vv->bare_ok) {
            int wants_in = jhas_sub(verb->jo, verb->jl, NAKA, 1) ||
                           jhas_sub(verb->jo, verb->jl, NAKA2, 2) ||
                           jhas_sub(verb->jo, verb->jl, NOZO, 2);
            static const int W1[] = {CMT_IN, CMT_AT, 0};
            static const int W2[] = {CMT_AT, CMT_IN, CMT_ON, CMT_UNDER, CMT_TO, 0};
            static const char *const WN1[] = {"in", "at"};
            static const char *const WN2[] = {"at", "in", "on", "under", "to"};
            const int *wl_ = wants_in ? W1 : W2;
            const char *const *wn = wants_in ? WN1 : WN2;
            for (int t = 0; wl_[t]; t++)
                if (has_tok(vv, wl_[t])) {
                    ow[now_] = wn[t];
                    owl[now_] = 0;
                    while (wn[t][owl[now_]]) owl[now_]++;
                    now_++;
                    break;
                }
        }
        for (int t = 0; t < na; t++) {
            PUSH_W(ands[t]->wo, ands[t]->wl);
            PUSH_S("and");
        }
        prso_at = now_;
        PUSH_W(prso->wo, prso->wl);
    }
    if (tool) {
        PUSH_S("with");
        tool_at = now_;
        PUSH_W(tool->wo, tool->wl);
    }
    if (dest && dest != prso) {
        if (has_tok(vv, role_tok(dest->role))) {
            switch (dest->role) {
            case CMR_TO: PUSH_S("to"); break;
            case CMR_IN: PUSH_S("in"); break;
            case CMR_ON: PUSH_S("on"); break;
            case CMR_UNDER: PUSH_S("under"); break;
            case CMR_BEHIND: PUSH_S("behind"); break;
            default: PUSH_S("from"); break;
            }
        } else {
            /* ★「に」(CMR_TO) を持たない動詞では、同じ「に」が指しうる役から選ぶ。
               AT が先（`throw bag at troll`。in だと原作が「場所がない。」と断る） */
            if (has_tok(vv, CMT_AT)) PUSH_S("at");
            else if (has_tok(vv, CMT_IN)) PUSH_S("in");
            else if (has_tok(vv, CMT_ON)) PUSH_S("on");
            else PUSH_S("to");
        }
        dest_at = now_;
        PUSH_W(dest->wo, dest->wl);
    } else if (dest && dest == prso && has_tok(vv, role_tok(dest->role))) {
        now_ = 1;                        /* 動詞だけ残して作り直し */
        switch (dest->role) {
        case CMR_TO: PUSH_S("to"); break;
        case CMR_IN: PUSH_S("in"); break;
        case CMR_ON: PUSH_S("on"); break;
        case CMR_UNDER: PUSH_S("under"); break;
        case CMR_BEHIND: PUSH_S("behind"); break;
        default: PUSH_S("from"); break;
        }
        prso_at = now_;
        PUSH_W(dest->wo, dest->wl);
    }
    if (!prso && !tool && !dest && nbd)
        PUSH_W(bare_dirs[0]->wo, bare_dirs[0]->wl);
    if (ne) {
        PUSH_S("except");
        for (int t = 0; t < ne; t++) {
            if (t) PUSH_S("and");
            PUSH_W(excepts[t]->wo, excepts[t]->wl);
        }
    }

    /* 別案(others を同じ位置に差し替え) */
    Tok *alt_src = 0;
    int alt_at = -1;
    if (prso_at >= 0 && prso->on) { alt_src = prso; alt_at = prso_at; }
    else if (tool_at >= 0 && tool && tool->on) { alt_src = tool; alt_at = tool_at; }
    else if (dest_at >= 0 && dest && dest->on) { alt_src = dest; alt_at = dest_at; }
    if (alt_src) {
        for (int a = 0; a < alt_src->on && r->alts_n < 4; a++) {
            const CmStr *w = &cm_others[alt_src->ooff + a];
            int o = 0;
            for (int t = 0; t < now_; t++) {
                if (t) r->alts[r->alts_n][o++] = ' ';
                if (t == alt_at) {
                    for (int q = 0; q < w->len && o < 95; q++)
                        r->alts[r->alts_n][o++] = cm_apool[w->off + q];
                } else {
                    for (int q = 0; q < owl[t] && o < 95; q++)
                        r->alts[r->alts_n][o++] = ow[t][q];
                }
            }
            r->alts_lens[r->alts_n] = o;
            r->alts_n++;
        }
        put_pool(r->obj_disp, &r->obj_disp_len, 24, alt_src->dp, alt_src->dpl);
    }

    int needs = !vv->bare_ok2 && !prso && !tool && !dest && !nbd;
    if (needs && r->unknown_n) {
        note_guide(r);
        r->trace = CMD_TR_UNKNOWN;
        return;
    }
    if (needs) {
        /* 聞き返し: 「何の◯を◯？」or「何を◯？」 */
        int cut = -1;
        for (int t = 1; t + 1 < verb->jl; t++)
            if (cm_jpool[verb->jo + t] == 0x3092) { cut = t; break; }
        r->ask_len = 0;
        if (cut > 0) {
            put_frag(r->ask, &r->ask_len, 64, 6);              /* 何の */
            put_pool(r->ask, &r->ask_len, 64, verb->jo, cut);
            put_frag(r->ask, &r->ask_len, 64, 8);              /* を */
            put_pool(r->ask, &r->ask_len, 64, verb->jo + cut + 1, verb->jl - cut - 1);
            put_frag(r->ask, &r->ask_len, 64, 9);              /* ？ */
        } else {
            put_frag(r->ask, &r->ask_len, 64, 7);              /* 何を */
            int dcut = -1;
            for (int t = 0; t < verb->dpl; t++)
                if (cm_jpool[verb->dp + t] == 0x3092) dcut = t;
            if (dcut >= 0)
                put_pool(r->ask, &r->ask_len, 64, verb->dp + dcut + 1, verb->dpl - dcut - 1);
            else
                put_pool(r->ask, &r->ask_len, 64, verb->dp, verb->dpl);
            put_frag(r->ask, &r->ask_len, 64, 9);              /* ？ */
        }
        r->needs_object = 1;
        r->verb_idx = vidx;
        /* JS は needsObject でも command を作って返すが、ホストは送らない。
         * 比較のため command も作る */
    }

    for (int t = 0; t < now_; t++) {
        if (t && r->command_len < 159) r->command[r->command_len++] = ' ';
        for (int q = 0; q < owl[t] && r->command_len < 159; q++)
            r->command[r->command_len++] = ow[t][q];
    }
    r->command[r->command_len] = 0;
    /* ★「およぐ」だけは目的語を添えて送る（JS の command.js と同じ手当て）。
       原作の V-SWIM は目的語なしのとき空の物に D ,PRSO を呼び、**文字化けを印字する**。
       水の無い場所では外れるので、素の swim を別案に添える。 */
    if (vidx == VK_SWIM && !prso && !tool && !dest && !nbd) {
        for (int t = 0; t < r->command_len && t < 95; t++)
            r->alts[r->alts_n][t] = r->command[t];
        r->alts_lens[r->alts_n] = r->command_len < 95 ? r->command_len : 95;
        r->alts_n++;
        static const char tail[] = " in water";
        for (int t = 0; tail[t] && r->command_len < 159; t++)
            r->command[r->command_len++] = tail[t];
        r->command[r->command_len] = 0;
    }
    r->has_command = 1;
    r->trace = CMD_TR_OK;

    /* 轟音の部屋: 打った呼び名 */
    if (prso && prso->jl)
        put_pool(r->echo_word, &r->echo_word_len, 24, prso->jo, prso->jl);
    else
        put_pool(r->echo_word, &r->echo_word_len, 24, verb->jo, verb->jl);
    r->verb_idx = vidx;
}
