#ifndef CARD_H
#define CARD_H

/* PS1 メモリーカード。★枠は 1 つだけ持つ ——
 * 名前やスロットを訊く画面を出すと、コントローラだけで遊べるという芯が壊れる
 * (ブラウザ版 src/glk-shim.js の SLOT と同じ判断)。 */

#define CARD_DATA_MAX (62 * 128 - 2)   /* 1 ブロック - タイトル - アイコン - 長さ */

int card_save(const unsigned char *data, int len);   /* 1 = 成功 */
int card_load(unsigned char *out, int max);          /* 長さ / -1 = 無い・読めない */
int card_have(void);                                 /* セーブが在れば 1 */

#endif
