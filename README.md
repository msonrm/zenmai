# 🌀 Zenmai

**A Z-machine you read in Japanese and type with a game controller.**

**日本語で読み、ゲームコントローラで打つ Z-machine。**

**Play in the browser / ブラウザで遊ぶ → https://zenmai.pages.dev/**

---

## What it is / これは何か

The **Z-machine** is the virtual machine Infocom built in 1979 to run its text adventures.
Zenmai runs it with a Japanese layer on top: the story's text is translated as it is printed
(with furigana over the kanji), and what you type in Japanese is turned into a command the original game understands.
*Zork I* comes with it as the sample story, and *Zork II* and *Zork III* can be played in Japanese too (beta).
No keyboard is required.

**Z-machine** は、Infocom が 1979 年にテキストアドベンチャーを動かすために作った仮想機械。
Zenmai はそれに日本語の層をかぶせて動かす —— 物語の文は出てくるそばから日本語になり（漢字にはふりがな）、
日本語で打った言葉は原作が受け付ける命令になって届く。
同梱の見本の作品は『Zork I』で、『Zork II』『Zork III』も日本語で遊べる（β）。**キーボードは要らない。**

## Where it runs / 動く場所

| | |
|---|---|
| **Web** | **https://zenmai.pages.dev/** — keyboard, gamepad, or flick input on a phone. Type in hiragana, kanji, or English. *Zork I*, *Zork II* (β), *Zork III* (β). |
| **PC-98** (MS-DOS, beta) | Romaji or kana-key input, furigana, a Bach canon at the title screen. *Zork II* / *III* in English. [Releases](https://github.com/msonrm/zenmai/releases/tag/pc98-v0.5.1-beta) |
| **PortMaster** (SDL2, handhelds) | The gamepad input on Linux handhelds (R36H and the like). Being prepared for the PortMaster catalog. |
| **PlayStation** (frozen) | Where it started. No longer developed; the last build stays in [Releases](https://github.com/msonrm/zenmai/releases/tag/ps1-v1.5.0). |

| | |
|---|---|
| **Web** | **https://zenmai.pages.dev/** —— キーボード / ゲームパッド / スマホのフリック入力。ひらがな・漢字まじり・英語のどれでも打てる。『Zork I』『Zork II』（β）『Zork III』（β） |
| **PC-98**（MS-DOS・beta） | ローマ字 / カナキーで打ち、漢字の上にふりがな。起動画面では PMD でバッハのカノン。『Zork II』『Zork III』は原作の英語のまま。[Releases](https://github.com/msonrm/zenmai/releases/tag/pc98-v0.5.1-beta) |
| **PortMaster**（SDL2・携帯機） | Linux の携帯ゲーム機（R36H など）でゲームパッド入力。PortMaster の一覧に載せる準備中 |
| **PlayStation**（凍結） | ここから始まった。開発は止めていて、最後の版は [Releases](https://github.com/msonrm/zenmai/releases/tag/ps1-v1.5.0) に残してある |

### In the browser / ブラウザ版

- **Continue where you left off** — the game remembers every move in the browser, so closing the tab loses nothing.
  “Save” keeps one extra point to go back to.
- **Print the session on a receipt** — connect a 58 mm thermal printer (USB or Bluetooth; Chrome-based browsers only),
  type *いんじする* (“print”), and each move comes out as you play, after a header with QR codes.
- 「**前回の続きから**」—— 打つたびにブラウザが覚えているので、閉じても次に開くと続きから始まる。「保存する」は戻りたい地点を 1 つ残すためのもの。
- 「**レシートに印字**」—— 58mm の感熱プリンタにつなぎ（USB / Bluetooth・Chrome 系のブラウザだけ）、「いんじする」と打つと、
  QR つきの頭書きのあと、遊んだ記録が 1 手ごとに出てくる。

## See it running / 動いているところ

[<img src="https://img.youtube.com/vi/rmKmOpTu2to/hqdefault.jpg" width="380" alt="I ported Zork to the PlayStation 1">](https://youtu.be/rmKmOpTu2to)
[<img src="https://img.youtube.com/vi/08_QX_4cLYY/hqdefault.jpg" width="380" alt="テキストアドベンチャーの代表作Zorkをプレステ1で動くようにした">](https://youtu.be/08_QX_4cLYY)

The PlayStation build, where it started (English · 日本語).

始まりの PlayStation 版（英語・日本語）。

## Typing without a keyboard / キーボード無しで文字を打つ

Text adventures need free text. Consoles have no keyboard. Zenmai's answer is not a
menu of commands but **a way to actually type**, using nothing but a D-pad and four buttons.

テキストアドベンチャーには自由な文章が要る。家庭用機にはキーボードが無い。
Zenmai の答えは**コマンドの一覧を出すこと**ではなく、**本当に打てるようにすること**。
使うのは十字キーと 4 つのボタンだけ。

### English — the telephone keypad / 英語 —— プッシュホン

The D-pad picks a key, a face button picks which letter on that key.
**The assignment is the one from a telephone keypad** (`2 = ABC`, `7 = PQRS`) —
the same one that phones used for text before smartphones.

十字キーでキーを選び、フェイスボタンでそのキーの何文字目かを選ぶ。
割り当ては**プッシュホンのそれ**（`2 = ABC`, `7 = PQRS`）——
スマートフォン以前の携帯電話が文字入力に使っていたものと同じ。

### Japanese — flick input, unfolded / 日本語 —— フリック入力を開く

The left hand picks the consonant row, the right hand picks the vowel — the same
five-way arrangement as **flick input** on a Japanese phone keyboard. The flick directions
land on the face buttons **in the positions they already point to**: left = い,
up = う, right = え, down = お, the right shoulder (no flick) = あ.
If you can type on a phone, you already know this.

左手で行を選び、右手で段を選ぶ。並びはスマートフォンの**フリック入力**そのまま。
フリックの向きは、**そのまま指している位置のフェイスボタン**に来る ——
左 = い / 上 = う / 右 = え / 下 = お / 右の肩（フリック無し）= あ。
**スマートフォンで打てる人は、もう覚えている。**

### Where it comes from / 出どころ

The scheme is not new to this port. It comes from
**[GIME](https://github.com/msonrm/GIME)** — a gamepad text-entry app that types Japanese,
Korean, English (T9), Chinese and Devanagari from a controller. The full button mapping,
including the analog sticks, is
**[`docs/gamepad-mapping.md`](https://github.com/msonrm/GIME/blob/main/docs/gamepad-mapping.md)**.
Zenmai ports that state machine to C and to the browser.

この方式はこの移植のために作ったものではなく、
**[GIME](https://github.com/msonrm/GIME)** —— ゲームパッドで日本語・韓国語・英語（T9）・
中国語・デーヴァナーガリーを打つアプリ —— から来ている。
アナログスティックまで含めたボタン割り当ての全体は
**[`docs/gamepad-mapping.md`](https://github.com/msonrm/GIME/blob/main/docs/gamepad-mapping.md)**。
Zenmai はその状態機械を C とブラウザへ移植している。

## Why it might matter / この先

The scheme needs only a directional input and four buttons — no pointer, no virtual keyboard,
no line of sight to a rendered key. That is also the shape of a **VR controller**, where
poking at a floating keyboard is still the state of the art. The same layout transfers.

要るのは方向入力と 4 ボタンだけ。ポインタも仮想キーボードも、キーを見て狙う動作も要らない。
これは **VR コントローラ**の形でもある —— 宙に浮いたキーボードを突く方式がいまだに主流の場所だ。
**この割り当てはそのまま持っていける。**

## Documents / 文書

- Design, how the translation layer works, the vocabulary structure, and the tests: **[`docs/overview.md`](docs/overview.md)**
- How the translations and the Japanese vocabulary are made (and the traps): **[`docs/translation-guide.md`](docs/translation-guide.md)**
- PC-98 build: **[`docs/pc98-port-plan.md`](docs/pc98-port-plan.md)** · PortMaster: **[`docs/portmaster-testing.md`](docs/portmaster-testing.md)** ·
  PlayStation: **[`docs/ps1-implementation-notes.md`](docs/ps1-implementation-notes.md)**

- 設計・訳の層のしくみ・語彙の構造・検査 —— **[`docs/overview.md`](docs/overview.md)**
- 訳と日本語の語彙の作り方（踏んだ罠も）—— **[`docs/translation-guide.md`](docs/translation-guide.md)**
- PC-98 版 **[`docs/pc98-port-plan.md`](docs/pc98-port-plan.md)**・PortMaster **[`docs/portmaster-testing.md`](docs/portmaster-testing.md)**・
  PlayStation **[`docs/ps1-implementation-notes.md`](docs/ps1-implementation-notes.md)**

## Support / 応援

If this was fun to read about or to play, you can buy me a coffee.

読んで、あるいは遊んで面白かったら、コーヒーをおごってもらえると励みになります。

[<img src="https://storage.ko-fi.com/cdn/kofi3.png?v=3" height="36" alt="Buy Me a Coffee at ko-fi.com">](https://ko-fi.com/msonrm)

## License / ライセンス

MIT for the code in this repository — **except `native/glyphs.h`**, which holds glyph bitmaps
extracted from the **KH Dot Font** and is therefore under the
**[SIL Open Font License 1.1](native/vendor/kh-dotfont/LICENSE)**
(Copyright (c) Keitarou Hiraki, Font Silo. 1990-2015 — see
[`native/vendor/kh-dotfont/`](native/vendor/kh-dotfont/README.md)).

*Zork I*, *Zork II* and *Zork III* come from the ZIL sources released under MIT
([zork1](https://github.com/historicalsource/zork1), [zork2](https://github.com/historicalsource/zork2),
[zork3](https://github.com/historicalsource/zork3); Microsoft, 2025). The Z-machine implementations are
[ZVM](https://github.com/curiousdannii/ifvms.js) (web; MIT) and
[MojoZork](https://github.com/icculus/mojozork) (PlayStation, SDL, PC-98; zlib).
The receipt QR codes are drawn with [qrcode-generator](https://github.com/kazuhikoarase/qrcode-generator) (web; MIT).
Zork is a trademark of Infocom; the rights are now held by Microsoft.
**This project is not affiliated with either, and does not use the title as its own name or brand.**

本リポジトリのコードは MIT —— ただし ★**`native/glyphs.h` だけは例外**。
**KH ドットフォント**から抜いた字形のビットマップなので
**[SIL Open Font License 1.1](native/vendor/kh-dotfont/LICENSE)** の下にある
（Copyright (c) Keitarou Hiraki, Font Silo. 1990-2015 —— 出どころと判断は
[`native/vendor/kh-dotfont/`](native/vendor/kh-dotfont/README.md)）。

『Zork I』『Zork II』『Zork III』は MIT で公開された ZIL ソース由来
（[zork1](https://github.com/historicalsource/zork1)・[zork2](https://github.com/historicalsource/zork2)・
[zork3](https://github.com/historicalsource/zork3)・Microsoft・2025）。Z-machine の実装は
[ZVM](https://github.com/curiousdannii/ifvms.js)（web・MIT）と
[MojoZork](https://github.com/icculus/mojozork)（PS1・SDL・PC-98・zlib）。
レシートの QR は [qrcode-generator](https://github.com/kazuhikoarase/qrcode-generator)（web・MIT）で描いている。
Zork は Infocom の商標で、現在の権利者は Microsoft。
**このプロジェクトはどちらとも関係が無く、作品名を自分の名称やブランドには用いない。**
