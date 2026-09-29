/* gen_translate.py が生成（ctab.py）。手で編集しない。PC-98 版: 表はパックの節から読む（tabload.c） */
#include <stddef.h>
#include "tabload.h"
#include "translate_data.h"
const char *tr_en_pool; unsigned tr_en_pool_n;
const unsigned short *tr_ja_pool; unsigned tr_ja_pool_n;
const TrPair *tr_exact; unsigned tr_exact_n;
const TrPair *tr_props; unsigned tr_props_n;
const TrPair *tr_words; unsigned tr_words_n;
const TrSeg *tr_segs; unsigned tr_segs_n;
const TrPat *tr_pats; unsigned tr_pats_n;
const TrPair *tr_notrans; unsigned tr_notrans_n;
const TlTab translate_tabs[] = {
  {"tr_en_pool", (const void **)&tr_en_pool, &tr_en_pool_n, sizeof(char), 1, {{0,1}}},
  {"tr_ja_pool", (const void **)&tr_ja_pool, &tr_ja_pool_n, sizeof(unsigned short), 1, {{0,2}}},
  {"tr_exact", (const void **)&tr_exact, &tr_exact_n, sizeof(TrPair), 4, {{offsetof(TrPair,eo),4},{offsetof(TrPair,el),2},{offsetof(TrPair,jo),4},{offsetof(TrPair,jl),2}}},
  {"tr_props", (const void **)&tr_props, &tr_props_n, sizeof(TrPair), 4, {{offsetof(TrPair,eo),4},{offsetof(TrPair,el),2},{offsetof(TrPair,jo),4},{offsetof(TrPair,jl),2}}},
  {"tr_words", (const void **)&tr_words, &tr_words_n, sizeof(TrPair), 4, {{offsetof(TrPair,eo),4},{offsetof(TrPair,el),2},{offsetof(TrPair,jo),4},{offsetof(TrPair,jl),2}}},
  {"tr_segs", (const void **)&tr_segs, &tr_segs_n, sizeof(TrSeg), 4, {{offsetof(TrSeg,off),4},{offsetof(TrSeg,len),2},{offsetof(TrSeg,kind),1},{offsetof(TrSeg,slot),1}}},
  {"tr_pats", (const void **)&tr_pats, &tr_pats_n, sizeof(TrPat), 4, {{offsetof(TrPat,seg_off),2},{offsetof(TrPat,en_n),2},{offsetof(TrPat,ja_n),2},{offsetof(TrPat,has_echo),2}}},
  {"tr_notrans", (const void **)&tr_notrans, &tr_notrans_n, sizeof(TrPair), 4, {{offsetof(TrPair,eo),4},{offsetof(TrPair,el),2},{offsetof(TrPair,jo),4},{offsetof(TrPair,jl),2}}},
};
const int translate_tabs_n = 8;
const unsigned long translate_schema = TR_SCHEMA;
