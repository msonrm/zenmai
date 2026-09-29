/* gen_translate.py が生成（ctab.py）。手で編集しない */
#ifndef TRANSLATE_DATA_H
#define TRANSLATE_DATA_H
typedef struct { unsigned int eo; unsigned short el; unsigned int jo; unsigned short jl; } TrPair;
typedef struct { unsigned int off; unsigned short len; unsigned char kind; unsigned char slot; } TrSeg;
typedef struct { unsigned short seg_off; unsigned short en_n; unsigned short ja_n; unsigned short has_echo; } TrPat;
enum { TRK_LIT, TRK_HOLE, TRK_QHOLE, TRK_JLIT, TRK_JREF };
enum { TRF_ECHO = 1, TRF_VERB = 2, TRF_SAID = 4 };
#define TR_SCHEMA 0x5C8075E0u   /* パックの節と突き合わせる（ctab.py） */
extern const char *tr_en_pool; extern unsigned tr_en_pool_n;
extern const unsigned short *tr_ja_pool; extern unsigned tr_ja_pool_n;
extern const TrPair *tr_exact; extern unsigned tr_exact_n;
#define TR_EXACT_N tr_exact_n
extern const TrPair *tr_props; extern unsigned tr_props_n;
#define TR_PROPS_N tr_props_n
extern const TrPair *tr_words; extern unsigned tr_words_n;
#define TR_WORDS_N tr_words_n
extern const TrSeg *tr_segs; extern unsigned tr_segs_n;
extern const TrPat *tr_pats; extern unsigned tr_pats_n;
#define TR_PATS_N tr_pats_n
extern const TrPair *tr_notrans; extern unsigned tr_notrans_n;
#define TR_NT_N tr_notrans_n
#endif
