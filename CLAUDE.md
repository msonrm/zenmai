# Zenmai —— 作業の手引き

Zork I（Z-machine）を日本語で読み、日本語で打つ。版はブラウザ（`web/` + `src/`）・SDL2（PortMaster）・PS1（★凍結）・
PC-98（ver. 0.5.1-beta を pre-release で公開・Zork II / III を英語で同梱・作品はパック + story のファイル・実機の報告待ち）。

## まず読むもの

- **`TODO.md`** —— 現在地と次にやること。★作業を始めるときは冒頭の「現在地」と、該当する節から
- `docs/overview.md` —— 設計・しくみ・語彙の構造・検査・出典
- **`docs/translation-guide.md`** —— 訳・語彙の作り方・踏んだ罠・Zork II / III を始める手順・パックの共通化の判断
- 版ごとの計画と実装ノート: `docs/ps1-port-plan.md` / `docs/ps1-implementation-notes.md` /
  `docs/portmaster-testing.md` / **`docs/pc98-port-plan.md`**（段の表と未決）

## 守ること

- ★**訳・語彙・ルビの挙動の正典は JS**（`src/translate.js` / `src/command.js` / `src/ruby.js`）。`native/` の C は移植。
  ★**訳・語彙は Web（JS）で進め、native は節目でまとめて追う**（msonrm 2026-11-13）—— データだけでなく**挙動の変更も、節目まで C に入れない**。
  節目 = Zork II・III の日本語を native に載せるとき（`TODO.md`）。理由: 1 件ごとに C・生成物・パックがついて回ると、Zork III の訳が進まない。
  ★そのあいだ **JS ↔ C の照合（`cmd_ref.js` ↔ `cmd_test_host`・`test_translate`）は食い違っていてよい**（赤は「C が遅れている」で、壊れたのではない）。
  ★C に入れていない挙動の変更は **`TODO.md` の「native が追うもの」に 1 行ずつ残す**（節目で拾う一覧。残さないと何を移すか分からなくなる）。
  ★native の不具合の報告（実機で出たもの）は節目を待たずに直す。節目で C を追うときは、これまでどおり両方を見る（各ファイルの頭書き）
- ★**語彙の原簿は非公開**（`zork1-cmd-ja.md` / `zork1-ja.md`）。`assets/*.json` だけ直すと次の生成で戻る
- ★**生成物は手で直さない**: `native/*_data.{c,h}`（`kana_input_data.c`・`misaki_data.c` を含む）・`native/*_tab.c`（`ctab.py`）・`pairs.h`・
  `glyphs.h`・`ui_data.h`・`pc98_jis.{c,h}`。生成元（`gen_*.py`・`pc98_jis.py`）を直して焼き直す
- ★**禁則の表は `native/kinsoku.h` の 1 つだけ**（PS1 / SDL / PC-98 が共有。Python の参照実装 `ps1-mock/gen_mock.py` とは
  `check_kinsoku.py` が突き合わせる）
- 外から採ったもの（字形・ローマ字の表）は `native/vendor/*/README.md` に出どころと許諾がある。配るときの義務もそこ
- 境界は「リンクする実装を差し替える」形: `plat.h`（機械）・`glyph.h`（字）・`card.h`（セーブ）・
  `render.h` の文字列の口（PC-98 は `render_pc98.c` が同じ名前で実装する）。
  上の層（`session.c`・`render.c`・`input.c`・`translate.c`・`cmd.c`）は無改造で運ぶ
- ★**VM とのつなぎは `session.c` の 1 本だけ**（全版が共有）。版ごとの入口（`main.c` / `main_pc98.c`）に
  VM に触る道を書かない
- ★**PS1 版は凍結**（開発を停止した扱い・msonrm 2026-11-13）。コード（`plat_ps1.c`・`build.sh`・`sim.py`・`card.c`）は**消さずに残す**が、
  直さない・出し直さない・検査も流さない。共有の C を触るときに PS1 で建つかは気にしない（`plat.h` の境界があるので残しても邪魔にならない）
- ★**先に進めるのは Zork III の翻訳まで素直に通すこと**（同日）。構造の直しは、その道の邪魔になるものから
- ★**SDL 版の検査は「字の記録」へ寄せる**（同日に決めた・★まだ作っていない → `TODO.md`）。PC-98 版と同じに、台本を流して記録を突き合わせる。
  理由: PS1 との**画素一致**（`test-sdl.sh`）が守っていたのは「PS1 への移植が SDL と同じ絵を出すか」で、相手を止めたので目的ごと消えた。
  上の層（訳・語彙・折り返し・禁則・ふりがな）は字の単位で足りる。★記録で見えないのは重ね描き（オプション・手引きの図）—— そこは実機か目視
- ★**配る物・公に出す文章では「Zenmai は Z-machine、Zork I は同梱の見本の作品」と書く**（商標の「ZORK」を前に出さない。
  書庫の名前にも入れない・msonrm の判断 2026-09-28）
- 文書は日本語。★は要点、決めたことには**理由と日付**、踏んだ罠は症状ごと残す（既存の文書の書き方に合わせる）

## 建てる・確かめる

| 版 | 建てる | 確かめる |
|---|---|---|
| SDL2 | `sh native/build-sdl.sh` | `native/test-wrap.sh`・`test-glyph.sh`・`test-face.sh`（PS1 の模擬に頼らない 3 本）。★入力・やめる・セーブ・オプションは記録方式の検査を作るまで網が無い |
| PS1（凍結） | `sh native/build.sh` | 流さない。`test-sdl.sh`・`test-entry.sh`・`test-quit.sh`・`test-save.sh`・`test-options.sh` は PS1 の模擬 `sim.py` に頼る（5 本で 30 分近く）ので、ふだんの検査から外した |
| 訳・語彙（JS・Web） | | **`npm test`**（全作品・約 8 秒。`npm test -- zork2` で作品を選ぶ）。1 本ずつは `WORK=zork2 node test/run-cmd.js` など。★作品ごとの固定表・手の列は `test/works/<作品>.js`（検査の本体に作品名を書かない） |
| 訳・語彙（C） | | `native/test_translate.c`・`native/cmd_test_host.c`（JS の記録と照合） |
| PC-98 | `sh native/build-pc98.sh` | `sh native/test-pc98.sh`（数秒。打鍵と組み方のホスト検査 + 台本をホストと QuuBee で流して記録を突き合わせる） |

PC-98 版を配る = `sh native/pack-pc98.sh`（→ `native/pc98-out/zenmai98-<版>.zip`）。版は `native/pc98_version.h` の 1 か所。
Release のタグは `pc98-vX.Y.Z`（PS1 版は `ps1-vX.Y.Z`）。

### PC-98 の道具

- 開発機は **aarch64**。Open Watcom 1.9 = `~/development/openwatcom-1.9`（ヘッダ・ライブラリ・`dos4gw.exe`）+
  `~/development/ow-bin`（この機械で動く `wcc386` / `wlink`）。`-za99` で C99
- ★DOS/4GW は **`SET DOS16M=1`** が要る（無いと `DOS/16M error: [26] 8042 timeout`）
- ★Watcom 1.9 は**自動変数の構造体を非定数で初期化できない**（`E1054`）。項目ごとに代入する
- PC-98 の画面は **1 行 24 ラスタ × 16 行**（テキスト VRAM の行番号はそのまま・0〜16）。キーは BIOS（INT 18h）から読む
- QuuBee（ブラウザの PC-98）= `~/development/qb`。headless の土台は `tools/lib/machine.js`
  （`pc98_run.js` / `pc98-mock/shot.js` が使う）
- 字のコード（UTF-16 → 漢字 ROM）の正典は `native/pc98_jis.py`
- 台本（`native/pc98-test/*.txt`・`ZENMAI /S 台本`）の記法: `#!english`（頭・英語面）/ `#!work ZORK2`（頭・作品。無ければ一覧の最初）/ `#!keys`（以降を打鍵として流す）/
  `#!line 文`（文をそのまま本文に流す = 組み方を画面で見る）
- ★作品は焼き込まず、**パック `ZORK1.ZMP`**（`gen_pack.py` が作る・書式の正典もそこ）を起動時に読み、
  パックが持つ識別で **story `ZORK1.Z3`** を横から探す。★story はパックに入れない（story を分けた意図）。★ただし**パックの TRAN 節は訳の照合キーとして英語の原文を持つ**ので、原作が再配布不可なら訳の束も配れない（2026-10-05 に確認・`docs/translation-guide.md` の 6.4）。
  ★訳・語彙・ふりがなの表もパックの節（書式の正典 = `ctab.py`）。PC-98 は `*_data.c` の代わりに `*_tab.c` + `tabload.c` を links する。
  ★表の形や UI の文言（`gen_cmd.py` の `UI_FRAGS`）を変えたらパックも作り直す（要約値が違うと本体が断る）
  `pc98-out/` に EXE と並べて置く（`pc98_run.js`・`test-pc98.sh`・`pack-pc98.sh` はそうしている）。セーブは作品ごと（`ZORK1.SAV`）。
  ★起動画面はカレントディレクトリの作品を並べる（`pack_list`）。Zork II・III（`vendor/zork2`・`zork3`・MIT）は訳が無いので英語だけ。
  ★確保の順番: 大きなもの（story・表）を先に、本文の環の塊を後に（逆だと拡張 1MB で表が取れない）
- ★**起動画面の曲は PMD が鳴らす**（`pc98_music.c` が常駐の PMD に `.M` を渡す。自前の FM 直叩きは無い）。曲の作り直し =
  `python3 gen_canon_mml.py` → `node mc98.js pc98-music/CANON.MML`（MC.EXE は `sh build-mc.sh`・KAJA 氏の自由公開ソースから）。
  出どころ・罠は `native/pc98-music/README.md`。検査は `test-pc98-music.js`（`test-pc98.sh` から流れる）。
  ★部屋ごとの曲は `ZENMAI.INI` / `<作品>.INI`（状態行の英語の部屋名 → 曲。書式は `pc98_music.h`）。記録の `# music:` をホストと突き合わせる
- ★**画面の色と縁の絵柄も INI**（`[theme]` 全体 / `[scene]` 場面ごと。書式の正典は `pc98_theme.h`。絵柄は `.MAG` 80×368 だけ・`gen_edge_mag.py` が作る見本。
  検査は `test_theme.c`・`test_mag.c` + `test-mag.js`）。★グラフィックの線（キャレット・▼）はテキストを消しても残るので、描き直すときは前の線を自分で消す
- ★配る書庫の `README.TXT` は PC-98 時代の流儀（NEC 罫線の枠・76 桁）。**台本 `native/pc98-readme.txt` を直し**、`fmt_readme.py` が折る（生成物を手で直さない）
- ★曲・絵はパックに入れない（外のファイル + 作品ごとの `.INI`・計画書の「次にやること」）
- 必要なメモリは像 約 262KB + 起動後に約 620KB（★要件は拡張 2MB。1MB は目標から外したが、今は QuuBee の 1MB でも動く ——
  測った境目と、大きなものを 59KB 以下の塊で取る理由は `docs/pc98-port-plan.md` の「段 8」）
- ★PS1 版との共有は重視しない（実験的な実装・msonrm 2026-09-29）。★PS1 版は 2026-11-13 に凍結したので、共有の C を触っても PS1 の検査は流さない
