/* MojoZork 埋め込みのホスト検証ハーネス。
 *
 *   gcc -std=gnu11 -O1 -w -I. -Ivendor -o zm_host_test zm_host_test.c
 *   ./zm_host_test ../vendor/zork1/zork1.z3 "open mailbox" "read leaflet"
 *
 * ★フラグは 3 つとも要る。`-Ivendor` が無いと `#include "mojozork.c"` を見つけられず、
 *   `-std=c11` だと strdup が宣言されず(新しい gcc は警告でなくエラー)、`-w` が無いと
 *   vendor 側の警告で埋まる。★**PS1 を焼かずに Zork の実挙動を数十秒で確かめられる**ので、
 *   「この語は打てるのか」「この書き方は通るのか」はここで当たるのが速い。
 *
 * multizorkd と同じ埋め込みの型:
 *   - main を改名して mojozork.c を include(素の getObjectPtr 等をそのまま使う)
 *   - initStory(メモリバッファ)→ opcode 表の read/save/restore を差し替え
 *   - read で step_completed=1 → ホストへ戻る → 入力を詰めて tokenizeUserInput → 再開
 * 出力は writestr で貯め、ターンごとに --- 区切りで印字する(ZVM との突き合わせ用)。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define main mojozork_main_unused
#include "mojozork.c"
#undef main

static char outbuf[65536];
static int outlen;

static void host_writestr(const char *str, const uintptr slen)
{
    if (outlen + (int)slen < (int)sizeof outbuf) {
        memcpy(outbuf + outlen, str, slen);
        outlen += (int)slen;
    }
}

static uint8 *pend_input;
static uint8 pend_inputlen;
static uint16 pend_operands[2];

static void host_read(void)
{
    updateStatusBar();
    uint8 *input = GState->story + GState->operands[0];
    const uint8 inputlen = *(input++);
    pend_input = input;
    pend_inputlen = inputlen;
    pend_operands[0] = GState->operands[0];
    pend_operands[1] = GState->operands[1];
    GState->step_completed = 1;
}

static void host_save(void) { GState->step_completed = 1; }   /* 未対応(検証では使わない) */
static void host_restore(void) { GState->step_completed = 1; }

static char statusbuf[80];
static ZMachineState zm;

__attribute__((noreturn)) static void host_die(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "DIE: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

static void run_until_read(void)
{
    GState->step_completed = 0;
    while (!GState->step_completed && !GState->quit)
        runInstruction();
}

static void feed(const char *cmd)
{
    snprintf((char *)pend_input, pend_inputlen - 1, "%s", cmd);
    for (char *p = (char *)pend_input; *p; p++) {
        if (*p >= 'A' && *p <= 'Z')
            *p = 'a' + (*p - 'A');
        else if ((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9')
                 || strchr(" .,!?_#'\"/\\-:()", *p))
            ;
        else
            *p = ' ';
    }
    GState->operands[0] = pend_operands[0];
    GState->operands[1] = pend_operands[1];
    GState->operand_count = 2;
    pend_input = NULL;
    tokenizeUserInput();
}

int main(int argc, char **argv)
{
    FILE *io = fopen(argv[1], "rb");
    if (!io) { fprintf(stderr, "story が開けない\n"); return 1; }
    static uint8 story[128 * 1024];
    const uint32 len = (uint32)fread(story, 1, sizeof story, io);
    fclose(io);

    GState = &zm;                      /* 本家 main の肩代わり */
    GState->die = host_die;
    GState->writestr = host_writestr;
    initStory(argv[1], story, len);
    GState->status_bar = statusbuf;
    GState->status_bar_len = sizeof statusbuf;
    GState->status_bar_enabled = 1;
    GState->story[1] &= ~(1 << 4);     /* ステータス行あり、とゲームに教える */
    GState->opcodes[181].fn = host_save;
    GState->opcodes[182].fn = host_restore;
    GState->opcodes[228].fn = host_read;
    for (uint8 i = 32; i <= 127; i++) GState->opcodes[i] = GState->opcodes[i % 32];
    for (uint8 i = 144; i <= 175; i++) GState->opcodes[i] = GState->opcodes[128 + (i % 16)];
    for (uint8 i = 192; i <= 223; i++) GState->opcodes[i] = GState->opcodes[i % 32];
    GState->writestr = host_writestr;

    run_until_read();
    printf("=== STATUS ===\n%s\n=== TURN 0 ===\n%.*s\n", statusbuf, outlen, outbuf);

    for (int i = 2; i < argc && !GState->quit; i++) {
        outlen = 0;
        feed(argv[i]);
        run_until_read();
        printf("=== STATUS ===\n%s\n=== TURN %d: %s ===\n%.*s\n",
               statusbuf, i - 1, argv[i], outlen, outbuf);
    }
    return 0;
}
