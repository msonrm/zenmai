#ifndef PACK_H
#define PACK_H
#include <stdint.h>

/* 作品の束（パック・.ZMP）を読み、合う story を探して読む。書式と作り方は gen_pack.py。
 *
 * ★**Zenmai は Z-machine、作品はファイルとして外から渡す**（2026-09-29）。
 *   ★story はパックに入っていない。パックが持つ識別（release・serial・checksum）で、横に置かれた story を探す:
 *   INFO の story= の名前 → パックと同じ名前の .Z3 → カレントディレクトリの全部の .Z3 / .DAT。
 *   ★識別が合わない story は使わない（訳の表は特定の版の文に合わせてあるので、別の版では訳が外れる）。
 * ★story は起動後に確保する —— 本体（DOS/4GW が載せる像）を小さく保つため（docs/pc98-port-plan.md の「段 8」）。
 *
 * ★stdio と、ディレクトリを見る所だけ機械ごと（PC-98 = Watcom の _dos_findfirst / ホスト = dirent）。 */

typedef struct {
    uint8_t *ram;       /* VM が読み書きする story の作業域（story を写したもの） */
    uint32_t len;       /* story の長さ */
    uint8_t *init;      /* 初期イメージの動的領域（セーブの差分の相手）。★story の全部は持たない */
    char base[9];       /* パックの名前（拡張子なし・大文字）。セーブの名前にも使う（ZORK1 → ZORK1.SAV） */
    char story[13];     /* 見つけた story のファイル名 */
    char title[48];     /* 作品名（UTF-8・INFO の title） */
    uint16_t release;   /* 合う story の識別（IDNT） */
    char serial[7];
    uint16_t checksum;
} ZmPack;

/* 読めたら 0。読めなければ理由（ASCII —— DOS の画面に出すので）。 */
const char *pack_load(const char *path, ZmPack *pk);

#endif
