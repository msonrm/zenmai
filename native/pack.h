#ifndef PACK_H
#define PACK_H
#include <stdint.h>

/* 作品の一覧を作り、選んだ作品を開く。パック（.ZMP）の書式と作り方は gen_pack.py。
 *
 * ★**Zenmai は Z-machine、作品はファイルとして外から渡す**（2026-09-29）。作品は 2 通り:
 *   - パック（.ZMP）+ story: パックが持つ識別（release・serial・checksum）で、横に置かれた story を探す
 *     （INFO の story= の名前 → パックと同じ名前の .Z3 → 全部の .Z3 / .DAT）。
 *     訳・語彙・ふりがなの節が揃っていれば日本語でも遊べる。揃っていなければ英語だけ（題などの情報だけのパック）
 *   - story だけ（.Z3 / .DAT。どのパックにも使われていないもの）: 英語だけ。題はファイル名
 *   ★識別が合わない story はパックに使わない（訳の表は特定の版の文に合わせてあるので、別の版では訳が外れる）。
 *   ★MojoZork は Z-machine の版 3 だけなので、ほかの版の story は並べない。
 * ★story と表は選んでから確保する —— 本体（DOS/4GW が載せる像）を小さく保つため（docs/pc98-port-plan.md の「段 8」）。
 *
 * ★stdio と、ディレクトリを見る所だけ機械ごと（PC-98 = Watcom の _dos_findfirst / ホスト = dirent）。 */

enum { WORKS_MAX = 8 };

typedef struct {
    char pack[13];      /* パックのファイル名（"" = パック無し・story だけ） */
    char story[13];     /* story のファイル名（"" = パックはあるが、合う story が見つからない） */
    char base[9];       /* セーブの名前（拡張子なし・大文字。パックかファイルの名前）: ZORK1 → ZORK1.SAV */
    char title[48];     /* 作品名（UTF-8・INFO の title。無ければファイル名） */
    int has_ja;         /* 訳・語彙・ふりがなの節が揃っている = 日本語で遊べる */
    uint16_t release;   /* story の識別 */
    char serial[7];
    uint16_t checksum;
    /* ↓ pack_open で埋まる */
    uint8_t *ram;       /* VM が読み書きする story の作業域（story を写したもの） */
    uint32_t len;       /* story の長さ */
    uint8_t *init;      /* 初期イメージの動的領域（セーブの差分の相手）。★story の全部は持たない */
} ZmPack;

/* カレントディレクトリの作品を並べる（パックが先・名前の順）。戻り値 = 数 */
int pack_list(ZmPack *w, int max);

/* 選んだ作品の story を読み、ja なら訳・語彙・ふりがなの表も読む。
 * 読めたら 0。読めなければ理由（ASCII —— DOS の画面に出すので）。 */
const char *pack_open(ZmPack *pk, int ja);

#endif
