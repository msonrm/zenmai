/* gen_ruby.py が生成。手で編集しない */
#ifndef RUBY_DATA_H
#define RUBY_DATA_H
typedef struct { unsigned int bo; unsigned short bl; unsigned int yo; unsigned short yl; } RbSeg;
typedef struct { unsigned int ko; unsigned short kl; unsigned short seg_off, seg_n; } RbKey;
enum { RB_KEY_N = 389 };
extern const unsigned short rb_pool[];
extern const RbSeg rb_segs[];
extern const RbKey rb_keys[RB_KEY_N];
#endif
