# TODO

> **現在地（2026-09-05）**: 実機（R36H）で動き、PS1 版とも画素一致。
> ★★**機能と UI は完成**（msonrm の判断）—— 2026-09-05 に 5 本入れて実機で確かめた:
> 「やめる」/ glibc 2.31 / 起動画面の並び / licenses の穴 / ボタン設定。
> ★残るは **PortMaster への公開申請**で、★**Zenmai が 1 本目**
> （[Higgins](https://github.com/msonrm/higgins) より先 —— 依存が SDL2 + FreeType だけなので）。
> 申請の手順を 1 本目で覚えてから Higgins に進む（msonrm さんの判断）。
> ★★**残っている関門は 1 つだけ = テストの記録**（実機が要るので人の仕事）。
> ★★**手順の正典は `docs/portmaster-testing.md`** —— 実機を触りながら潰す紙として
> 書いてあるので、**次に始めるときはそこから読む**。

## ★★1. PortMaster への申請

要件は [packaging.html](https://portmaster.games/packaging.html)（2026-09-04 に実測）。
★**`port.json`（version 2）/ `.sh` / `licenses/` / `gameinfo.xml` は既に満たしている**
（`.sh` は `control.txt` / `get_controls` / `pm_platform_helper` / `pm_finish` の作法どおり）。

- ★スクリーンショットは **実際のプレイ画面**（タイトル画面だけは不可）・4:3・640×480 以上
  → ある（`portmaster/screenshot.png`）。★**画面を変える直しをしたら撮り直す** ——
  2026-09-05 に撮り直した（語の折り返しの修正より前のものが残っていて 110 行ぶん古かった）
- ★README は**原作者への謝辞**と操作表が要る → ある（`portmaster/README.md`）
- ★★**テストの記録が必須** —— AmberELEC / ArkOS / ROCKNIX / muOS で試した記録を
  Discord の #testing-n-dev に出す。**記録の無い PR は却下される**。★ここは実機が要る。
  ★**バイナリ側の心配は無くなった**（下の 2 で glibc 2.17 まで下がった）ので、
  残っているのは**動かして記録を取ること**だけ。
  ★★**手順・チェックリスト・報告文の雛形・画像の残し方は
  `docs/portmaster-testing.md`**（実機を触りながら潰す紙として書いてある）
- 手順: [PortMaster-New](https://github.com/PortsMaster/PortMaster-New) を fork →
  Actions を切る → `tools/prepare_repo.sh` → `ports/` に置く →
  `python3 tools/build_release.py --do-check` → PR

★**Zork I の権利は問題ない** —— ZIL ソースが
[MIT で公開されている](https://github.com/historicalsource/zork1)（2025-11-20・
Microsoft Open Source Programs Office / Team Xbox / Activision）。story ファイルは
そこから自前で焼いているので、**同梱して配れる**。

## 済んだこと

### ★Start メニューに「やめる」（2026-09-05・段 1）

★気軽にやめられないゲームは良くない（実機の指摘）。
★**コマンドとして投げる**ので「本当にやめますか」は原作がそのまま出す。
詳細は `docs/ps1-implementation-notes.md` の 7 節。

- 項目の数（`UI_MENU_N` = 4）と読み物の頁の数（`UI_PAGE_N` = 3）を分けた
- 確定処理を対話ループ 2 つから `submit_ja` / `submit_en` へ切り出してから繋いだ
- 投げる語は語彙の原簿から（`assets/zork1-cmd.json` → `gen_ui.py` → `UI_QUIT`）
- ★**決めた面ボタンは押されたままメニューを抜ける**（`carry_over_pad`）。
  検査 = `test-options.sh` の 2 件（日英）／`test-quit.sh` の 3 件

### ★起動画面は ENGLISH → 日本語（2026-09-05・PR #44）

★原典（Zork I・1979）は英語なので、先に来るのは原典の言語。既定の選択も ENGLISH。
★**並びと値は別物** —— 返す `lang_en` は 0 = 日本語 / 1 = ENGLISH のまま。
★台本 39 本が影響を受けた（素の Start で日本語に入っていたものに ↓ を挟む）。

### ★licenses/ に Zork I の MIT が無かった（2026-09-05・PR #45）

story を実行ファイルに焼き込んでいるのに、その MIT が `licenses/` に入っていなかった。
★あわせて `licenses/README.md` で**何を覆っていないか**も書いた
（kh-dotfont はこの版では字を描いていない / SDL2・FreeType は同梱していない）。

### ★★ボタン設定 —— フェイスボタンの位置を本人に訊く（2026-09-05・PR #46）

★SDL の A/B/X/Y は**札の名前**であって位置ではないので、機種ごとに割れる。
初回起動で「右のボタンを押してください」と訊く。原則 1 回で決まり、
★既知の 2 系統のどちらでもなければ「下」「上」「左」と訊き続ける。
Start メニューからも入れる（上から ひらがな入力方法 / システムコマンド / ボタン設定 /
ライセンス / やめる）。詳細 = `docs/ps1-implementation-notes.md` の 7 節。

### ★glibc 2.31 で焼き直す（2026-09-05・段 2）

- `portmaster/Containerfile` = **Ubuntu 20.04**（ArkOS / AmberELEC の土台そのもの）。
  ★Debian bullseye も 2.31 だが `bullseye-security` の索引と pool が食い違って apt が 404
- `sh portmaster/build-port.sh` が**既定で入れ物に入る**（`ZM_HOST_BUILD=1` で手元の glibc）
- ★結果: 要求は **GLIBC_2.17 だけ**（開発機 trixie で焼くと 2.34 を要求していた）。
  依存は `libSDL2-2.0.so.0` / `libfreetype.so.6` / `libc.so.6` のみ
- ★**入れ物で焼いても画面は 1 画素も変わらない**（307,200 点を突き合わせて確認）
