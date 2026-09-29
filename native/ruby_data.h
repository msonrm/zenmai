/* gen_ruby.py が生成（ctab.py）。手で編集しない */
#ifndef RUBY_DATA_H
#define RUBY_DATA_H
typedef struct { unsigned int bo; unsigned short bl; unsigned int yo; unsigned short yl; } RbSeg;
typedef struct { unsigned int ko; unsigned short kl; unsigned short seg_off; unsigned short seg_n; } RbKey;
#define RB_SCHEMA 0x8069E5D0u   /* パックの節と突き合わせる（ctab.py） */
extern const unsigned short *rb_pool; extern unsigned rb_pool_n;
extern const RbSeg *rb_segs; extern unsigned rb_segs_n;
extern const RbKey *rb_keys; extern unsigned rb_keys_n;
#define RB_KEY_N rb_keys_n
#endif
