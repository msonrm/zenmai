/* Zenmai プラットフォーム実装 — SDL2（PortMaster / Linux デスクトップ）。
 *
 * plat_ps1.c の対。境界は plat.h の 9 本だけで、描画の芯・組版・グリフ・履歴・窓・
 * 入力の状態機械・訳は 1 行も触らずにそのまま動く。
 *
 * ★**PS1 の制約がそのまま移植性になっている**:
 *   - 画面は 640×480 の 15bit バッファ 1 枚。SDL では BGR555 のストリーミング
 *     テクスチャに毎フレーム流すだけで、色の変換が要らない
 *     （PS1 VRAM の 15bpp は `x BBBBB GGGGG RRRRR` ＝ SDL_PIXELFORMAT_BGR555 と同じ並び。
 *      render.h の BG = 0x0442 が (R,G,B) = (2,2,1) になることで確かめてある）
 *   - インタレースが要らない（LCD なのでプログレッシブ）
 *   - 640×480 は PortMaster 主流機の解像度と一致する。それ以外の機種も
 *     SDL_RenderSetLogicalSize が縦横比を保って拡縮する
 *
 * ★**提示は wait_fields でだけ行う**。PS1 では GPU が勝手に走査していたので、
 *   ゲーム側に「1 フレーム描き終えた」という合図が無い —— 唯一それに相当するのが
 *   wait_fields（対話ループは毎周 wait_fields(1) を通る）。ここに提示を寄せると、
 *   ★上の層に「フレームの終わり」を教える改造を入れずに済む。
 *
 * ★**やめる＝プロセスを終える**。PS1 版は .bss を潰して _start の頭へ戻るが、
 *   Linux で同じことをすると glibc と SDL の状態まで消える。PortMaster では
 *   ポートを抜けるとランチャのメニューへ戻るので、そちらが作法としても正しい。
 */
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plat.h"

static SDL_Window *win;
static SDL_Renderer *ren;
static SDL_Texture *tex;
static SDL_GameController *pad;
static uint16_t fb[H][W];              /* 画面そのもの（PS1 の VRAM 表示領域に対応） */
static uint32_t next_ms;               /* 次のフィールドの目標時刻（積算で刻む） */

/* ---- 画面 ---- */

/* ★台本の再生中は画面が要らないので headless のドライバで上げる。
   ★**どれが入っているかは SDL のビルド次第** —— 開発機（Debian の libsdl2）は
   `dummy` を持っているが、**R36H の SDL2 は `offscreen` しか持っていなかった**
   （実機で判明。`dummy not available` で落ちた）。呼ぶ側に選ばせると、機械ごとに
   検査の起動法が変わってしまうので、ここで両方あたる。
   SDL_VIDEODRIVER が既に指定されていれば、その指定を尊重する。 */
static int init_video(void)
{
    const char *sc = SDL_getenv("ZENMAI_SCRIPT");
    if (sc && *sc && !SDL_getenv("SDL_VIDEODRIVER")) {
        static const char *const HEADLESS[] = { "offscreen", "dummy" };
        for (unsigned i = 0; i < sizeof HEADLESS / sizeof *HEADLESS; i++) {
            SDL_setenv("SDL_VIDEODRIVER", HEADLESS[i], 1);
            if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) == 0)
                return 0;
        }
        SDL_setenv("SDL_VIDEODRIVER", "", 1);   /* どれも無ければ既定に戻して試す */
    }
    return SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
}

void gpu_init(void)
{
    if (win)                           /* PS1 版は _start の周回で毎回呼ぶので冪等にする */
        return;
    if (init_video() != 0) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        exit(1);
    }
    /* ★既定は全画面（PortMaster）。ZENMAI_WINDOW=1 で窓にする（開発機での確認用）。 */
    Uint32 flags = SDL_WINDOW_FULLSCREEN_DESKTOP;
    const char *w = SDL_getenv("ZENMAI_WINDOW");
    if (w && *w && *w != '0')
        flags = 0;
    win = SDL_CreateWindow("ぜんまい / Zenmai", SDL_WINDOWPOS_CENTERED,
                           SDL_WINDOWPOS_CENTERED, W, H, flags);
    if (!win) { SDL_Log("SDL_CreateWindow: %s", SDL_GetError()); exit(1); }
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren)
        ren = SDL_CreateRenderer(win, -1, 0);
    if (!ren) { SDL_Log("SDL_CreateRenderer: %s", SDL_GetError()); exit(1); }
    SDL_RenderSetLogicalSize(ren, W, H);       /* 機種ごとの画面へ縦横比を保って拡縮 */
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_BGR555,
                            SDL_TEXTUREACCESS_STREAMING, W, H);
    if (!tex) { SDL_Log("SDL_CreateTexture: %s", SDL_GetError()); exit(1); }
    SDL_ShowCursor(SDL_DISABLE);
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_IsGameController(i) && (pad = SDL_GameControllerOpen(i)))
            break;
    memset(fb, 0, sizeof fb);
    next_ms = SDL_GetTicks();
}

void gpu_display_on(void)
{
    /* PS1 では前のプログラムが残した VRAM を見せないための表示オンだが、
       こちらは fb が最初から黒なので何もしなくてよい。 */
}

/* PS1 の GPU フィルは色を 0xBBGGRR で受け取る（GP0(02h) の並び）。
   ★地色には使わない約束（main.c の注記）なので、ここも忠実に 15bit へ落とすだけ。 */
void gp0_fill(int x, int y, int w, int h, uint32_t rgb24)
{
    uint16_t c = (uint16_t)(((rgb24 & 0xF8) >> 3)              /* R */
                          | ((rgb24 & 0xF800) >> 6)            /* G */
                          | ((rgb24 & 0xF80000) >> 9));        /* B */
    for (int j = 0; j < h; j++) {
        int dy = y + j;
        if (dy < 0 || dy >= H) continue;
        for (int i = 0; i < w; i++) {
            int dx = x + i;
            if (dx >= 0 && dx < W) fb[dy][dx] = c;
        }
    }
}

void gp0_upload(int x, int y, int w, int h, const uint16_t *src)
{
    for (int j = 0; j < h; j++) {
        int dy = y + j;
        if (dy < 0 || dy >= H) continue;
        int x0 = x < 0 ? 0 : x;
        int x1 = x + w > W ? W : x + w;
        if (x1 > x0)
            memcpy(&fb[dy][x0], src + (size_t)j * w + (x0 - x),
                   (size_t)(x1 - x0) * sizeof(uint16_t));
    }
}

/* ★画面内コピー。上下どちらへ動いても壊れないよう行単位で memmove する
   （render.c 側が重ならない刻みで呼んでいるが、ここで保証しておく方が安い）。 */
void gp0_copy(int sx, int sy, int dx, int dy, int w, int h)
{
    if (w > W) w = W;
    if (dy > sy) {
        for (int j = h - 1; j >= 0; j--)
            if (sy + j >= 0 && sy + j < H && dy + j >= 0 && dy + j < H)
                memmove(&fb[dy + j][dx], &fb[sy + j][sx], (size_t)w * sizeof(uint16_t));
    } else {
        for (int j = 0; j < h; j++)
            if (sy + j >= 0 && sy + j < H && dy + j >= 0 && dy + j < H)
                memmove(&fb[dy + j][dx], &fb[sy + j][sx], (size_t)w * sizeof(uint16_t));
    }
}

static void present(void)
{
    SDL_UpdateTexture(tex, NULL, fb, W * (int)sizeof(uint16_t));
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, NULL, NULL);
    SDL_RenderPresent(ren);
}

static void pump(void)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            exit(0);
        case SDL_KEYDOWN:
            if (e.key.keysym.sym == SDLK_ESCAPE)
                exit(0);
            break;
        case SDL_CONTROLLERDEVICEADDED:
            if (!pad)
                pad = SDL_GameControllerOpen(e.cdevice.which);
            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            if (pad && e.cdevice.which
                == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad))) {
                SDL_GameControllerClose(pad);
                pad = NULL;
            }
            break;
        }
    }
}

/* ★n フィールド（1/60 秒）待つ。提示とイベント汲みもここでやる。
   目標時刻を積算するので、1 回遅れても後で取り返して時計がずれない。 */
void wait_fields(int n)
{
    static int headless = -1;
    if (headless < 0) {
        const char *s_ = SDL_getenv("ZENMAI_SCRIPT");
        headless = s_ && *s_ ? 1 : 0;
    }
    if (headless) {                    /* ★台本の再生中は待たない（検査を待たせない） */
        pump();
        return;
    }
    for (; n > 0; n--) {
        present();
        next_ms += 1000 / 60;
        uint32_t now = SDL_GetTicks();
        if ((int32_t)(next_ms - now) > 0)
            SDL_Delay(next_ms - now);
        else if ((int32_t)(now - next_ms) > 250)
            next_ms = now;             /* 大きく遅れたら取り返しを諦める（早送りを防ぐ） */
        pump();
    }
}

/* ---- パッド ---- */

void pad_try_analog(void)
{
    /* SDL のゲームコントローラは最初からアナログ。PS1 の「アナログ化コマンド」に相当なし。 */
}

/* ★キーボードは開発機で確かめるためのもの（PortMaster では使わない）。
   十字 = 方向キー / ✕=Z ○=X □=A △=S / L1=Q R1=E L2=1 R2=3 /
   START=Enter SELECT=Tab / 右スティック=IJKL / 左スティック=TFGH */
static int keyboard_bits(const Uint8 *k)
{
    int p = 0;
    if (k[SDL_SCANCODE_UP])     p |= BTN_UP;
    if (k[SDL_SCANCODE_DOWN])   p |= BTN_DOWN;
    if (k[SDL_SCANCODE_LEFT])   p |= BTN_LEFT;
    if (k[SDL_SCANCODE_RIGHT])  p |= BTN_RIGHT;
    if (k[SDL_SCANCODE_Z])      p |= BTN_X;
    if (k[SDL_SCANCODE_X])      p |= BTN_CIR;
    if (k[SDL_SCANCODE_A])      p |= BTN_SQ;
    if (k[SDL_SCANCODE_S])      p |= BTN_TRI;
    if (k[SDL_SCANCODE_Q])      p |= BTN_L1;
    if (k[SDL_SCANCODE_E])      p |= BTN_R1;
    if (k[SDL_SCANCODE_1])      p |= BTN_L2;
    if (k[SDL_SCANCODE_3])      p |= BTN_R2;
    if (k[SDL_SCANCODE_RETURN]) p |= BTN_START;
    if (k[SDL_SCANCODE_TAB])    p |= BTN_SELECT;
    return p;
}

/* SDL の軸（-32768..32767）を PS1 の 0..255（0x80 が中立）へ。 */
static uint8_t axis8(Sint16 v)
{
    int a = (v + 32768) >> 8;
    return (uint8_t)(a < 0 ? 0 : a > 255 ? 255 : a);
}

static int script_step(int *p, uint8_t axes[4]);

int pad_read_ex(uint8_t axes[4])
{
    pump();
    axes[0] = axes[1] = axes[2] = axes[3] = 0x80;
    {
        int scripted;
        if (script_step(&scripted, axes))
            return scripted;
    }

    const Uint8 *k = SDL_GetKeyboardState(NULL);
    int p = keyboard_bits(k);
    if (k[SDL_SCANCODE_I]) axes[1] = 0x00;
    if (k[SDL_SCANCODE_K]) axes[1] = 0xFF;
    if (k[SDL_SCANCODE_J]) axes[0] = 0x00;
    if (k[SDL_SCANCODE_L]) axes[0] = 0xFF;
    if (k[SDL_SCANCODE_T]) axes[3] = 0x00;
    if (k[SDL_SCANCODE_G]) axes[3] = 0xFF;
    if (k[SDL_SCANCODE_F]) axes[2] = 0x00;
    if (k[SDL_SCANCODE_H]) axes[2] = 0xFF;

    if (pad) {
        /* ★フェイスボタンは**位置**で写す —— 盤の図が ✕ = 下 / ○ = 右 / □ = 左 /
           △ = 上 を**位置で**描いているので、位置が合っていないと図が嘘になる。
           ★★ところが **SDL の A/B/X/Y から位置は復元できない**。SDL の契約では
           A = 下だが、実際の割り当ては機種ごとの定義ファイル次第で、PortMaster は
           **その機体に印刷された札**に合わせている（R36H の実測: `a:b1, b:b0,
           x:b2, y:b3` ＝ a が物理的に右、b が下、x が上、y が左）。
           Linux 側も助けにならない —— `BTN_X` は `BTN_NORTH` の別名なのに、
           ドライバは印刷された札で割り当てるので、名前と位置が対応しない。
           ★**だから下の対応表は「Nintendo 式の札を持つ機体」という仮定**である。
           PortMaster の主流機（Anbernic / Powkiddy 系）はどれもこれに当たる。
           デスクトップの Xbox パッドなど**標準の並びの機体では上下逆になる** ——
           そのときは A↔B と X↔Y を入れ替えること（設定にはしていない）。
           2026-08-29 に R36H の実機で判明。 */
        static const struct { int sdl, bit; } M[] = {
            { SDL_CONTROLLER_BUTTON_B,             BTN_X      },   /* 下 = ✕ */
            { SDL_CONTROLLER_BUTTON_A,             BTN_CIR    },   /* 右 = ○ */
            { SDL_CONTROLLER_BUTTON_Y,             BTN_SQ     },   /* 左 = □ */
            { SDL_CONTROLLER_BUTTON_X,             BTN_TRI    },   /* 上 = △ */
            { SDL_CONTROLLER_BUTTON_BACK,          BTN_SELECT },
            { SDL_CONTROLLER_BUTTON_START,         BTN_START  },
            { SDL_CONTROLLER_BUTTON_LEFTSTICK,     BTN_L3     },
            { SDL_CONTROLLER_BUTTON_RIGHTSTICK,    BTN_R3     },
            { SDL_CONTROLLER_BUTTON_LEFTSHOULDER,  BTN_L1     },
            { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, BTN_R1     },
            { SDL_CONTROLLER_BUTTON_DPAD_UP,       BTN_UP     },
            { SDL_CONTROLLER_BUTTON_DPAD_DOWN,     BTN_DOWN   },
            { SDL_CONTROLLER_BUTTON_DPAD_LEFT,     BTN_LEFT   },
            { SDL_CONTROLLER_BUTTON_DPAD_RIGHT,    BTN_RIGHT  },
        };
        for (unsigned i = 0; i < sizeof M / sizeof *M; i++)
            if (SDL_GameControllerGetButton(pad, (SDL_GameControllerButton)M[i].sdl))
                p |= M[i].bit;
        /* ★L2 / R2 はアナログトリガ。★入力の要（子音行の L1 / BS の R2）なので
           閾値は浅め（1/4）にする —— 深いと「押したのに出ない」が入力全体に効く。 */
        if (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT)  > 8192) p |= BTN_L2;
        if (SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 8192) p |= BTN_R2;

        Sint16 rx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX);
        Sint16 ry = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY);
        Sint16 lx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
        Sint16 ly = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
        if (axes[0] == 0x80) axes[0] = axis8(rx);
        if (axes[1] == 0x80) axes[1] = axis8(ry);
        if (axes[2] == 0x80) axes[2] = axis8(lx);
        if (axes[3] == 0x80) axes[3] = axis8(ly);
    }
    return p;
}

int pad_read(void)
{
    uint8_t axes[4];
    return pad_read_ex(axes);
}

/* ---- 台本の再生（検査用）----
 *
 * ★**PS1 の sim.py と同じ台本ファイルを食べる**。行は "f0-f1:UP+TRI"、時間軸は
 *   **パッドのポーリング回数**（sim.py の --polls と同じ閉ループ）。
 *   これで「同じ操作を与えたら同じ画面になる」を**画素で**突き合わせられる ——
 *   ★移植で挙動が変わっていないことを、目視ではなく検査で言えるようにするため。
 *
 *   ZENMAI_SCRIPT=<台本>  再生する（人の入力は無視される）
 *   ZENMAI_STOP=<回>      この回数で止めて書き出す（既定 40000）
 *   ZENMAI_RAW=<出力>     画面を 640×480 の RGB555 リトルエンディアンで書き出す
 */
#define SCRIPT_MAX 256
static struct { int f0, f1, mask; } sc[SCRIPT_MAX];
static int sc_n = -1;                  /* -1 = 未読み込み / 0 = 台本なし */
static int poll_n;

static int bit_of(const char *nm)
{
    static const struct { const char *nm; int bit; } B[] = {
        { "SEL", BTN_SELECT }, { "L3", BTN_L3 }, { "R3", BTN_R3 }, { "START", BTN_START },
        { "UP", BTN_UP }, { "RIGHT", BTN_RIGHT }, { "DOWN", BTN_DOWN }, { "LEFT", BTN_LEFT },
        { "L2", BTN_L2 }, { "R2", BTN_R2 }, { "L1", BTN_L1 }, { "R1", BTN_R1 },
        { "TRI", BTN_TRI }, { "CIR", BTN_CIR }, { "X", BTN_X }, { "SQ", BTN_SQ },
        /* スティックは疑似ビット（PS1 の 16 ビットの上に積む。sim.py と同じ並び） */
        { "LSL", 1 << 16 }, { "LSR", 1 << 17 }, { "LSU", 1 << 18 }, { "LSD", 1 << 19 },
        { "RSL", 1 << 20 }, { "RSR", 1 << 21 }, { "RSU", 1 << 22 }, { "RSD", 1 << 23 },
    };
    for (unsigned i = 0; i < sizeof B / sizeof *B; i++)
        if (strcmp(nm, B[i].nm) == 0)
            return B[i].bit;
    fprintf(stderr, "台本: 知らないボタン名 '%s'\n", nm);
    exit(2);
}

static void script_load(void)
{
    sc_n = 0;
    const char *path = SDL_getenv("ZENMAI_SCRIPT");
    if (!path || !*path)
        return;
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "台本が開けない: %s\n", path); exit(2); }
    char line[512];
    while (fgets(line, sizeof line, f)) {
        char *h = strchr(line, '#');
        if (h) *h = 0;
        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = 0;
        int f0, f1;
        if (sscanf(line, " %d - %d", &f0, &f1) != 2) continue;
        int mask = 0;
        char *p = colon + 1, *tok;
        while ((tok = strtok(p, "+ \t\r\n")) != NULL) {
            p = NULL;
            mask |= bit_of(tok);
        }
        if (sc_n < SCRIPT_MAX)
            sc[sc_n++] = (typeof(sc[0])){ f0, f1, mask };
    }
    fclose(f);
}

/* 台本を再生中なら 1 を返し、p / axes をその回の内容で埋める。 */
static int script_step(int *p, uint8_t axes[4])
{
    if (sc_n < 0)
        script_load();
    if (!sc_n)
        return 0;

    const char *stop = SDL_getenv("ZENMAI_STOP");
    int limit = stop && *stop ? atoi(stop) : 40000;
    if (++poll_n >= limit) {
        const char *raw = SDL_getenv("ZENMAI_RAW");
        if (raw && *raw) {
            FILE *f = fopen(raw, "wb");
            if (f) { fwrite(fb, sizeof fb, 1, f); fclose(f); }
        }
        exit(0);
    }

    int mask = 0;
    for (int i = 0; i < sc_n; i++)
        if (poll_n >= sc[i].f0 && poll_n <= sc[i].f1)
            mask |= sc[i].mask;
    *p = mask & 0xFFFF;
    axes[0] = axes[1] = axes[2] = axes[3] = 0x80;
    if (mask & (1 << 16)) axes[2] = 0x00;      /* 左スティック ← */
    if (mask & (1 << 17)) axes[2] = 0xFF;      /* → */
    if (mask & (1 << 18)) axes[3] = 0x00;      /* ↑ */
    if (mask & (1 << 19)) axes[3] = 0xFF;      /* ↓ */
    if (mask & (1 << 20)) axes[0] = 0x00;      /* 右スティック ← */
    if (mask & (1 << 21)) axes[0] = 0xFF;      /* → */
    if (mask & (1 << 22)) axes[1] = 0x00;      /* ↑ */
    if (mask & (1 << 23)) axes[1] = 0xFF;      /* ↓ */
    return 1;
}
