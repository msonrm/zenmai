#ifndef SAVE_DOS_H
#define SAVE_DOS_H

/* セーブの名前を作品に合わせる（base = パックの名前・拡張子なし → base.SAV）。card.h の口より前に呼ぶ */
void save_dos_name(const char *base);

#endif
