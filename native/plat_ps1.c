/* Zenmai プラットフォーム実装 — PlayStation 実機。
 *
 * ★render.c から切り出した（2026-08-29）。中身は 1 行も変えていない ——
 *   移植のために振る舞いを直すと、直したのか壊したのか区別が付かなくなる。
 *   SDL 版（plat_sdl.c）はこのファイルの対で、境界は plat.h。
 *
 * GPU は 640×480i NTSC 15bpp。パッドは JOY シリアル（psx-spx の手順）。
 */
#include "plat.h"

#define GP0 (*(volatile uint32_t *)0x1F801810)
#define GP1 (*(volatile uint32_t *)0x1F801814)
#define GPUSTAT (*(volatile uint32_t *)0x1F801814)

/* ---- GPU ---- */

void gpu_init(void)
{
    GP1 = 0x00000000;
    GP1 = 0x08000027;                  /* 640×480i NTSC 15bpp */
    GP1 = 0x06C60260;
    GP1 = 0x07042010;
    GP1 = 0x05000000;
}

/* GPU がコマンド語を受け付けられるまで待つ(GPUSTAT bit26)。
 * 待たずに連発すると 16 語の FIFO が溢れ、実機ではコマンドが落ちる
 * (VRAM 面内コピーの連発でスクロールに断片が残った実測あり) */
static void gpu_sync(void)
{
    while (!(GPUSTAT & (1u << 26))) { }
}

void gp0_fill(int x, int y, int w, int h, uint32_t rgb24)
{
    gpu_sync();
    GP0 = 0x02000000 | rgb24;
    GP0 = (uint32_t)(y << 16) | (uint32_t)x;
    GP0 = (uint32_t)(h << 16) | (uint32_t)w;
}

void gp0_upload(int x, int y, int w, int h, const uint16_t *src)
{
    gpu_sync();
    GP0 = 0xA0000000;
    GP0 = (uint32_t)(y << 16) | (uint32_t)x;
    GP0 = (uint32_t)(h << 16) | (uint32_t)w;
    const uint32_t *p = (const uint32_t *)src;
    for (int i = 0; i < w * h / 2; i++)
        GP0 = p[i];
}

void wait_fields(int n)
{
    uint32_t last = GPUSTAT >> 31;
    while (n > 0) {
        uint32_t b = GPUSTAT >> 31;
        if (b != last) { last = b; n--; }
    }
}

/* ---- パッド ---- */

#define JOY_DATA (*(volatile uint8_t *)0x1F801040)
#define JOY_STAT (*(volatile uint32_t *)0x1F801044)
#define JOY_MODE (*(volatile uint16_t *)0x1F801048)
#define JOY_CTRL (*(volatile uint16_t *)0x1F80104A)
#define JOY_BAUD (*(volatile uint16_t *)0x1F80104E)

static int pad_exchange(const uint8_t *tx, int txn, uint8_t *rx)
{
    JOY_CTRL = 0x1003;
    JOY_MODE = 0x000D;
    JOY_BAUD = 0x88;
    int total = txn;
    int n = 0;
    for (int i = 0; i < total; i++) {
        int t = 2000;
        while (!(JOY_STAT & 0x1) && --t) { }
        if (!t) break;
        JOY_DATA = i < txn ? tx[i] : 0;
        t = 2000;
        while (!(JOY_STAT & 0x2) && --t) { }
        if (!t) break;
        rx[n++] = JOY_DATA;
        for (volatile int d = 0; d < 60; d++) { }
        if (i == 1) {
            int want = 3 + (rx[1] & 0x0F) * 2;
            if (want > total) total = want > 9 ? 9 : want;
        }
    }
    JOY_CTRL = 0;
    return n;
}

void pad_try_analog(void)
{
    static const uint8_t enter[9] = {0x01, 0x43, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t setan[9] = {0x01, 0x44, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t exitc[9] = {0x01, 0x43, 0x00, 0x00, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A};
    uint8_t rx[9];
    pad_exchange(enter, 5, rx);
    for (volatile int d = 0; d < 300; d++) { }
    pad_exchange(setan, 9, rx);
    for (volatile int d = 0; d < 300; d++) { }
    pad_exchange(exitc, 9, rx);
}

int pad_read_ex(uint8_t axes[4])
{
    static const uint8_t tx[5] = {0x01, 0x42, 0, 0, 0};
    uint8_t rx[9];
    axes[0] = axes[1] = axes[2] = axes[3] = 0x80;
    int n = pad_exchange(tx, 5, rx);
    if (n >= 9 && rx[1] == 0x73) {
        axes[0] = rx[5];
        axes[1] = rx[6];
        axes[2] = rx[7];
        axes[3] = rx[8];
        return ~(rx[3] | (rx[4] << 8)) & 0xFFFF;
    }
    if (n >= 5 && rx[1] == 0x41)
        return ~(rx[3] | (rx[4] << 8)) & 0xFFFF;
    return -1;
}

int pad_read(void)
{
    uint8_t axes[4];
    return pad_read_ex(axes);
}

/* 画面内コピー（render.c のスクロールが使う）。
 * ★露出帯の転送と隙間なく続けて発行するので、間に何も挟まない。 */
void gp0_copy(int sx, int sy, int dx, int dy, int w, int h)
{
    gpu_sync();
    GP0 = 0x80000000;                  /* VRAM 面内コピー */
    GP0 = (uint32_t)(sy << 16) | (uint32_t)sx;
    GP0 = (uint32_t)(dy << 16) | (uint32_t)dx;
    GP0 = (uint32_t)(h << 16) | (uint32_t)w;
}

/* ★表示オン。最初の画面を組み終えてから呼ぶ（起動直後に出すと、
 *   前のプログラムが残した VRAM がそのまま見えてしまう）。 */
void gpu_display_on(void)
{
    GP1 = 0x03000000;
}
