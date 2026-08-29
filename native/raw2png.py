#!/usr/bin/env python3
"""SDL ビルドが書き出した画面（RGB555 の生バイト）を PNG にする。

★これは**実機の画面そのもの**として使える —— PS1 版と SDL 版が同じ台本で
画素一致することを test-sdl.sh が示しており、その一致は R36H の実機でも確かめてある。
つまりカメラで撮る必要も、機体のスクリーンショット機能も要らない。

5bit → 8bit の伸ばし方は sim.py の PNG 出力（`(p & 31) << 3`）に揃える。
そうしないと、同じ画面なのに片方だけ暗く出る。

  raw2png.py <in.raw> <out.png>

生バイトの作り方:
  ZENMAI_SCRIPT=dual_ja.script ZENMAI_STOP=2200 ZENMAI_RAW=out.raw ./zenmai-zork
"""
import sys
from pathlib import Path

W, H = 640, 480


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    data = Path(sys.argv[1]).read_bytes()
    if len(data) != W * H * 2:
        sys.exit(f"大きさが違う: {len(data)}（期待 {W * H * 2}）")
    from PIL import Image
    img = Image.new("RGB", (W, H))
    img.putdata([
        (((p := data[i] | (data[i + 1] << 8)) & 31) << 3,
         ((p >> 5) & 31) << 3,
         ((p >> 10) & 31) << 3)
        for i in range(0, len(data), 2)
    ])
    img.save(sys.argv[2])
    print(f"{sys.argv[2]} ({W}x{H})")


if __name__ == "__main__":
    main()
