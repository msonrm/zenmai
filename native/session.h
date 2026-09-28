/* Zenmai の芯 —— Z-machine（MojoZork）を抱え、打った言葉を VM に渡し、出力を訳して本文へ流す。
 *
 * ★**全部の版（PS1 / SDL / PC-98）がこれを共有する。** もとは main.c の中にあり、
 *   PC-98 版の段 1 では写しを持っていた（2026-09-28 に抜き出した）。
 *   ★写しは「片方だけ直る」ので、VM に触る道はここ 1 本にする。
 *
 * 流す先は render.h の文字列の口（draw_plain / draw_echo / hist_blank）だけ。
 * 画素を知らないので、PC-98 のテキスト画面も同じ口を実装すれば載る（render_pc98.c）。
 * セーブの置き場は card.h（PS1 = card.c / SDL = save_file.c / PC-98 = save_dos.c）。
 *
 * ★画面側の前後（遡り中なら下端へ跳ぶ・状態行・新しい内容を見せる送り・入力欄を空にする）は
 *   呼ぶ側の仕事。ここは「打った → 本文に積んだ」までで止まる。
 */
#ifndef SESSION_H
#define SESSION_H
#include <stdint.h>

/* VM を用意して最初の入力待ちまで回し、そこまでの出力を流す。
 * en = 1 なら訳さずに英語のまま流す。die = VM が止まったときに呼ぶ（戻ってきたら固まる）。
 * ★story は初期イメージとして持ち続ける（セーブの差分の相手） */
void sess_start(int en, const uint8_t *story, uint32_t len, void (*die)(const char *msg));

int sess_quit(void);                   /* 原作の QUIT が通ったら 1 */
const char *sess_status(void);         /* Z-machine の状態行（49 桁・部屋名と得点の間は空白 2 つ以上） */

/* 英語面: 打ったとおりを反響し、VM に渡して出力を流す */
void sess_submit_en(const uint16_t *typed, int n);

/* 日本語面: cmd_run で英語のコマンドに落とし、反響し、VM に渡して出力を流す。
 * pending_verb = 聞き返し中の動詞（無ければ -1。ここが書き換える）。
 * 戻り値 1 = VM を回さなかった（読み取れなかった・「何を?」と訊き返した）。
 * ★呼ぶ側は 0 のときだけ状態行を描き直す（切り出す前と同じ間合い） */
int sess_submit_ja(const uint16_t *typed, int n, int *pending_verb);

#endif
