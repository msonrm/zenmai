#!/usr/bin/env python3
"""PS1 の画面（PNG）と SDL の画面（RGB555 の生バイト）を突き合わせる。

sim.py は VRAM を PNG に落とすとき 5bit を 3bit 左シフトして 8bit にしている
（`(p & 31) << 3`）ので、★**戻すときも同じ落とし方に揃える**（>> 3）。
そうしないと、値そのものは合っているのに丸めの差で全画素が食い違って見える。

  cmp_frame.py <ps1.png> <sdl.raw>   → 一致なら 0、違えば 1（差分の要約を出す）
"""
import sys
from pathlib import Path

W, H = 640, 480


def load_png(path):
    from PIL import Image
    img = Image.open(path).convert("RGB")
    if img.size != (W, H):
        sys.exit(f"寸法が違う: {img.size}")
    return [(r >> 3, g >> 3, b >> 3) for r, g, b in img.getdata()]


def load_raw(path):
    data = Path(path).read_bytes()
    if len(data) != W * H * 2:
        sys.exit(f"生バイトの大きさが違う: {len(data)}（期待 {W * H * 2}）")
    out = []
    for i in range(0, len(data), 2):
        p = data[i] | (data[i + 1] << 8)
        out.append((p & 31, (p >> 5) & 31, (p >> 10) & 31))
    return out


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    a, b = load_png(sys.argv[1]), load_raw(sys.argv[2])
    diff = [i for i, (x, y) in enumerate(zip(a, b)) if x != y]
    if not diff:
        return 0
    # ★食い違いは「どこが」まで出す。全面ずれ（色の並び違い）と
    #   一部ずれ（描画の差）は原因がまったく別なので、数と位置で切り分けられる。
    ys = sorted({i // W for i in diff})
    print(f"画素 {len(diff):,} 点 食い違い / 行 {ys[0]}..{ys[-1]}", file=sys.stderr)
    for i in diff[:3]:
        print(f"  ({i % W},{i // W}) PS1={a[i]} SDL={b[i]}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
