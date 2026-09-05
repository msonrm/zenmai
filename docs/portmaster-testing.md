# PortMaster 提出のためのテストと記録

> **これは何か**: PortMaster へポートを出すには、**複数の CFW で動かした記録**が要る。
> 要件の原文はこう ——
> *"Pull Requests submitted without documented testing in the #testing-n-dev channel
> will not be accepted."*
> ★**記録の無い PR は却下される。** 実機が要るので、ここだけは人の仕事になる。
>
> 要件の正典は https://portmaster.games/packaging.html 。
> このファイルは「実機を触りながら潰す紙」として書いてある。

## 0. 事前に済んでいること（2026-09-05 時点）

| | |
|---|---|
| `port.json` / `.sh` / `gameinfo.xml` / `README.md` / `licenses/` | 揃っている |
| スクリーンショット | 実プレイ画面・640×480（`portmaster/screenshot.png`） |
| glibc | **GLIBC_2.17 だけ**を要求（`sh portmaster/build-port.sh --check`） |
| 公式の検査 | `tools/build_release.py --do-check` → **Broken: 0**・警告 0 |
| 名前の重複 | 公開中 1,387 ports に `zenmai.zip` / `Zenmai.sh` / `zenmai/` は無い |

★**残っているのはこのファイルの中身だけ**。

## 1. どの CFW を回るか

| CFW | 要否 | 備考 |
|---|---|---|
| **AmberELEC** | 必須 | Ubuntu 20.04 系（glibc 2.31） |
| **ArkOS** | 必須 | 同上。★ここが glibc のいちばん厳しい相手 |
| **ROCKNIX** | 必須 | Panfrost と Malai の**両方**が望ましい |
| muOS | 必須 | |
| Knulli | 任意 | |

解像度は **640×480 が必須**。480×320 / 720×720 / それ以上は任意。
★Zenmai は `SDL_RenderSetLogicalSize(640,480)` で縦横比を保って拡縮するので、
**480×320 の機種では漢字が 15px 相当まで縮む**。読めるかどうかは実際に見ないと分からない
（つらければ「非対応」と書いてよい。要件上は任意）。

## 2. インストール —— ★必ず PortMaster 経由で

★**手で展開しないこと。** 手で展開すると `gamelist.xml` に登録されず、
**Ports 一覧に出ない**（2026-08-30 に実際に踏んだ）。

さらに実機で分かった作法が 2 つある（2026-09-05）:

- ★**ポートのフォルダ直下に `.sh` を置くと、ES がそのフォルダごと Ports 一覧に並べる。**
  データフォルダが裸で出ていたら、たいてい中に `.sh` が紛れ込んでいる
  （PortMaster 自身のフォルダは `gamelist.xml` の
  `<folder><hidden>true</hidden></folder>` で隠してある）
- ★**zip をポートのルートで展開すると `port.json` / `gameinfo.xml` / `README.md` が
  散らばる。** 本来は PortMaster のインストーラが振り分ける

入れ方: zip を `ports/PortMaster/autoinstall/` に置いて PortMaster を起動する
（機種によって場所が違うので、その CFW の作法に従う）。

## 3. チェックリスト（機種ごとに、この順で）

★**通ったことだけでなく、通らなかったことを書く。** レビューが信用するのは後者。

| # | 見ること | 落ちるとしたら |
|---|---|---|
| 1 | **Ports 一覧に Zenmai が出る**（データフォルダが裸で出ていないことも） | 登録漏れ・`.sh` の紛れ込み |
| 2 | **起動する** | ★**glibc**。ここで落ちたら他は全部見なくてよい |
| 3 | 言語メニューが出て、**ENGLISH と 日本語 の両方**に入れる | |
| 4 | ★**ボタン設定が出て、右のボタン 1 回で抜ける** | ★**機種ごとに一番割れる所**。下記参照 |
| 5 | ★**「ひらがな入力方法」の図と、実際に入る字が一致する** | 4 の答え合わせ |
| 6 | 日本語で 1 コマンド通る（例: `ゆうびんばこをあける`） | |
| 7 | 英語で 1 コマンド通る（T9 で `open mailbox`） | |
| 8 | **ほぞんする → さいかいする** が往復する | 保存先の書き込み権限 |
| 9 | メニューの**やめる → はい**でランチャに戻る（固まらない） | |
| 10 | **画面が切れていない**（640×480 以外は縮小されるので上下左右） | |

### ★4 について —— これが今回いちばん知りたいこと

SDL の `A/B/X/Y` は**機体に印刷された札の名前**であって位置ではない。
Zenmai は図を**位置で**描くので、位置を本人に訊いている。

- **1 回で終わった** → その機体は既知の 2 系統のどちらか（任天堂式 or Xbox 式）
- ★**「下」「上」「左」まで訊かれた** → **どちらでもない機体**。
  ★これは不具合ではなく**発見**なので、必ず報告に書く（そういう機体の存在が分かる）
- ★**5 が食い違った** → これは**不具合**。図と実際に入る字が違うのは最悪の事故なので、
  どのボタンでどの字が出たかを書く

## 4. 報告文の雛形（#testing-n-dev へ）

★英語で、★**機種ごとに 1 つ**、★同じ形で。「動いた」だけの報告より、
**何を何回試したか**が読める方が通る。

```
Device: <e.g. Anbernic RG35XX H>
CFW:    <e.g. ArkOS 20240218>   Resolution: 640x480
Port:   Zenmai (zenmai.zip)     Install: PortMaster (autoinstall)

Boots to the language menu; both ENGLISH and 日本語 start a game.
Button setup appeared once and accepted the right-hand face button on the first
press; the on-screen kana chart then matched what actually got typed.
Typed one command in Japanese (ゆうびんばこをあける) and one in English
(open mailbox) — both parsed and the game responded.
Save and restore round-tripped (ほぞんする / さいかいする).
Quit from the options menu asked for confirmation and returned to the launcher.
No stretching or cut-off edges at 640x480.

Notes: <ここに違ったことを書く。無ければ "nothing unexpected.">
```

食い違いがあったときの書き方の例:

```
Notes: button setup did not finish on the first press — it went on to ask for
"bottom", "top" and "left", so this device labels its face buttons in a third way.
After answering all four the kana chart matched what was typed.
```

```
Notes: at 480x320 the kanji are legible but tight; the ruby (furigana) above them
is hard to read. Playable, but I would not call this resolution supported.
```

## 5. 画像の残し方

★目視より確実で、記録に添えられる。**ssh が通る CFW なら**、実機自身に画面を
書き出させて、開発機と **md5 で突き合わせる**ことができる。

```sh
# 実機の上で（SDL の offscreen ドライバで画面をファイルへ）
cd /<roms>/ports/zenmai
SDL_VIDEODRIVER=offscreen ZENMAI_SCRIPT=/tmp/x.script ZENMAI_STOP=200 \
  ZENMAI_RAW=/tmp/x.raw ZENMAI_CONF=/tmp/x.conf ZENMAI_SAVE=/tmp/x.sav \
  ./zenmai-zork.aarch64
md5sum /tmp/x.raw
```

```sh
# 開発機で同じものを出して突き合わせる
cd native
ZENMAI_SCRIPT=<台本> ZENMAI_STOP=200 ZENMAI_RAW=/tmp/h.raw \
  ZENMAI_CONF=/tmp/h.conf ZENMAI_SAVE=/tmp/h.sav \
  ../portmaster/zenmai/zenmai-zork.aarch64
md5sum /tmp/h.raw                 # ★一致すれば「同じ画面」と言い切れる
python3 raw2png.py /tmp/h.raw /tmp/h.png   # 貼る用の PNG
```

- 台本の書き方は `native/*.script` を見る（`f0-f1:BUTTONS` の行が並ぶだけ）
- ★実機の SDL に `dummy` は**無い**（`offscreen` / `wayland` のみ）。
  `libEGL warning` が出るが動く
- ★**`ZENMAI_CONF` と `ZENMAI_SAVE` は必ず `/tmp` へ逃がす** ——
  本番のセーブとボタン設定を壊さないため
- ★**台本が 1 行も無いと `ZENMAI_STOP` が発火せず固まる**（`sc_n == 0`）。
  何も押さない画面を撮りたいときは、ずっと後のフレームに無害な 1 行を置く
  （`native/boot_menu.script` がその形）

★**画面を撮れない CFW もある**（ssh が開いていない等）。そのときは普通に写真でよい。
記録として要るのは「どの機種で・何を・どうしたか」であって、画質ではない。

## 6. 実機へファイルを送るときの作法

★**回線が不安定なときは `scp` より `ssh 'cat > file'` が通る**（2026-08-31 / 09-05 に実測）。

```sh
ssh <user>@<ip> 'cat > /<roms>/ports/zenmai/zenmai-zork.new' < portmaster/zenmai/zenmai-zork.aarch64
ssh <user>@<ip> 'cd /<roms>/ports/zenmai && md5sum zenmai-zork.new'   # ★一致を見てから
ssh <user>@<ip> 'cd /<roms>/ports/zenmai && mv zenmai-zork.new zenmai-zork.aarch64 && chmod +x zenmai-zork.aarch64'
```

- ★**md5 が一致してから本番の名前に置く。** `scp` は途中で切れることがあり、
  半端なファイルが本番の名前で残ると、起動が変な落ち方をする
- ★**`Zenmai.sh` はポートのフォルダではなく `ports/` の直下**。
  中に置くと**ランチャは古い方を使い続ける**うえ、フォルダが Ports 一覧に出る
  （2026-09-05 に踏んだ）
- ★**`zenmai.sav` は上書きしない**

## 7. 申請の手順

テストの記録が揃ってから:

1. [PortMaster-New](https://github.com/PortsMaster/PortMaster-New) を fork
2. fork の設定で **GitHub Actions を切る**
3. sparse checkout で clone（★リポジトリは巨大。`--filter=blob:none` を付けないと
   1.3GB を引いて index-pack が落ちる）
4. `tools/prepare_repo.sh`
5. `ports/zenmai/` に置く（`Zenmai.sh` / `port.json` / `gameinfo.xml` / `README.md` /
   `screenshot.png` / `zenmai/`）
6. `python3 tools/build_release.py --do-check` → **Broken: 0** を確認
7. PR を出す。★**本文にテスト記録（Discord スレッド）へのリンクを入れる**
