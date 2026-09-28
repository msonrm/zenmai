/* PC-98 の FM 音源。規則は pc98_fm.h。曲は canon_data.c（gen_canon.py が作る）。
 *
 * OPN のレジスタ（1 声 = ch 0〜2）:
 *   30h〜 DT/MUL・40h〜 TL・50h〜 KS/AR・60h〜 DR・70h〜 SR・80h〜 SL/RR・90h〜 SSG-EG
 *   （演算子の並びは番地の順で OP1・OP3・OP2・OP4 = +0・+4・+8・+0Ch）
 *   A4h/A0h 周波数（A4h を先に書く）・B0h FB/ALG・B4h 左右（OPNA だけ。26K では空振り）・28h キーオン
 */
#include "pc98_fm.h"

#ifdef PC98_HOST
void music_start(void) { }
void music_poll(void) { }
void music_stop(void) { }
#else
#include <conio.h>
#include <stdint.h>
#include "canon_data.h"

enum { P_ADDR = 0x188, P_DATA = 0x18A };

/* ★速さ: 二分音符 = 60（6a の指定は 80。起動画面なので落ち着かせた）。
   32 分音符 = 0.0625 秒 = 垂直帰線（56.4247 Hz）3.53 回ぶん。×1024 の固定小数で持ち、ずれを溜めない */
enum { UNIT_FX = 3611, GAP = 1 };      /* GAP = 次の音の前に離す帰線の数（音の切れ目） */

static int present, playing;

static void opn_wait(void)
{
    int n = 4000;
    while ((inp(P_ADDR) & 0x80) && --n) { }     /* bit7 = BUSY */
}

static void opn(int reg, int val)
{
    opn_wait();
    outp(P_ADDR, reg);
    for (int i = 0; i < 4; i++) (void)inp(P_ADDR);   /* 番地を書いた後の待ち（ISA の読み 1 回 ≒ 1µs）*/
    outp(P_DATA, val);
    opn_wait();
}

/* 音色。演算子は OP1〜OP4 の順（番地の順ではない）。値: DT, MUL, TL, KS, AR, DR, SR, SL, RR。
   ★どれも ALG 4 = (OP1→OP2) + (OP3→OP4)。OP2・OP4 が音を出し、OP1・OP3 が音色を作る。DT は 1〜3 が上・5〜7 が下 */
typedef struct { uint8_t alg, fb, op[4][9]; } Patch;
enum { FLUTE, HARPSICHORD, STRINGS, REED, EPIANO, SYNBASS };
static const Patch patches[] = {        /* ★並びは上の enum と同じ（Watcom 1.9 は指定初期化子が怪しいので並びで書く）*/
    /* フルート: ほぼ正弦波・ゆっくり立ち上がる。OP4 は 1 オクターブ上をうすく */
    { 4, 3, { { 0, 1, 38, 0, 22, 0, 0, 0, 6 }, { 0, 1, 18, 0, 20, 2, 0, 1, 6 },
              { 0, 2, 60, 0, 24, 0, 0, 0, 6 }, { 1, 2, 38, 0, 18, 2, 0, 1, 6 } } },
    /* チェンバロ: 弾いて減衰する明るい音 */
    { 4, 6, { { 0, 4, 32, 2, 31, 11, 4, 6, 8 }, { 0, 1, 14, 1, 31, 7, 3, 4, 8 },
              { 3, 1, 26, 2, 31, 12, 5, 7, 8 }, { 7, 2, 22, 1, 31, 7, 3, 4, 8 } } },
    /* ストリングス: 帰還で鋸歯状波に寄せ、2 つの組を上下にずらして厚くする */
    { 4, 7, { { 0, 1, 24, 0, 18, 0, 0, 0, 7 }, { 3, 1, 16, 0, 16, 1, 0, 1, 6 },
              { 0, 1, 26, 0, 18, 0, 0, 0, 7 }, { 7, 1, 18, 0, 16, 1, 0, 1, 6 } } },
    /* ファゴット風のリード: 3 倍の成分で鼻にかかった音 */
    { 4, 5, { { 0, 1, 28, 0, 24, 4, 0, 2, 7 }, { 0, 1, 14, 0, 22, 2, 0, 1, 7 },
              { 0, 3, 40, 0, 26, 6, 0, 3, 7 }, { 3, 1, 24, 0, 22, 2, 0, 1, 7 } } },
    /* エレピ（DX 系）: 14 倍の成分が打鍵の「チン」 */
    { 4, 0, { { 0, 14, 52, 2, 31, 14, 6, 15, 8 }, { 3, 1, 14, 1, 31, 5, 2, 3, 6 },
              { 0, 1, 30, 1, 31, 8, 3, 6, 6 }, { 7, 1, 16, 1, 31, 5, 2, 3, 6 } } },
    /* シンセベース: 半分の成分で太く、すぐ減衰する */
    { 4, 6, { { 0, 0, 24, 0, 31, 8, 2, 4, 8 }, { 0, 1, 12, 0, 31, 4, 1, 2, 8 },
              { 0, 1, 22, 0, 31, 10, 3, 5, 8 }, { 3, 1, 18, 0, 31, 4, 1, 2, 8 } } },
};
/* ★2 声の組（先の声, 後の声）。音色の違いで追いかけっこを聞き分けやすくする */
#ifndef FM_PAIR
#define FM_PAIR 2                      /* ★C = エレピ × シンセベース（聴き比べて msonrm が選んだ・2026-09-29）*/
#endif
static const uint8_t pairs[][2] = {
    { FLUTE, HARPSICHORD },            /* 0: バロックの定番 */
    { STRINGS, REED },                 /* 1: 弦と木管（浄書のヴィオラとチェロに近い） */
    { EPIANO, SYNBASS },               /* 2: 80 年代の PC-98 らしい音 */
};
static const uint8_t slot[4] = { 0, 8, 4, 12 };      /* OP1〜OP4 → 番地の足し分 */

/* ★音量の足し分: 音を出す演算子（ALG 4 では OP2・OP4）の TL をこれだけ下げる（1 段 = 0.75 dB）。
   音色の表は音色だけを決め、全体の大きさはここで決める */
#ifndef FM_LOUD
#define FM_LOUD 12                     /* 約 +9 dB。QuuBee でピーク 27299 / 32767（割れない）。0 では小さかった（msonrm・東方や SuperDepth と比べて）*/
#endif

static void set_patch(int ch, const Patch *p)
{
    for (int o = 0; o < 4; o++) {
        const uint8_t *v = p->op[o];
        const int r = slot[o] + ch;
        const int tl = (o == 1 || o == 3) && v[2] > FM_LOUD ? v[2] - FM_LOUD : (o == 1 || o == 3) ? 0 : v[2];
        opn(0x30 + r, v[0] << 4 | v[1]);
        opn(0x40 + r, tl);
        opn(0x50 + r, v[3] << 6 | v[4]);
        opn(0x60 + r, v[5]);
        opn(0x70 + r, v[6]);
        opn(0x80 + r, v[7] << 4 | v[8]);
        opn(0x90 + r, 0);
    }
    opn(0xB0 + ch, p->fb << 3 | p->alg);
    opn(0xB4 + ch, 0xC0);              /* 左右とも（OPNA）*/
}

/* F ナンバー（C〜B）。★音の高さ = F ナンバー × 2^(ブロック−1) × fM ÷ (72 × 2^20)、fM = 3.9936 MHz
   （OPN の FM は fM/72 の刻みで回る）。618 はブロック 4 で C4。★「144」の式は OPNA を 8 MHz で見たときのもので、
   それで作ると 1 オクターブ上に出る（QuuBee で録って周波数を測って気付いた・2026-09-29） */
static const uint16_t fnum[12] = { 618, 655, 694, 735, 779, 825, 874, 926, 981, 1040, 1102, 1167 };

static void set_freq(int ch, int note)
{
    const int f = fnum[note % 12], blk = note / 12 - 1;    /* C4 = 60 → ブロック 4 */
    opn(0xA4 + ch, blk << 3 | f >> 8);
    opn(0xA0 + ch, f & 0xFF);
}

static void key_on(int ch, int note)
{
    opn(0x28, ch);                     /* 前の音を離してから */
    set_freq(ch, note);
    opn(0x28, 0xF0 | ch);
}

/* ---- 曲を進める ---- */
typedef struct {
    const uint8_t (*ev)[2];
    int n, i, on;
    unsigned long pos;                 /* 次の音の頭（32 分音符の数・頭から数える） */
    unsigned long off;                 /* 離す帰線 */
} Voice;
static Voice voice[2];
static unsigned long tick;             /* 数えた垂直帰線 */
static int vs_last;

static unsigned long at(unsigned long units) { return units * UNIT_FX >> 10; }

void music_start(void)
{
    present = inp(P_ADDR) != 0xFF;
    if (!present) return;
    opn(0x27, 0x00);                   /* タイマーは使わない・CH3 は普通の形 */
    opn(0x07, 0xBF);                   /* SSG は鳴らさない（bit7/6 = I/O ポートの向き。PC-98 の決まり）*/
    for (int ch = 0; ch < 3; ch++) opn(0x28, ch);
    set_patch(0, &patches[pairs[FM_PAIR][0]]);
    set_patch(1, &patches[pairs[FM_PAIR][1]]);
    voice[0].ev = canon_v0; voice[0].n = CANON_N0;
    voice[1].ev = canon_v1; voice[1].n = CANON_N1;
    for (int v = 0; v < 2; v++) { voice[v].i = 0; voice[v].on = 0; voice[v].pos = 0; voice[v].off = 0; }
    tick = 0;
    vs_last = inp(0x60) & 0x20;
    playing = 1;
}

void music_poll(void)
{
    if (!playing) return;
    const int vs = inp(0x60) & 0x20;   /* GDC の状態: bit5 = 垂直帰線中。立ち上がりで 1 つ数える */
    if (!vs || vs_last) { vs_last = vs; return; }
    vs_last = vs;
    tick++;
    for (int v = 0; v < 2; v++) {
        Voice *p = &voice[v];
        if (p->on && tick >= p->off) { opn(0x28, v); p->on = 0; }
        while (tick >= at(p->pos)) {
            const int b = p->ev[p->i][0], note = b & 0x7F, len = p->ev[p->i][1];
            if (++p->i == p->n) p->i = 0;          /* 2 声の長さは同じなので、そろって頭へ戻る */
            if (note) {
                if ((b & CANON_LEGATO) && p->on)
                    set_freq(v, note);             /* ★トリルの 2 音目から: 弾き直さず高さだけ */
                else
                    key_on(v, note);
                p->on = 1;
                /* 次がレガートなら離さない */
                p->off = (p->ev[p->i][0] & CANON_LEGATO) ? ~0UL : at(p->pos + len) - GAP;
            }
            p->pos += len;
        }
    }
}

void music_stop(void)
{
    if (!present || !playing) return;
    playing = 0;
    for (int ch = 0; ch < 3; ch++) opn(0x28, ch);
}
#endif
