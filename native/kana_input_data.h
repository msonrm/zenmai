/* gen_kana_input.py が生成。手で編集しない */
#ifndef KANA_INPUT_DATA_H
#define KANA_INPUT_DATA_H
/* ローマ字 1 行: 打った列 → 出る字（最大 3・0 詰め）と、次へ残す列（"" = 無し。`tch` → っ + `ch`） */
typedef struct { const char *in; unsigned short out[3]; const char *pend; } KiRomaji;
typedef struct { unsigned short base; unsigned char mark; unsigned short to; } KiDakuten;
enum { KI_ROMAJI_N = 323, KI_DAKUTEN_N = 26 };
extern const KiRomaji ki_romaji[KI_ROMAJI_N];
extern const unsigned short ki_hankana[0xE0 - 0xA1];
extern const KiDakuten ki_dakuten[KI_DAKUTEN_N];
#endif
