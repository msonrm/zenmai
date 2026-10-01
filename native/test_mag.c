/* pc98_mag の復号を、QuuBee のデコーダ（test-mag.js）と突き合わせるための印字: 引数の .MAG を復号し、
 * 「幅x高さ ハッシュ」を出す（ハッシュは画素ごとの R,G,B,255 を 8 ビットに広げたものの FNV-1a）。 */
#include <stdio.h>
#include "pc98_mag.h"

int main(int argc, char **argv)
{
    for (int a = 1; a < argc; a++) {
        Mag m;
        if (!mag_load(argv[a], &m)) { printf("%s: 読めない\n", argv[a]); return 1; }
        unsigned long hsh = 2166136261UL;
        for (int y = 0; y < m.h; y++)
            for (int x = 0; x < m.w; x++) {
                const int b = m.px[y * (m.w / 2) + x / 2], i = x & 1 ? b & 15 : b >> 4;
                const unsigned v[4] = { (unsigned)m.pal[i][1] * 17, (unsigned)m.pal[i][0] * 17, (unsigned)m.pal[i][2] * 17, 255 };
                for (int k = 0; k < 4; k++) hsh = (hsh ^ v[k]) * 16777619UL & 0xFFFFFFFFUL;
            }
        printf("%dx%d %08lx\n", m.w, m.h, hsh);
        mag_free(&m);
    }
    return 0;
}
