/* GamepadEngine 状態機械の C 移植。挙動は machine.ts / ops.ts と同一(変更しない)。 */
#include "input.h"
#include "tables.h"

/* RT 連打サイクル: 1打=ん, 2打=を, 3打=んを, 4打で ん へ戻る */
static const unsigned short RT_CYCLE[3][2] = {{0x3093, 0}, {0x3092, 0}, {0x3093, 0x3092}};
static const int RT_CYCLE_LEN[3] = {1, 1, 2};

void gp_init(GpMachine *s)
{
    static const GpMachine zero;
    *s = zero;
    s->prevVowelIndex = -1;
}

void gp_sync_prev(GpMachine *s, const GpFrame *f)
{
    s->prevConsonantCount = f->consonantCount;
    s->prevVowelPressed = f->vowelNow;
    s->prevRow = f->row;
    s->prevVowelIndex = f->vowel;
    s->prevLT = f->ltNow;
    s->prevRT = f->rtNow;
}

void gp_consume_rt(GpMachine *s) { s->rtUsed = 1; }
void gp_break_rt_cycle(GpMachine *s) { s->rtCycleStep = 0; }
unsigned short gp_row_char(int row) { return (row >= 0 && row < 10) ? GP_KANA[row][0] : 0x3042; }
unsigned short gp_row_char_en(int row) { return (row >= 0 && row < 10) ? GP_ENG[row][0] : '1'; }

static void push_kana1(GpAction *a, unsigned short c, int replace)
{
    a->type = GPA_KANA;
    a->text[0] = c;
    a->text[1] = 0;
    a->tlen = 1;
    a->replace = replace;
}

static int step_japanese(GpMachine *s, const GpFrame *f, GpAction out[4])
{
    int n = 0;
    int v = f->vowel < 0 ? 0 : f->vowel;

    if (f->vowelNow) {
        unsigned short ch = (f->row >= 0 && f->row < 10) ? GP_KANA[f->row][v] : 0;
        if (ch) {
            int rowChanged = f->row != s->prevRow;
            int vowelChanged = f->vowel != s->prevVowelIndex;
            if (!s->prevVowelPressed) {
                push_kana1(&out[n++], ch, 0);
                s->eagerSet = 1;
                s->eagerCharLen = 1;
                s->eagerTime = f->now;
                s->rtCycleStep = 0;
            } else if (rowChanged || vowelChanged) {
                int consonantReleased = rowChanged && f->consonantCount < s->prevConsonantCount;
                if (!consonantReleased) {
                    int rep = (s->eagerSet && (f->now - s->eagerTime) < GP_CHORD_WINDOW_MS)
                        ? s->eagerCharLen : 0;
                    push_kana1(&out[n++], ch, rep);
                    s->eagerSet = 1;
                    s->eagerCharLen = 1;
                    s->eagerTime = f->now;
                    s->rtCycleStep = 0;
                }
            }
        }
    }
    if (s->prevVowelPressed && !f->vowelNow)
        s->eagerSet = 0;

    /* LT: 拗音後置シフト / LT+RT = っ */
    if (f->ltNow && !s->prevLT)
        s->rtDuringLT = 0;
    if (f->ltNow && f->rtNow) {
        s->rtDuringLT = 1;
        s->rtUsed = 1;
    }
    if (!f->ltNow && s->prevLT) {
        if (s->rtDuringLT)
            push_kana1(&out[n++], 0x3063 /* っ */, 0);
        else
            out[n++].type = GPA_YOUON;
        s->rtDuringLT = 0;
        s->rtCycleStep = 0;
    }

    /* RT 単押し → 連打サイクル */
    if (f->rtNow && !s->prevRT)
        s->rtUsed = 0;
    if (!f->rtNow && s->prevRT) {
        if (!s->rtUsed && !f->ltNow && !s->prevLT) {
            int prevStep = s->rtCycleStep;
            int nextStep = prevStep >= 3 ? 1 : prevStep + 1;
            GpAction *a = &out[n++];
            a->type = GPA_KANA;
            a->text[0] = RT_CYCLE[nextStep - 1][0];
            a->text[1] = RT_CYCLE[nextStep - 1][1];
            a->tlen = RT_CYCLE_LEN[nextStep - 1];
            a->replace = prevStep == 0 ? 0 : RT_CYCLE_LEN[prevStep - 1];
            s->rtCycleStep = nextStep;
        } else {
            s->rtCycleStep = 0;
        }
        s->rtUsed = 0;
    }
    return n;
}

static int is_alpha(unsigned short c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int step_english(GpMachine *s, const GpFrame *f, GpAction out[4])
{
    int n = 0;
    int v = f->vowel < 0 ? 0 : f->vowel;

    if (f->vowelNow) {
        unsigned short ch = (f->row >= 0 && f->row < 10) ? GP_ENG[f->row][v] : 0;
        if (ch) {
            if (s->engCapsLock || s->engSmartCaps || s->engShiftNext) {
                if (ch >= 'a' && ch <= 'z')
                    ch -= 0x20;
                if (s->engShiftNext)
                    s->engShiftNext = 0;
                if (s->engSmartCaps && !is_alpha(ch)) {
                    s->engSmartCaps = 0;
                    s->engCapsLock = 0;
                }
            }
            int rowChanged = f->row != s->prevRow;
            int vowelChanged = f->vowel != s->prevVowelIndex;
            if (!s->prevVowelPressed) {
                push_kana1(&out[n++], ch, 0);
                s->eagerSet = 1;
                s->eagerCharLen = 1;
                s->eagerTime = f->now;
            } else if (rowChanged || vowelChanged) {
                int consonantReleased = rowChanged && f->consonantCount < s->prevConsonantCount;
                if (!consonantReleased) {
                    int rep = (s->eagerSet && (f->now - s->eagerTime) < GP_CHORD_WINDOW_MS)
                        ? s->eagerCharLen : 0;
                    push_kana1(&out[n++], ch, rep);
                    s->eagerSet = 1;
                    s->eagerCharLen = 1;
                    s->eagerTime = f->now;
                }
            }
        }
    }
    if (s->prevVowelPressed && !f->vowelNow)
        s->eagerSet = 0;

    /* LT: 短押し=Shift、長押し=CapsLock、短押し×2=SmartCaps */
    if (f->ltNow && !s->prevLT) {
        s->engLTPressTime = f->now;
        s->engLTHolding = 1;
    }
    if (f->ltNow && s->engLTHolding && (f->now - s->engLTPressTime) >= GP_ENG_LONG_PRESS_MS) {
        s->engCapsLock = !s->engCapsLock;
        s->engSmartCaps = 0;
        s->engShiftNext = 0;
        s->engLTHolding = 0;
    }
    if (!f->ltNow && s->prevLT) {
        if (s->engLTHolding) {
            if ((f->now - s->engLastLTRelease) < GP_ENG_DOUBLE_TAP_MS) {
                s->engSmartCaps = 1;
                s->engShiftNext = 0;
            } else {
                s->engShiftNext = 1;
            }
        }
        s->engLTHolding = 0;
        s->engLastLTRelease = f->now;
    }

    /* RT: "0" */
    if (f->rtNow && !s->prevRT)
        s->rtUsed = 0;
    if (!f->rtNow && s->prevRT) {
        if (!s->rtUsed && !f->ltNow)
            push_kana1(&out[n++], '0', 0);
        s->rtUsed = 0;
    }
    return n;
}

int gp_step(GpMachine *s, int english, const GpFrame *f, GpAction out[4])
{
    return english ? step_english(s, f, out) : step_japanese(s, f, out);
}

/* ---- ops.ts: 合成末尾への解決 ---- */

int gp_resolve_youon(const unsigned short *tail, int n, unsigned short out[2], int *replace)
{
    if (n == 0)
        return 0;
    unsigned short last = tail[n - 1];
    for (int i = 0; i < GP_YOUON_N; i++) {
        if (GP_YOUON[i].from == last) {
            out[0] = GP_YOUON[i].to[0];
            out[1] = GP_YOUON[i].to[1];
            *replace = 1;
            return GP_YOUON[i].to[1] ? 2 : 1;
        }
    }
    out[0] = 0x3063;                   /* 対象外 → っ を追加 */
    *replace = 0;
    return 1;
}

/* 1 字を 清音→濁音→半濁音→清音 のサイクルで次へ。非対象は 0。 */
static unsigned short cycle_dakuten(unsigned short c)
{
    for (int i = 0; i < GP_HANDAKU_N; i++)             /* 半濁音 → 清音 */
        if (GP_HANDAKU[i].on == c)
            return GP_HANDAKU[i].sei;
    for (int i = 0; i < GP_DAKU_N; i++)                /* 濁音 → 半濁音 or 清音 */
        if (GP_DAKU[i].on == c) {
            unsigned short sei = GP_DAKU[i].sei;
            for (int k = 0; k < GP_HANDAKU_N; k++)
                if (GP_HANDAKU[k].sei == sei)
                    return GP_HANDAKU[k].on;
            return sei;
        }
    for (int i = 0; i < GP_DAKU_N; i++)                /* 清音 → 濁音 */
        if (GP_DAKU[i].sei == c)
            return GP_DAKU[i].on;
    return 0;
}

int gp_resolve_dakuten(const unsigned short *tail, int n, unsigned short out[2], int *replace)
{
    if (n == 0)
        return 0;
    unsigned short last = tail[n - 1];
    /* 末尾が小書き ゃゅょ → 直前の基字に適用し、小書きは残して 2 字差し替え */
    if ((last == 0x3083 || last == 0x3085 || last == 0x3087) && n >= 2) {
        unsigned short cycled = cycle_dakuten(tail[n - 2]);
        if (!cycled)
            return 0;
        out[0] = cycled;
        out[1] = last;
        *replace = 2;
        return 2;
    }
    unsigned short cycled = cycle_dakuten(last);
    if (!cycled)
        return 0;
    out[0] = cycled;
    *replace = 1;
    return 1;
}
