/* PS1 メモリーカード。
 *
 * プロトコルは psx-spx の Read/Write Frame をそのまま写した。装置アドレスは 81h で、
 * パッド(01h)と同じ JOY ポートを共有する。1 フレーム = 128 バイト、
 * 1 ブロック = 64 フレーム = 8KB、カード全体 = 16 ブロック(先頭はディレクトリ)。
 *
 * ★フォーマットはしない。ヘッダが "MC" でなければ失敗を返す ——
 *   フォーマットは他のセーブを消す操作なので、ゲームが黙ってやってよいことではない。
 */
#include "card.h"
#include "card_icon.h"

typedef unsigned char u8;
typedef unsigned int u32;

#define JOY_DATA (*(volatile u8 *)0x1F801040)
#define JOY_STAT (*(volatile u32 *)0x1F801044)
#define JOY_MODE (*(volatile unsigned short *)0x1F801048)
#define JOY_CTRL (*(volatile unsigned short *)0x1F80104A)
#define JOY_BAUD (*(volatile unsigned short *)0x1F80104E)

#define FRAME 128
#define BLK_FRAMES 64
#define FNAME "BIZENMAI-ZORK"      /* 20 文字まで。BIOS の一覧に出るのはタイトルの方 */

/* ★render.c の pad_exchange と同じ形。分けてあるのは、カードの方が
   (1) 応答が遅い(書き込み後は特に)ので待ちを長く取る必要があり、
   (2) 一度に 140 バイト交換するのでパッドの可変長ロジックが邪魔なため。 */
static int card_exchange(const u8 *tx, u8 *rx, int n)
{
    JOY_CTRL = 0x1003;
    JOY_MODE = 0x000D;
    JOY_BAUD = 0x88;
    for (int i = 0; i < n; i++) {
        int t = 40000;
        while (!(JOY_STAT & 0x1) && --t) { }
        if (!t) { JOY_CTRL = 0; return i; }
        JOY_DATA = tx[i];
        t = 40000;
        while (!(JOY_STAT & 0x2) && --t) { }
        if (!t) { JOY_CTRL = 0; return i; }
        rx[i] = JOY_DATA;
        for (volatile int d = 0; d < 60; d++) { }
    }
    JOY_CTRL = 0;
    return n;
}

static void zero(u8 *p, int n) { while (n--) *p++ = 0; }

static u8 xsum(const u8 *p, int n) { u8 c = 0; while (n--) c ^= *p++; return c; }

static int read_frame(int sector, u8 *out)
{
    u8 tx[140], rx[140];
    zero(tx, sizeof tx);
    tx[0] = 0x81;
    tx[1] = 0x52;                                  /* "R" */
    tx[4] = (u8)((sector >> 8) & 0xFF);
    tx[5] = (u8)(sector & 0xFF);
    if (card_exchange(tx, rx, 140) != 140) return 0;
    if (rx[139] != 0x47) return 0;                 /* "G" = Good 以外は失敗 */
    u8 chk = (u8)(tx[4] ^ tx[5]);
    for (int i = 0; i < 128; i++) { out[i] = rx[10 + i]; chk ^= out[i]; }
    return chk == rx[138];
}

static int write_frame(int sector, const u8 *in)
{
    u8 tx[138], rx[138];
    zero(tx, sizeof tx);
    tx[0] = 0x81;
    tx[1] = 0x57;                                  /* "W" */
    tx[4] = (u8)((sector >> 8) & 0xFF);
    tx[5] = (u8)(sector & 0xFF);
    u8 chk = (u8)(tx[4] ^ tx[5]);
    for (int i = 0; i < 128; i++) { tx[6 + i] = in[i]; chk ^= in[i]; }
    tx[134] = chk;
    if (card_exchange(tx, rx, 138) != 138) return 0;
    return rx[137] == 0x47;
}

/* ディレクトリを見て、このソフトのブロックを探す。
   戻り値 = 使うブロック(1..15。既存が無ければ空き)。0 = 使えない。
   *existing には「既にセーブが在るブロック」を入れる(無ければ 0)。 */
static int find_block(int *existing)
{
    u8 f[FRAME];
    int free_blk = 0;
    *existing = 0;
    if (!read_frame(0, f)) return 0;
    if (f[0] != 'M' || f[1] != 'C') return 0;      /* 未フォーマット */
    for (int b = 1; b <= 15; b++) {
        if (!read_frame(b, f)) return 0;
        u32 state = (u32)f[0] | ((u32)f[1] << 8) | ((u32)f[2] << 16) | ((u32)f[3] << 24);
        if (state == 0x51) {                       /* 使用中(単独 or 先頭ブロック) */
            const char *n = FNAME;
            int i = 0;
            while (n[i] && f[0x0A + i] == (u8)n[i]) i++;
            if (!n[i] && f[0x0A + i] == 0) { *existing = b; return b; }
        } else if ((state & 0xFFFFFFF0u) == 0xA0u && !free_blk) {
            free_blk = b;                          /* A0..A3 = 空き */
        }
    }
    return free_blk;
}

static int write_dir(int blk)
{
    u8 f[FRAME];
    zero(f, FRAME);
    f[0] = 0x51;                                   /* 単独ブロックの先頭 */
    f[5] = 0x20;                                   /* ファイル長 2000h = 8KB */
    f[8] = 0xFF;
    f[9] = 0xFF;                                   /* 次のブロックは無い */
    const char *n = FNAME;
    for (int i = 0; n[i]; i++) f[0x0A + i] = (u8)n[i];
    f[0x7F] = xsum(f, 0x7F);
    return write_frame(blk, f);
}

/* タイトルフレーム(ブロックの先頭)。BIOS の管理画面はここを読む。checksum は無い。 */
static int write_title(int blk)
{
    u8 f[FRAME];
    zero(f, FRAME);
    f[0] = 'S';
    f[1] = 'C';
    f[2] = 0x11;                                   /* アイコン 1 枚(静止) */
    f[3] = 0x01;                                   /* 1 ブロック */
    for (int i = 0; i < 64; i++) f[4 + i] = CARD_TITLE_SJIS[i];
    for (int i = 0; i < 16; i++) {                 /* CLUT は 16bit リトルエンディアン */
        f[0x60 + i * 2] = (u8)(CARD_ICON_CLUT[i] & 0xFF);
        f[0x61 + i * 2] = (u8)(CARD_ICON_CLUT[i] >> 8);
    }
    return write_frame(blk * BLK_FRAMES, f);
}

int card_save(const u8 *data, int len)
{
    if (len < 0 || len > CARD_DATA_MAX) return 0;
    int existing = 0;
    int blk = find_block(&existing);
    if (blk < 1) return 0;
    if (!write_title(blk)) return 0;
    if (!write_frame(blk * BLK_FRAMES + 1, CARD_ICON_BITMAP)) return 0;
    u8 f[FRAME];
    int off = 0;
    for (int fr = 2; fr < BLK_FRAMES; fr++) {
        zero(f, FRAME);
        int o = 0;
        if (fr == 2) {                             /* データの先頭に長さを置く */
            f[0] = (u8)(len & 0xFF);
            f[1] = (u8)((len >> 8) & 0xFF);
            o = 2;
        }
        while (o < FRAME && off < len) f[o++] = data[off++];
        if (!write_frame(blk * BLK_FRAMES + fr, f)) return 0;
        if (off >= len) break;
    }
    /* ★ディレクトリは最後に書く。途中で電池が切れても「半端なセーブが有効に見える」
       ことがないように(既存を上書きしている場合は救えないが、新規は守れる)。 */
    return write_dir(blk);
}

int card_load(u8 *out, int max)
{
    int existing = 0;
    if (find_block(&existing) < 1 || !existing) return -1;
    u8 f[FRAME];
    if (!read_frame(existing * BLK_FRAMES + 2, f)) return -1;
    int len = f[0] | (f[1] << 8);
    if (len < 0 || len > CARD_DATA_MAX || len > max) return -1;
    int off = 0, o = 2;
    for (int fr = 2; fr < BLK_FRAMES && off < len; fr++) {
        if (fr > 2 && !read_frame(existing * BLK_FRAMES + fr, f)) return -1;
        while (o < FRAME && off < len) out[off++] = f[o++];
        o = 0;
    }
    return off == len ? len : -1;
}

int card_have(void)
{
    int existing = 0;
    find_block(&existing);
    return existing != 0;
}
