/* 訳の層(src/translate.js の C 移植)。挙動の正典は JS —— 変更は両方を見ること。
 * 検証: test_translate(ホスト)が walkthrough 全行の入出力対(pairs.jsonl)を照合する。 */
#ifndef TRANSLATE_H
#define TRANSLATE_H

/* 反響({ECHO})に使う「打った語」。NULL で解除。ASCII */
void tr_set_echo(const char *word);
void tr_set_echo16(const unsigned short *word, int n);   /* 打った語がかなのとき */

/* 1 語(名詞句)を訳す(ステータス行の部屋名用)。引けなければ原文を写す。戻り値 = 長さ */
int tr_word_str(const char *en, int len, unsigned short *out, int outmax);

/* 1 行を訳す。raw = VM の生 1 行(ASCII)。
 * 戻り値: out に書いた UTF-16 長 / -1 = 訳せず(呼び出し側が原文を出す)。
 * 素通し(プロンプトだけの行など)は原文を out にコピーして返す。 */
int tr_line(const char *raw, unsigned short *out, int outmax);

#endif
