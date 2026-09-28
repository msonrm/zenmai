/* gen_canon.py が生成。手で編集しない */
#ifndef CANON_DATA_H
#define CANON_DATA_H
#include <stdint.h>
enum { CANON_N0 = 195, CANON_N1 = 180, CANON_UNITS = 1280, CANON_LEGATO = 0x80 };
/* 1 音 = { MIDI の音の番号（0 = 休符・bit7 = 弾き直さない）, 長さ（32 分音符の数）} */
extern const uint8_t canon_v0[CANON_N0][2];   /* 先の声 */
extern const uint8_t canon_v1[CANON_N1][2];   /* 後の声 */
#endif
