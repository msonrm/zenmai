/* 入力側(かな → 英語コマンド。src/command.js toCommand の C 移植)。
 * 語彙はビルド時に構築済み(cmd_data)。挙動の正典は JS —— 変更は両方を見ること。 */
/* ★インクルードガードは CMD_H にしない —— render.h の `enum { CMD_H = 24 }`
   (コマンド欄の高さ)と衝突し、**この名前が黙って空マクロになる**。
   実際に踏んだ: zm_dual_main.c で CMD_H を書いたら「式が無い」と言われた。 */
#ifndef ZENMAI_CMD_H
#define ZENMAI_CMD_H

typedef struct {
    int has_command;                   /* 0 = 送らない(note/ask/unknown を見せる) */
    char command[160];                 /* 英語コマンド(ASCII) */
    int command_len;
    unsigned short echo[96];           /* 画面に映す打鍵の代表形(漢字) */
    int echo_len;
    unsigned short note[160];          /* 断り書き(無ければ 0) */
    int note_len;
    unsigned short ask[64];            /* 聞き返し(needs_object のとき) */
    int ask_len;
    unsigned short unknown[8][32];     /* 知らない言葉(かな) */
    int unknown_lens[8];
    int unknown_n;
    char alts[4][96];                  /* 別案コマンド */
    int alts_lens[4];
    int alts_n;
    unsigned short obj_disp[24];       /* 別案の元になった打った言い方 */
    int obj_disp_len;
    unsigned short echo_word[24];      /* 轟音の部屋用: 打った呼び名(かな) */
    int echo_word_len;
    /* ★{SAID} 用: 打った物の表示形(漢字)。原作が入力バッファをそのまま印字する行
       (You can't see any X here! / 目的語の聞き返し)に差し込む。
       echo_word との違いは「音として返すか、字として見せるか」 */
    unsigned short said[24];
    int said_len;
    int needs_object;
    int verb_idx;                      /* 聞き返し中の動詞(次回の pending に渡す) */
    int trace;                         /* CMD_TR_*(検証用) */
} CmdRes;

enum { CMD_TR_EMPTY, CMD_TR_ENGLISH, CMD_TR_PARSER, CMD_TR_YESNO, CMD_TR_NEG,
       CMD_TR_UNKNOWN, CMD_TR_NOUN, CMD_TR_NOVERB, CMD_TR_NOSHAPE, CMD_TR_DIR, CMD_TR_OK };

/* pending_verb = 聞き返し中の動詞 index(無ければ -1) */
void cmd_run(const unsigned short *in, int inlen, int pending_verb, CmdRes *r);

#endif
