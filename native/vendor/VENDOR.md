MojoZork f94c3104 (https://github.com/icculus/mojozork, zlib license)
2026-08-19 vendoring。埋め込みは multizorkd 方式(#include + opcode 表差し替え)。

パッチ(2026-08-19): 非アラインの uint16 ロード 2 箇所を memcpy 化(opcode_call の
ローカル既定値読み・opcode_loadw)。ARM/x86 では動くが、MIPS(PS1)では例外/誤読になる
移植性バグ。挙動は同一(生バイトコピー)。upstream への報告価値あり。

パッチ(2026-08-20): print_zscii のスタックバッファ 512→2048(Zork I の最長文字列
約 1.3KB が malloc 経路に落ちてフリーズしていた —— PS1 側の malloc は当初スタブだった。
lib.c に LIFO アリーナ malloc も実装済みで二重に安全)。
