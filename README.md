# 🌀 Zenmai

**Zork I on the original PlayStation — read in Japanese, typed with a game controller.**

**初代 PlayStation で動く Zork I —— 日本語で読み、ゲームコントローラで打つ。**

[**Download / ダウンロード**](https://github.com/msonrm/zenmai/releases) → `zenmai-zork.psexe`

---

## What it is / これは何か

A port of the **Z-machine** — Infocom's 1979 virtual machine, the thing that runs *Zork* —
to the original PlayStation, with a Japanese layer over it. No keyboard required.

**Z-machine の PS1 移植。** Z-machine は Infocom が 1979 年に作った仮想機械で、『Zork』を
動かしていたもの。それを初代 PlayStation に載せ、日本語の層をかぶせてある。
**キーボードは要らない。**

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
land on the face buttons **in the positions they already point to**: □ (left) = い,
△ (up) = う, ○ (right) = え, ✕ (down) = お, R1 (no flick) = あ.
If you can type on a phone, you already know this.

左手で行を選び、右手で段を選ぶ。並びはスマートフォンの**フリック入力**そのまま。
フリックの向きは、**そのまま指している位置のフェイスボタン**に来る ——
□（左）= い / △（上）= う / ○（右）= え / ✕（下）= お / R1（フリック無し）= あ。
**スマートフォンで打てる人は、もう覚えている。**

## Why it might matter / この先

The scheme needs only a directional input and four buttons — no pointer, no virtual keyboard,
no line of sight to a rendered key. That is also the shape of a **VR controller**, where
poking at a floating keyboard is still the state of the art. The same layout transfers.

要るのは方向入力と 4 ボタンだけ。ポインタも仮想キーボードも、キーを見て狙う動作も要らない。
これは **VR コントローラ**の形でもある —— 宙に浮いたキーボードを突く方式がいまだに主流の場所だ。
**この割り当てはそのまま持っていける。**

## Also / そのほか

The same game runs in a browser at **https://zenmai.pages.dev/**
(keyboard, gamepad, or flick input on a phone).

同じものはブラウザでも動く —— **https://zenmai.pages.dev/**
（キーボード / ゲームパッド / スマホのフリック入力）。

Design notes, how the translation layer works, the vocabulary structure, and the test suite:
**[`docs/overview.md`](docs/overview.md)**.
PlayStation build and implementation notes: **[`docs/ps1-implementation-notes.md`](docs/ps1-implementation-notes.md)**.

設計・訳の層のしくみ・語彙の構造・検査は **[`docs/overview.md`](docs/overview.md)**、
PS1 のビルドと実装ノートは **[`docs/ps1-implementation-notes.md`](docs/ps1-implementation-notes.md)**。

## License / ライセンス

MIT for the code in this repository. *Zork I* comes from the
[ZIL sources released under MIT](https://github.com/historicalsource/zork1) (Microsoft, 2025);
the Z-machine implementations are [ZVM](https://github.com/curiousdannii/ifvms.js) (web)
and [MojoZork](https://github.com/icculus/mojozork) (PlayStation).
Zork is a trademark of Infocom; the rights are now held by Microsoft.
**This project is not affiliated with either, and does not use the title as its own name or brand.**

本リポジトリのコードは MIT。『Zork I』は
[MIT で公開された ZIL ソース](https://github.com/historicalsource/zork1)（Microsoft・2025）由来。
Z-machine の実装は [ZVM](https://github.com/curiousdannii/ifvms.js)（web）と
[MojoZork](https://github.com/icculus/mojozork)（PS1）。
Zork は Infocom の商標で、現在の権利者は Microsoft。
**このプロジェクトはどちらとも関係が無く、作品名を自分の名称やブランドには用いない。**
