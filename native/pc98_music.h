/* PC-98 の曲: 常駐の PMD（梶原正裕さんの Professional Music Driver）に `.M` を渡して鳴らす層と、
 * 「どの部屋でどの曲か」を決める設定（INI）の層。
 *
 * ★曲は本体が鳴らさない。PMD（`PMD86.COM` = 86 ボードの機種・`PMD.COM` = 26K の機種）が
 *   垂直帰線とは別のタイマー割り込みで鳴らすので、こちらは頼むだけ（何かを待つ間に呼び続けなくてよい）。
 *   PMD が常駐していなければ何もしない（曲なしで動く）。
 * ★呼び方: INT 60h（AH = 機能番号）。DOS/4GW の下でも int386 で通る（未フックの割り込みは実モードへ反射する）。
 *   ★曲の置き場（AH=06h）が返す DS は反射で潰れて 0 になるので使わない。置き場は
 *   「INT 60h のベクタのセグメント : 返された DX」（docs/pc98-port-plan.md「段 7」の調べた結果）
 * ★ホスト（PC98_HOST）では PMD を呼ばない（設定の読みと「どの曲か」の判断は同じに動く = 記録で突き合わせる）。
 *
 * ★設定（INI・ASCII・`;` から行末はコメント）:
 *   ZENMAI.INI   [zenmai]  music = on|off（曲の全体の入り切り・既定 on）/ title = 起動画面の曲（既定 CANON.M）
 *   <作品>.INI   [music]   部屋名 = 曲.M     ★作品 = ZORK1.ZMP なら ZORK1.INI（パックの無い story も ZORK2.Z3 → ZORK2.INI）
 *                [picture] 部屋名 = 絵       ★絵はまだ形だけ（割り当てを記録に書くまで）
 *   ★部屋名は状態行の**英語の部屋名**（版 3 は VM が毎回出すので、訳にも VM の中にも依らず、人が読んで書ける）。
 *     大文字小文字は区別しない。末尾の `*` で前方一致（`Forest* = …`）、`*` だけで既定。上から順に見て最初に合ったもの。
 *     曲が `-` なら止める。どれにも合わなければ止める。★同じ曲の部屋どうしを動くときは頭に戻さず続ける
 */
#ifndef PC98_MUSIC_H
#define PC98_MUSIC_H

void music_config(void);               /* ZENMAI.INI を読む（起動画面の前に 1 回） */
const char *music_title_file(void);    /* 起動画面の曲のファイル名（無ければ ""） */
void music_work(const char *base);     /* 作品の INI（<base>.INI）を読む。★作品を選んだあとに 1 回 */
enum { MUSIC_CHANGED = 1, PICTURE_CHANGED = 2 };
int music_room(const char *name, int n);   /* 部屋名 → 曲・絵。替わったほうの印を返す（音は PMD に頼む）。INI が無ければ 0 */
const char *music_current(void);       /* 今の曲（止まっていれば ""） */
const char *picture_current(void);     /* 今の絵（無ければ ""） */

void music_start(const char *file);    /* PMD が常駐していれば `.M` を読んで頭から鳴らす。読めなければ何もしない */
void music_stop(void);                 /* 止める。音は数秒かけて消える（フェードアウト）*/

#endif
