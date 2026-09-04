# Licences

What each file in this folder covers, and what it does not.

## Bundled in `zenmai-zork.aarch64`

| File | Covers |
|---|---|
| `zenmai-MIT.txt` | Zenmai itself — the C, the translation layer, the input state machine. |
| `mojozork-zlib.txt` | [MojoZork](https://github.com/icculus/mojozork) by Ryan C. Gordon — the Z-machine. Vendored into the binary; see `VENDOR.md` for the patches. |
| `zork1-MIT.txt` | **Zork I.** The story file is compiled from the ZIL sources that Microsoft / Activision released under MIT in November 2025 ([historicalsource/zork1](https://github.com/historicalsource/zork1)), and it is baked into the executable. |

## Bundled as `zenmai.otf`

| File | Covers |
|---|---|
| `noto-cjk-OFL.txt` | **Noto Sans CJK JP**, subsetted. This is the font the port actually draws with. Name IDs 13/14 (the OFL notice and URL) are preserved inside the font file itself. |
| `kh-dotfont-OFL.txt` | **KH Dot Font.** Not drawn with here — the PlayStation build bakes its 24px bitmaps, this build does not. It is listed because its character coverage decided which glyphs the subset above keeps. |

## Not bundled — provided by the firmware

`libSDL2-2.0.so.0` (zlib) and `libfreetype.so.6` (FTL or GPLv2, at FreeType's option) are
linked dynamically against whatever the CFW ships. No copy of either is in this port, so
their licence texts are not reproduced here.
