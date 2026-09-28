# Zenmai —— 作業の手引き

Zork I（Z-machine）を日本語で読み、日本語で打つ。版はブラウザ（`web/` + `src/`）・PS1・SDL2（PortMaster）・
PC-98（作業中）。

## まず読むもの

- **`TODO.md`** —— 現在地と次にやること。★作業を始めるときは冒頭の「現在地」と、該当する節から
- `docs/overview.md` —— 設計・しくみ・語彙の構造・検査・出典
- 版ごとの計画と実装ノート: `docs/ps1-port-plan.md` / `docs/ps1-implementation-notes.md` /
  `docs/portmaster-testing.md` / **`docs/pc98-port-plan.md`**（段の表と未決）

## 守ること

- ★**訳・語彙・ルビの挙動の正典は JS**（`src/translate.js` / `src/command.js` / `src/ruby.js`）。
  `native/` の C は移植なので、**直すときは両方を見る**（各ファイルの頭書きに書いてある）
- ★**語彙の原簿は非公開**（`zork1-cmd-ja.md` / `zork1-ja.md`）。`assets/*.json` だけ直すと次の生成で戻る
- ★**生成物は手で直さない**: `native/*_data.c`・`pairs.h`・`glyphs.h`・`ui_data.h`・`pc98_jis.{c,h}`。
  生成元（`gen_*.py`・`pc98_jis.py`）を直して焼き直す
- 境界は「リンクする実装を差し替える」形: `plat.h`（機械）・`glyph.h`（字）・`card.h`（セーブ）。
  上の層（`render.c`・`input.c`・`translate.c`・`cmd.c`）は無改造で運ぶ
- ★PS1 と SDL は**画素一致**を検査している（`native/test-*.sh`）。共有の C を触ったら流す
- 文書は日本語。★は要点、決めたことには**理由と日付**、踏んだ罠は症状ごと残す（既存の文書の書き方に合わせる）

## 建てる・確かめる

| 版 | 建てる | 確かめる |
|---|---|---|
| PS1 | `sh native/build.sh` | `native/test-*.sh`（★`.test-lock` がある間は焼き直さない） |
| SDL2 | `sh native/build-sdl.sh` | `sh native/test-sdl.sh` |
| 訳・語彙（C） | | `native/test_translate.c`・`native/cmd_test_host.c`（JS の記録と照合） |
| PC-98 | `sh native/build-pc98.sh` | `sh native/test-pc98.sh`（ホストと QuuBee で台本の記録を突き合わせる） |

### PC-98 の道具

- 開発機は **aarch64**。Open Watcom 1.9 = `~/development/openwatcom-1.9`（ヘッダ・ライブラリ・`dos4gw.exe`）+
  `~/development/ow-bin`（この機械で動く `wcc386` / `wlink`）。`-za99` で C99
- ★DOS/4GW は **`SET DOS16M=1`** が要る（無いと `DOS/16M error: [26] 8042 timeout`）
- QuuBee（ブラウザの PC-98）= `~/development/qb`。headless の土台は `tools/lib/machine.js`
  （`pc98_run.js` / `pc98-mock/shot.js` が使う）
- 字のコード（UTF-16 → 漢字 ROM）の正典は `native/pc98_jis.py`
