/* GamepadEngine の状態機械の C 移植(labo web/src/gamepad/machine.ts + ops.ts)。
 *
 * 決定的な純核のみ。ボタン読み取り・時間の供給・バッファ管理はホスト側。
 * ゴールデン: labo の 35 件を test_input(ホストビルド)で照合する。
 * ★移植元(TS)と同じ挙動を 2 か所で持つことになるため、直すときは両方を見ること。
 */
#ifndef GP_INPUT_H
#define GP_INPUT_H

enum { GP_CHORD_WINDOW_MS = 300, GP_ENG_LONG_PRESS_MS = 500, GP_ENG_DOUBLE_TAP_MS = 300 };

/* 1 フレーム分の派生入力。vowel: -1 = 未選択 */
typedef struct {
    int now;                 /* ms */
    int row;                 /* 子音行 0..9 */
    int vowel;               /* 母音 0..4 / -1 */
    int vowelNow, ltNow, rtNow;
    int consonantCount;
} GpFrame;

typedef struct {
    int prevVowelPressed, prevRow, prevVowelIndex, prevConsonantCount, prevLT, prevRT;
    int eagerSet, eagerCharLen, eagerTime;
    int rtUsed, rtDuringLT, rtCycleStep;
    int engShiftNext, engCapsLock, engSmartCaps, engLTHolding, engLTPressTime, engLastLTRelease;
} GpMachine;

enum { GPA_KANA, GPA_YOUON };

typedef struct {
    int type;
    unsigned short text[2];  /* GPA_KANA のみ */
    int tlen;
    int replace;
} GpAction;

void gp_init(GpMachine *s);
/* 1 フレームを処理してアクションを out に詰める(最大 4)。戻り値 = 個数。
 * 呼び出し後にホストが gp_sync_prev を呼ぶこと(resolver.stepFrame と同じ順)。 */
int gp_step(GpMachine *s, int english, const GpFrame *f, GpAction out[4]);
void gp_sync_prev(GpMachine *s, const GpFrame *f);
/* 拗音後置シフト / 濁点トグルを合成末尾に解決する。
 * 戻り値 = 出力長(0 = 無反応)。*replace = 末尾から差し替える字数。 */
int gp_resolve_youon(const unsigned short *tail, int n, unsigned short out[2], int *replace);
int gp_resolve_dakuten(const unsigned short *tail, int n, unsigned short out[2], int *replace);
/* RT を「使用済み」にする(リリース時の「ん」を抑止。resolver.consumeRt 相当) */
void gp_consume_rt(GpMachine *s);
/* RT 連打サイクルを断つ(resolver.action 相当 — スティック等の出力前に呼ぶ) */
void gp_break_rt_cycle(GpMachine *s);
/* 行インジケータ用: その行のあ段の字 / 英語は T9 の数字 */
unsigned short gp_row_char(int row);
unsigned short gp_row_char_en(int row);

#endif
