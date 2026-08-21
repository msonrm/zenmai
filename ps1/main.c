/* Zenmai PS1 — 日本語デモ(自動送り + かな入力)。
 *
 * 描画層は render.c(共有)、入力エンジンは input.c(ゴールデン 35 件検証済み)。
 * パッド未検出 = 自動デモ(sim のゴールデン経路)/ 検出 = かな入力で 6 手を進める。
 */
#include <stdint.h>
#include "render.h"
#include "content.h"
#include "input.h"

/* パッド未検出なら fields 後に自動送り。検出後は × で進む(自動送りしない)。
 * ↑↓ = 履歴のスクロールバック(半行刻み — 480i で揺れない歩幅に統一)。 */
static void wait_interactive(int fields)
{
    int elapsed = 0, tick = 0;
    for (;;) {
        wait_fields(1);
        elapsed++;
        tick++;
        int p = pad_read();
        if (p < 0) {
            pad_prev = 0;
            if (!pad_seen && elapsed >= fields) return;
            continue;
        }
        pad_seen = 1;
        int edge = p & ~pad_prev;
        pad_prev = p;
        if (edge & BTN_X) return;
        if ((p & BTN_UP) && tick % 3 == 0) view_scroll(-SCROLL_STEP);
        if ((p & BTN_DOWN) && tick % 3 == 0) view_scroll(SCROLL_STEP);
    }
}

static const uint16_t IND_A[1] = {0x3042};   /* 行表示の既定「あ」 */

/* ---- 対話モード(パッドでかなを打つ) ---- */

enum { COMP_MAX = 20 };
static uint16_t comp[COMP_MAX];
static int clen, caret;

static GpMachine gm;

/* キャレット位置へ挿入(replace はキャレット直前から消す)。追記時は従来と同じ挙動 */
static void comp_insert(const uint16_t *t, int n, int replace)
{
    int del = replace > caret ? caret : replace;
    for (int i = caret - del; i + del < clen; i++)
        comp[i] = comp[i + del];
    clen -= del;
    caret -= del;
    if (clen + n > COMP_MAX)
        n = COMP_MAX - clen;
    for (int i = clen - 1; i >= caret; i--)
        comp[i + n] = comp[i];
    for (int i = 0; i < n; i++)
        comp[caret + i] = t[i];
    clen += n;
    caret += n;
}

static void comp_bs(void)
{
    if (!caret) return;
    for (int i = caret - 1; i + 1 < clen; i++)
        comp[i] = comp[i + 1];
    clen--;
    caret--;
}

static void confirm_input(int *next)
{
    /* 遡り中の確定は、まず下端へ跳んでひと呼吸置く */
    if (view_px < hist_total() - body_h) {
        view_bottom();
        render_window();
        wait_fields(18);
    }
    int match = 0;
    if (*next < TURN_N) {
        const Turn *tn = &cturns[*next];
        if (clen == tn->cmd_len) {
            match = 1;
            for (int i = 0; i < clen; i++)
                if (comp[i] != pool[tn->cmd_off + i]) { match = 0; break; }
        }
    }
    draw_echo(comp, clen);
    if (match) {
        const Turn *tn = &cturns[*next];
        for (int i = 0; i < tn->line_cnt; i++)
            draw_logical(&clines[tn->line_off + i], INK);
        (*next)++;
    } else {
        draw_plain(extra_notice, EXTRA_NOTICE_LEN, INK);
    }
    clen = 0;
    caret = 0;
    gm.eagerSet = 0;                   /* 消えたバッファへの巻き戻しを防ぐ */
    scroll_new();
}

__attribute__((noreturn)) static void interactive(void)
{
    int fields = 0, next = 1, rt_hold = 0, dirty = 1;
    int ls_prev = 0, ls_rep = 0;
    uint16_t prev_rowchar = 0;
    gp_init(&gm);
    clen = 0;
    caret = 0;
    pad_prev = 0;
    for (;;) {
        wait_fields(1);
        fields++;
        uint8_t axes[4];
        int p = pad_read_ex(axes);
        if (p < 0) { pad_prev = 0; continue; }
        int edge = p & ~pad_prev;
        pad_prev = p;

        /* フレーム導出(web engine.ts と同じ写像。R1=あ段 / □△○× = い〜お段) */
        int dir = (p & BTN_LEFT) ? 1 : (p & BTN_UP) ? 2 : (p & BTN_RIGHT) ? 3
                : (p & BTN_DOWN) ? 4 : 0;
        int row = dir + ((p & BTN_L1) ? 5 : 0);
        int vowel = (p & BTN_R1) ? 0 : (p & BTN_SQ) ? 1 : (p & BTN_TRI) ? 2
                  : (p & BTN_CIR) ? 3 : (p & BTN_X) ? 4 : -1;
        int cc = !!(p & BTN_LEFT) + !!(p & BTN_UP) + !!(p & BTN_RIGHT)
               + !!(p & BTN_DOWN) + !!(p & BTN_L1);
        GpFrame f = { fields * 17, row, vowel, vowel >= 0,
                      !!(p & BTN_L2), !!(p & BTN_R2), cc };
        GpAction a[4];
        int n = gp_step(&gm, 0, &f, a);
        for (int i = 0; i < n; i++) {
            if (a[i].type == GPA_KANA) {
                comp_insert(a[i].text, a[i].tlen, a[i].replace);
            } else {                   /* GPA_YOUON */
                uint16_t o[2];
                int rep, l = gp_resolve_youon(comp, caret, o, &rep);
                /* ★PS1 適応: 対象外の「っ」追加(rep==0)は捨てる —— 意図せず入る
                 * (実機フィードバック 2026-08-19)。っ は L2+R2 で明示的に打てる */
                if (l && rep > 0) comp_insert(o, l, rep);
            }
            dirty = 1;
        }
        gp_sync_prev(&gm, &f);

        /* 右スティック = GIME 由来のフリック(↑濁点 / ←BS / →ー / ↓確定) */
        int rs = pad_rstick_flick(axes);
        if (rs)
            gp_break_rt_cycle(&gm);    /* スティック入力で ん→を 循環は切れる(GIME 準拠) */

        if ((edge & BTN_SELECT) || rs == 2) {   /* Select / 右スティック↑ = 濁点トグル */
            uint16_t o[2];
            int rep, l = gp_resolve_dakuten(comp, caret, o, &rep);
            if (l) { comp_insert(o, l, rep); dirty = 1; }
        }
        if (p & BTN_R2) {              /* R2 長押し = BS 1 回だけ(リピートしない —— 打ち間違いは
                                        * 1 字消せば足りる。実機フィードバック 2026-08-19) */
            rt_hold++;
            if (rt_hold == 24) {
                gp_consume_rt(&gm);
                if (caret) { comp_bs(); gm.eagerSet = 0; dirty = 1; }
            }
        } else {
            rt_hold = 0;
        }
        if (rs == 1 && caret) {        /* 右スティック← = BS 1 回 */
            comp_bs();
            gm.eagerSet = 0;
            dirty = 1;
        }
        if (rs == 3) {                 /* 右スティック→ = 長音「ー」 */
            uint16_t ch = 0x30FC;
            comp_insert(&ch, 1, 0);
            dirty = 1;
        }
        if ((edge & BTN_R3) && clen) { /* R3 押込み = 入力行クリア(GIME の RS 押込み=キャンセル) */
            clen = 0;
            caret = 0;
            gm.eagerSet = 0;
            dirty = 1;
        }
        if (((edge & (BTN_START | BTN_L3)) || rs == 4) && clen) {
            confirm_input(&next);      /* Start / L3 押込み / 右スティック↓ = 確定 */
            dirty = 1;                 /* コマンド欄を空に描き直す */
        }
        /* L2+↑↓(デジタル)/ 左スティック縦(アナログ)= 履歴スクロール */
        if (fields % 3 == 0) {
            int up = (clen == 0 && (p & BTN_L2) && (p & BTN_UP)) || axes[3] < 0x40;
            int dn = (clen == 0 && (p & BTN_L2) && (p & BTN_DOWN)) || axes[3] > 0xC0;
            if (up) view_scroll(-SCROLL_STEP);
            if (dn) view_scroll(SCROLL_STEP);
        }
        /* 左スティック横 = キャレット移動(エッジ + 6 フィールドごとリピート) */
        {
            int d = axes[2] < 0x40 ? -1 : axes[2] > 0xC0 ? 1 : 0;
            int fire = 0;
            if (d != ls_prev) { ls_prev = d; ls_rep = 0; fire = d != 0; }
            else if (d && ++ls_rep >= 6) { ls_rep = 0; fire = 1; }
            if (fire) {
                int nc = caret + d;
                if (nc >= 0 && nc <= clen && nc != caret) {
                    caret = nc;
                    gm.eagerSet = 0;   /* 移動したら巻き戻し対象は無効 */
                    dirty = 1;
                }
            }
        }
        uint16_t rowchar = gp_row_char(row);
        if (dirty || rowchar != prev_rowchar) {
            draw_strip(comp, clen, caret, &rowchar, 1);
            prev_rowchar = rowchar;
            dirty = 0;
        }
    }
}

__attribute__((section(".text.start"), noreturn)) void _start(void)
{
    gpu_init();
    gp0_fill(0, 0, W, H, 0x0F1214);    /* 全面 BG(GPU フィルは 0xBBGGRR) */
    render_init();
    jp_text_init();                    /* 本文はルビつき描画 */

    /* 冒頭(ターン 0)を積んで表示 ON */
    for (int i = 0; i < cturns[0].line_cnt; i++)
        draw_logical(&clines[cturns[0].line_off + i], INK);
    view_bottom();
    render_window();
    draw_strip(0, 0, 0, IND_A, 1);
    GP1 = 0x03000000;
    pad_prev = 0;
    pad_seen = 0;
    pad_try_analog();

    /* パッド検出(30 フィールド)。検出 → 対話モード / 無し → 自動デモ(sim のゴールデン経路) */
    for (int i = 0; i < 30; i++) {
        wait_fields(1);
        if (pad_read() >= 0)
            interactive();             /* noreturn */
    }
    wait_interactive(WAIT_READ);

    for (int t = 1; t < TURN_N; t++) {
        const Turn *tn = &cturns[t];
        /* 入力欄にコマンドが打たれる */
        draw_strip(&pool[tn->cmd_off], tn->cmd_len, tn->cmd_len, IND_A, 1);
        wait_interactive(WAIT_TYPE);
        /* 確定: 反響(＞コマンド)と出力を巻物へ足し、半行送りで見せる */
        draw_echo(&pool[tn->cmd_off], tn->cmd_len);
        for (int i = 0; i < tn->line_cnt; i++)
            draw_logical(&clines[tn->line_off + i], INK);
        draw_strip(0, 0, 0, IND_A, 1);
        scroll_new();
        if (t < TURN_N - 1)
            wait_interactive(WAIT_READ);
    }
    /* 最終画面: スクロールバックだけ受け付け続ける。
     * パッド未検出(= sim)なら停止して終了扱い。 */
    for (;;) {
        wait_fields(1);
        int p = pad_read();
        if (p < 0) {
            if (!pad_seen) break;
            pad_prev = 0;
            continue;
        }
        pad_seen = 1;
        pad_prev = p;
        if ((p & BTN_UP) && view_scroll(-SCROLL_STEP))
            wait_fields(2);
        if ((p & BTN_DOWN) && view_scroll(SCROLL_STEP))
            wait_fields(2);
    }
    for (;;) { }
}
