/* PC-98 の画面の装い（色）と、場面ごとの替え方（INI）。
 *
 * ★色は 2 種類:
 *   - 背景・縁・帯 = グラフィックのパレット（4096 色）。INI では `#RGB`（16 進 3 桁・例 `#237`）。
 *     パレットを書き換えるだけなので、場面が替わっても再描画は要らない。
 *   - 文字 = テキスト画面の固定 8 色。INI では名前（black blue red magenta green cyan yellow white）。
 * ★設定（INI・ASCII・`;` から行末はコメント）:
 *   ZENMAI.INI / <作品>.INI   [theme]  キー = 値        全体の既定（作品の INI が後から上書きする）
 *   <作品>.INI                [scene]  部屋名 = キー=値 キー=値 …   場面ごとに替えるもの（部屋名の照合は曲の INI と同じ）
 *   背景など（#RGB）: band 上の帯 / top 本文の上の細い帯 / side 本文の左右 / input 入力欄 / body 本文の地 / ruby ふりがな
 *   文字（名前）:     status 上の帯の場所 / score 上の帯の得点 / prompt 入力欄の ＞ / input_fg コマンド文字（キャレットと ▼ も同じ色）
 *   ★全体だけ（[theme] のみ。履歴の行が色を持つので場面で替えると混ざる）: text 本文 / echo 打ったコマンドの反響
 *   ★キーが無い・値が読めないときは既定（今までの色）のまま。
 */
#ifndef PC98_THEME_H
#define PC98_THEME_H

enum {
    TH_BAND, TH_TOP, TH_SIDE, TH_INPUT, TH_BODY, TH_RUBY,                  /* 色（#RGB・0x0RGB） */
    TH_STATUS, TH_SCORE, TH_PROMPT, TH_INPUT_FG, TH_TEXT, TH_ECHO,         /* 文字（テキストの属性） */
    TH_N, TH_FG0 = TH_STATUS
};

/* パレットの番号（4 = キャレット・▼ = コマンド文字色 / 8 = ふりがな）*/
enum { PAL_BODY = 0, PAL_BAND = 1, PAL_SIDE = 2, PAL_INPUT = 3, PAL_CARET = 4, PAL_TOP = 5, PAL_RUBY = 8 };

void theme_config(void);               /* ZENMAI.INI の [theme] を読む（起動画面の前に 1 回） */
void theme_work(const char *base);     /* <作品>.INI の [theme] と [scene] を読む。★作品を選んだあとに 1 回 */
int  theme_room(const char *name, int n);   /* 状態行の英語の部屋名 → 場面の装い。替わったら 1 */
void theme_apply(void);                /* 今の装いをパレットに書く */
int  theme_attr(int key);              /* 文字の属性（TH_FG0 以降） */
int  theme_rgb(int key);               /* 背景などの色（0x0RGB） */
void theme_describe(char *buf, int n); /* 今の装いの 1 行（記録用） */

#endif
