#ifndef PACK_H
#define PACK_H
#include <stdint.h>

/* 作品の束（パック・.ZMP）を読む。書式と作り方は gen_pack.py。
 *
 * ★**Zenmai は Z-machine、作品はファイルとして外から渡す**（2026-09-29）。PC-98 版は story を
 *   実行ファイルに焼き込まず、起動時にここで読む。★本体（DOS/4GW が載せる像）を小さくして、
 *   大きなものは起動後に確保するため —— 拡張 1MB では像が約 680KB を超えると載らないが、
 *   malloc は通常メモリも使えてさらに約 500KB 取れる（QuuBee で測った・docs/pc98-port-plan.md の「段 8」）。
 *
 * ★stdio だけで書いてある（PC-98 とホストで同じもの）。 */

typedef struct {
    uint8_t *ram;       /* VM が読み書きする story の作業域（story を写したもの） */
    uint32_t len;       /* story の長さ */
    uint8_t *init;      /* 初期イメージの動的領域（セーブの差分の相手）。★story の全部は持たない */
    char name[48];      /* 作品名（UTF-8） */
} ZmPack;

/* 読めたら 0。読めなければ理由（ASCII —— DOS の画面に出すので）。 */
const char *pack_load(const char *path, ZmPack *pk);

#endif
