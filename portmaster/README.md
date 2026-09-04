# Zenmai

**Zork I (1979), read and typed in Japanese — with a Japanese input method driven
entirely by the gamepad.** English play is included too, typed with T9.

No keyboard. No system IME. Hold a consonant row with the left hand, tap a vowel with
the right, and the kana appears — the same idea as a chorded keyboard, laid onto a
D-pad and four face buttons.

## Thanks

- **Ryan C. Gordon (icculus)** for [MojoZork](https://github.com/icculus/mojozork) —
  the Z-machine interpreter this runs on, small and clear enough to port to a
  PlayStation and read end to end. zlib licence.
- **Infocom** for *Zork I* (1979), and everyone who has kept the Z-machine
  documented for forty-odd years.
- **num_kadoma** for the [KH Dot Font](http://kanji-database.sourceforge.net/fonts/khdotfont.html),
  which is where the Japanese glyphs come from. SIL Open Font License 1.1.
- **The PortMaster team**, for making "it runs on the handheld" a thing one person
  can actually finish.

## Starting

The first screen asks which language to play in: **ENGLISH** on top and selected —
the original is English — with **日本語** below it. `↑↓` to move, any face button or
START to confirm. To change your mind later, start the port again.

## Controls

Every mapping below was read out of the source (`native/main.c` `pad_row` / `pad_vowel`
and `native/tables.h`), not written from memory. **The game also carries the chart
itself** — press START on an empty line to open the options menu, then pick
ひらがな入力方法.

### Picking a row (both languages)

The left hand chooses the row. **L1 shifts to the second half.**

| | none | ← | ↑ | → | ↓ |
|---|---|---|---|---|---|
| **plain** | あ | か | さ | た | な |
| **+ L1** | は | ま | や | ら | わ |

| | none | ← | ↑ | → | ↓ |
|---|---|---|---|---|---|
| **plain** | `1 . , - ␣` | abc | def | ghi | jkl |
| **+ L1** | mno | pqrs | tuv | wxyz | `0` |

### Picking the letter (both languages)

The right hand chooses which of the five. Press it together with the row.

| R1 | □ | △ | ○ | ✕ |
|---|---|---|---|---|
| あ | い | う | え | お |
| 1st | 2nd | 3rd | 4th | 5th |

So `←` + `✕` is こ, and `L1` + `↑` + `△` is ゆ. In English, `←` + `□` is **a**, and
`✕` on the plain row (no direction) is a **space**.

**The symbols are positions, not labels.** On a handheld with Nintendo-style
lettering that means **B = ✕ (bottom), A = ○ (right), Y = □ (left), X = △ (top)** —
which is what nearly every PortMaster device prints. On a desktop Xbox pad the two
pairs come out swapped; there is no way to tell the two apart at runtime, so the
build assumes the handheld.

### Editing and sending

| | |
|---|---|
| **START** / **L3** / right stick ↓ | send the line |
| SELECT | dakuten ゛ / handakuten ゜ (toggles on the last kana) |
| R2 tap | ん → を → んを (tap again to cycle) |
| R2 **hold** | backspace |
| Right stick | ↑ dakuten · ← backspace · → ー · ↓ send |
| Left stick ← → | move the cursor |
| Left stick ↑ ↓, or L2 + ↑ ↓ | scroll back through the transcript |
| **START on an empty line** | open the options menu |

In the options menu: ↑↓ to choose, **any face button** to confirm, START to close.
It has four entries — three pages to read (how to type, system commands, licence)
and **Quit**.

### Leaving

`やめる` / `quit` then `はい` / `yes` **exits the port** and returns you to the
launcher. (On the PlayStation build the same command drops back to the language
menu — there is nowhere else to go on a console.) To switch language, start the
port again.

**Quit** in the options menu is a shortcut for the same thing, not a separate exit:
it types the word for you, so the game still asks *do you really want to quit?* and
still wants an answer. The original is what runs; the menu only saves you the typing.

### Saving

Type `ほぞんする` / `save` to save and `さいかいする` / `restore` to come back. There
is **one slot**, kept beside the port as `zenmai.sav` — no naming screen, because
asking for a filename would break the "gamepad only" premise.

## Building

The port is plain C plus SDL2 — no engine, no runtime.

```sh
git clone https://github.com/msonrm/zenmai
cd zenmai
sh portmaster/build-port.sh          # -> portmaster/zenmai.zip
sh portmaster/build-port.sh --check  # -> the glibc versions the binary requires
```

The same C also builds a PlayStation executable (`native/build.sh`). The two differ
behind exactly two seams:

- [`native/plat.h`](https://github.com/msonrm/zenmai/blob/main/native/plat.h) — the
  nine functions that touch the machine (screen and pad).
- [`native/glyph.h`](https://github.com/msonrm/zenmai/blob/main/native/glyph.h) — the
  five that draw letters. The PlayStation uses baked bitmaps; this port uses FreeType,
  which is why the English text here is proportional and the kanji are antialiased.

Everything above those — layout, wrapping, ruby, history, the input state machine, the
translation — is the same code. There is a test that builds this port *with* the baked
glyphs and checks it renders pixel-for-pixel identically to the PlayStation build.

**On glibc:** the binary asks for whatever glibc it was built against, and a newer
one is not there on older firmware. So `build-port.sh` builds inside a container
([`portmaster/Containerfile`](https://github.com/msonrm/zenmai/blob/main/portmaster/Containerfile),
Ubuntu 20.04 = glibc 2.31 — what ArkOS and AmberELEC are built on), which also runs
on newer firmware such as dArkOS/dArkOSen. It needs `podman`; pass `ZM_HOST_BUILD=1`
to use your own toolchain instead. `--check` fails if anything above 2.31 crept in.

## Licence

MIT for the code, with two exceptions, both **SIL Open Font License 1.1**:

- `zenmai/zenmai.otf` — a subset of **Noto Sans CJK JP** (7,623 characters), bundled
  so the port does not depend on the firmware having a CJK font.
- `native/glyphs.h` in the source tree — glyph bitmaps extracted from the **KH Dot
  Font**, used by the PlayStation build and by the test that checks the two builds
  render identically.

MojoZork is zlib. Full texts are in `zenmai/licenses/`.
