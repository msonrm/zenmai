/* Zenmai 設定 — フェイスボタンの並びだけを覚えておく小さな入れ物（SDL2 ビルド専用）。
 *
 * ★**セーブ（card.h）とは別にする。** セーブは「遊びの続き」で、こちらは「この機体の
 *   ことを覚えた」。混ぜると、セーブを消したらボタンの並びも忘れることになる。
 *
 * ★PS1 版には無い —— PlayStation のパッドは △○✕□ の位置が決まっているので、
 *   覚えることが何も無い（main.c 側も #ifdef ZM_SDL で丸ごと落としている）。
 *
 * 置き場所: 環境変数 ZENMAI_CONF があればそのパス。無ければセーブと同じ設定ディレクトリ
 * （Linux なら ~/.local/share/msonrm/zenmai/zenmai.conf）。
 * ★PortMaster の起動スクリプトは $GAMEDIR/conf の下へ向ける。
 */
#ifndef CONF_H
#define CONF_H

/* 位置の並び。★添字は**位置**（右 / 下 / 上 / 左）で、値は**素の並びでの通し番号**
   （0 = ○ 1 = ✕ 2 = △ 3 = □）。pad_face_order へ渡す形とそのまま対応する。 */
enum { FACE_RIGHT = 0, FACE_BOTTOM, FACE_TOP, FACE_LEFT, FACE_N };

/* 読めたら 1、無ければ 0（＝まだ訊いていない ＝ 初回起動）。
   ★壊れた内容も 0 を返す —— 半端な並びを入れると押しても何も出ないボタンが生まれる。 */
int conf_face_load(int order[FACE_N]);
void conf_face_save(const int order[FACE_N]);

#endif
