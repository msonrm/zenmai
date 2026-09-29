/* gen_ruby.py が生成（ctab.py）。手で編集しない。PC-98 版: 表はパックの節から読む（tabload.c） */
#include <stddef.h>
#include "tabload.h"
#include "ruby_data.h"
const unsigned short *rb_pool; unsigned rb_pool_n;
const RbSeg *rb_segs; unsigned rb_segs_n;
const RbKey *rb_keys; unsigned rb_keys_n;
const TlTab ruby_tabs[] = {
  {"rb_pool", (const void **)&rb_pool, &rb_pool_n, sizeof(unsigned short), 1, {{0,2}}},
  {"rb_segs", (const void **)&rb_segs, &rb_segs_n, sizeof(RbSeg), 4, {{offsetof(RbSeg,bo),4},{offsetof(RbSeg,bl),2},{offsetof(RbSeg,yo),4},{offsetof(RbSeg,yl),2}}},
  {"rb_keys", (const void **)&rb_keys, &rb_keys_n, sizeof(RbKey), 4, {{offsetof(RbKey,ko),4},{offsetof(RbKey,kl),2},{offsetof(RbKey,seg_off),2},{offsetof(RbKey,seg_n),2}}},
};
const int ruby_tabs_n = 3;
const unsigned long ruby_schema = RB_SCHEMA;
