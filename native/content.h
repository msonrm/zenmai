/* gen_data.py が生成。手で編集しない(定義は content_data.c) */
#ifndef CONTENT_H
#define CONTENT_H
typedef struct { unsigned char blen, rlen; unsigned short boff, roff; } Item;
typedef struct { unsigned short off, cnt; } Line;  /* cnt==0 は空行 */
typedef struct { unsigned short cmd_off, cmd_len, line_off, line_cnt; } Turn;
enum { TURN_N = 7, EXTRA_NOTICE_LEN = 24 };
extern const unsigned short pool[];
extern const Item citems[];
extern const Line clines[];
extern const Turn cturns[TURN_N];
extern const unsigned short extra_notice[EXTRA_NOTICE_LEN];
#endif
