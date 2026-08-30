/* gen_translate.py が生成。手で編集しない */
#ifndef TRANSLATE_DATA_H
#define TRANSLATE_DATA_H
typedef struct { unsigned int eo; unsigned short el; unsigned int jo; unsigned short jl; } TrPair;
typedef struct { unsigned int off; unsigned short len; unsigned char kind; unsigned char slot; } TrSeg;
typedef struct { unsigned short seg_off, en_n, ja_n, has_echo; } TrPat;
enum { TRK_LIT, TRK_HOLE, TRK_QHOLE, TRK_JLIT, TRK_JREF };
enum { TRF_ECHO = 1, TRF_VERB = 2, TRF_SAID = 4 };
enum { TR_EXACT_N = 1220, TR_PROPS_N = 336, TR_WORDS_N = 444, TR_PATS_N = 253, TR_NT_N = 30 };
extern const char tr_en_pool[];
extern const unsigned short tr_ja_pool[];
extern const TrPair tr_exact[TR_EXACT_N];
extern const TrPair tr_props[TR_PROPS_N];
extern const TrPair tr_words[TR_WORDS_N];
extern const TrSeg tr_segs[];
extern const TrPat tr_pats[TR_PATS_N];
extern const TrPair tr_notrans[TR_NT_N];
#endif
