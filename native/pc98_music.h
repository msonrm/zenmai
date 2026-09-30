/* PC-98 の曲: 常駐の PMD（梶原正裕さんの Professional Music Driver）に `.M` を渡して鳴らす層。
 *
 * ★曲は本体が鳴らさない。PMD（`PMD86.COM` = 86 ボードの機種・`PMD.COM` = 26K の機種）が
 *   垂直帰線とは別のタイマー割り込みで鳴らすので、こちらは頼むだけ（何かを待つ間に呼び続けなくてよい）。
 *   PMD が常駐していなければ何もしない（曲なしで動く）。
 * ★呼び方: INT 60h（AH = 機能番号）。DOS/4GW の下でも int386 で通る（未フックの割り込みは実モードへ反射する）。
 *   ★曲の置き場（AH=06h）が返す DS は反射で潰れて 0 になるので使わない。置き場は
 *   「INT 60h のベクタのセグメント : 返された DX」（docs/pc98-port-plan.md「段 7」の調べた結果）
 * ★ホスト（PC98_HOST）では何もしない。
 */
#ifndef PC98_MUSIC_H
#define PC98_MUSIC_H

void music_start(const char *file);    /* PMD が常駐していれば `.M` を読んで頭から鳴らす。読めなければ何もしない */
void music_stop(void);                 /* 止める（起動画面を抜けるとき）。音は数秒かけて消える（フェードアウト）*/

#endif
