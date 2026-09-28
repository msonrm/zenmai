/* キーボードのかな入力（kana_input.c）のホスト検査。
 *   cc -std=c11 -Wall test_kana_input.c kana_input.c kana_input_data.c -o test_kana_input && ./test_kana_input
 * ★打鍵の列を流して、入力欄に確定した字（UTF-8 で比べる）を見る。test-pc98.sh が最初に流す。
 */
#include <stdio.h>
#include <string.h>
#include "kana_input.h"

static void to_utf8(const uint16_t *s, int n, char *out)
{
    int o = 0;
    for (int i = 0; i < n; i++) {
        const unsigned u = s[i];
        if (u < 0x80) {
            out[o++] = (char)u;
        } else if (u < 0x800) {
            out[o++] = (char)(0xC0 | u >> 6);
            out[o++] = (char)(0x80 | (u & 0x3F));
        } else {
            out[o++] = (char)(0xE0 | u >> 12);
            out[o++] = (char)(0x80 | (u >> 6 & 0x3F));
            out[o++] = (char)(0x80 | (u & 0x3F));
        }
    }
    out[o] = 0;
}

static int fails;

/* keys: 打鍵の列。'\b' = BS。english: 英字（CAPS）。commit: 最後に Enter の前の片づけをするか */
static void check(const char *keys, int english, int commit, const char *want, const char *want_pend)
{
    KiLine l;
    ki_clear(&l);
    for (const unsigned char *p = (const unsigned char *)keys; *p; p++) {
        if (*p == '\b') ki_backspace(&l);
        else ki_key(&l, *p, english);
    }
    if (commit) ki_commit(&l);
    char got[256];
    to_utf8(l.buf, l.n, got);
    char pend[16];
    memcpy(pend, l.pend, (size_t)l.np);
    pend[l.np] = 0;
    if (strcmp(got, want) || strcmp(pend, want_pend)) {
        printf("NG [%s]%s → 「%s」+「%s」（期待「%s」+「%s」）\n",
               keys, english ? "（英字）" : "", got, pend, want, want_pend);
        fails++;
    }
}

int main(void)
{
    /* ローマ字 */
    check("yuubinbakowoakeru", 0, 1, "ゆうびんばこをあける", "");
    check("kitte", 0, 1, "きって", "");
    check("konnnichiha", 0, 1, "こんにちは", "");
    check("konnichiha", 0, 1, "こんいちは", "");          /* ★Mozc の表どおり: nn で ん が決まる */
    check("shinbun", 0, 1, "しんぶん", "");
    check("shinbun", 0, 0, "しんぶ", "n");                 /* 末尾の n は確定まで待つ */
    check("kyaxtultutcha", 0, 1, "きゃっっっちゃ", "");
    check("n'ana", 0, 1, "んあな", "");
    check("si-ru", 0, 1, "しーる", "");
    check("ki,ka.", 0, 1, "き、か。", "");
    check("jinja", 0, 1, "じんじゃ", "");
    check("fa", 0, 1, "ふぁ", "");
    check("vu", 0, 1, "ヴ", "");                           /* ★ゔ は漢字 ROM に無いので ヴ */
    check("k", 0, 0, "", "k");
    check("kk", 0, 0, "っ", "k");
    check("ka\b", 0, 1, "", "");
    check("k\b", 0, 1, "", "");
    check("ky\ba", 0, 1, "か", "");
    check("ki 2", 0, 1, "き 2", "");
    check("kz", 0, 1, "kz", "");                           /* 表に無い並びは ASCII のまま */
    /* 英字: 表を通さない。大文字は英字の扱いでなくても素通し */
    check("open mailbox", 1, 1, "open mailbox", "");
    check("OPEN", 0, 1, "OPEN", "");
    check("naOK", 0, 1, "なOK", "");
    /* カナキー（半角カナ） */
    check("\xB6\xDE\xCA\xDF\xB3\xDE\xDD\xB0", 0, 1, "がぱヴんー", "");
    check("\xB1\xDE", 0, 1, "あ゛", "");                   /* かぶせられない濁点は単独形 */
    check("k\xB6", 0, 1, "kか", "");                       /* カナキーの前にローマ字の途中を片づける */
    /* 入力欄の長さで止まる */
    check("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 0, 1, "ああああああああああああああああああああああああああああああああああああ", "");

    if (fails) {
        printf("かな入力: %d 件ちがう\n", fails);
        return 1;
    }
    printf("かな入力: 全件一致\n");
    return 0;
}
