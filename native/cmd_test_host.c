/* cmd.c のホストハーネス: 入力(UTF-8・1 行 1 件)→ JSON 行(cmd_ref.js と同形式)。
 *   gcc -std=c11 -O1 -o cmd_test_host cmd_test_host.c cmd.c cmd_data.c
 */
#include <stdio.h>
#include <string.h>
#include "cmd.h"

static int u8_decode(const unsigned char *s, int n, unsigned short *out)
{
    int o = 0;
    for (int i = 0; i < n;) {
        unsigned c = s[i];
        if (c < 0x80) { out[o++] = (unsigned short)c; i++; }
        else if ((c >> 5) == 6) { out[o++] = (unsigned short)(((c & 31) << 6) | (s[i + 1] & 63)); i += 2; }
        else if ((c >> 4) == 14) {
            out[o++] = (unsigned short)(((c & 15) << 12) | ((s[i + 1] & 63) << 6) | (s[i + 2] & 63));
            i += 3;
        } else { i += 4; out[o++] = '?'; }
    }
    return o;
}

static void jstr16(const unsigned short *s, int n)
{
    putchar('"');
    for (int i = 0; i < n; i++) {
        unsigned short c = s[i];
        if (c == '"' || c == '\\') { putchar('\\'); putchar((char)c); }
        else if (c >= 0x20 && c < 0x7F) putchar((char)c);
        else printf("\\u%04x", c);
    }
    putchar('"');
}

static void jstr8(const char *s, int n)
{
    putchar('"');
    for (int i = 0; i < n; i++) {
        char c = s[i];
        if (c == '"' || c == '\\') { putchar('\\'); putchar(c); }
        else putchar(c);
    }
    putchar('"');
}

int main(void)
{
    char line[512];
    while (fgets(line, sizeof line, stdin)) {
        int n = (int)strlen(line);
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) n--;
        if (!n) continue;
        unsigned short in[256];
        int inn = u8_decode((unsigned char *)line, n, in);
        static CmdRes r;
        cmd_run(in, inn, -1, &r);
        /* ★trace も突き合わせる。同じ「送らない」でも**理由が違えば画面の文言が違う**
           —— 実際、宣言した語（NOCMD）を断片（UNKNOWN）と同じ扱いにしていたせいで
           `たきをみる` が「（読み取れなかった）」になった（2026-08-31・実機の指摘）。 */
        static const char *const TR[] = {
            "EMPTY", "ENGLISH", "PARSER", "YESNO", "NEG", "UNKNOWN",
            "NOUN", "NOVERB", "NOSHAPE", "DIR", "OK", "NOCMD",
        };
        printf("{\"trace\":\"%s\",\"command\":",
               (r.trace >= 0 && r.trace < (int)(sizeof TR / sizeof *TR)) ? TR[r.trace] : "?");
        if (r.has_command) jstr8(r.command, r.command_len); else printf("null");
        printf(",\"note\":");
        jstr16(r.note, r.note_len);
        printf(",\"unknown\":[");
        for (int i = 0; i < r.unknown_n; i++) {
            if (i) putchar(',');
            jstr16(r.unknown[i], r.unknown_lens[i]);
        }
        printf("],\"echo\":");
        jstr16(r.echo, r.echo_len);
        printf(",\"alts\":[");
        for (int i = 0; i < r.alts_n; i++) {
            if (i) putchar(',');
            jstr8(r.alts[i], r.alts_lens[i]);
        }
        printf("],\"objDisp\":");
        jstr16(r.obj_disp, r.obj_disp_len);
        printf(",\"echoWord\":");
        if (r.needs_object) printf("\"\"");
        else jstr16(r.echo_word, r.echo_word_len);
        printf(",\"said\":");
        jstr16(r.said, r.said_len);
        printf(",\"needsObject\":%s", r.needs_object ? "true" : "false");
        printf(",\"ask\":");
        if (r.needs_object) jstr16(r.ask, r.ask_len); else printf("\"\"");
        printf("}\n");
    }
    return 0;
}
