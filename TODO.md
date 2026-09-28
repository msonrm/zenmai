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
> ★**PC-98 版は ver. 0.2.0-beta を公開**（2026-09-29・pre-release。0.1.0-beta は 2026-09-28）—— 実機の報告待ち。→ 下の 3。

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

## 2. 語彙の原簿で直すもの

★**直す場所は非公開の原簿**（`zork1-cmd-ja.md` / `zork1-ja.md`）。`assets/*.json` だけ直すと次の生成で戻る。
見つけたのは PC-98 版のふりがなの見本を作ったとき（2026-09-27）。

- **円匙(えんぴ)** —— シャベル（`SHOVEL`）の入力の言い方に `シャベル / スコップ / 円匙 / えんぴ` とある。
  ★**ほぼ誰も使わない語**なので外す。訳文には出てこない（入力の語彙とふりがなの表にだけある）
- **`"一片": [["一片", "にんにく"]]`** —— ふりがなの表で「一片」に「にんにく」を振る項目。
  いまは訳文に「一片」が単独で出ず、長い `にんにくの一片`（= 一片(いっぺん)）が先に当たるので表に出ていない。
  ★読みの割り付けが物の名前の読みを拾ってしまったもの。単独の「一片」が訳文に入った時点で化ける

## 3. PC-98 版

★**ver. 0.2.0-beta を公開した**（2026-09-29・[Release `pc98-v0.2.0-beta`](https://github.com/msonrm/zenmai/releases/tag/pc98-v0.2.0-beta)・
pre-release。0.1.0-beta は 2026-09-28）。★**正典は `docs/pc98-port-plan.md`**（段の表・決めたこと・末尾の「次にやること」）。

- ★**次は実機の報告を待って直す**（msonrm は PC-98 の実機を持っていない。報告先 = GitHub の issues）。
  ★**実機で動いた**（2026-09-28・X の告知への報告）。DOS に戻るとファンクションキーの行が消えるのは直した
  （起動時の画面を控えて戻す・詳細 = `docs/pc98-port-plan.md` の「段 6」）。0.2.0-beta に入れた。
  いちばん知りたいのは**ふりがなの帯**（CRTC の PL = −8 が実機で同じに出るか）
- ★**起動画面で FM 音源の曲が鳴る**（2026-09-29・Bach の謎カノン・エレピ × シンセベース）。詳細 = 計画書の「段 7」。
  26K で鳴るかは実機待ち。0.2.0-beta に入れた
- **main へ入れる**: いまはブランチ `feat/pc98` だけ（push 済み・タグもその上）。main の README に PC-98 版のことは未記載
- 拡張メモリ 1MB で動かす（いまは 2MB 以上。表 488KB を詰めないと届かない）・音楽・イラスト
- 建てる・確かめる・配る = `sh native/build-pc98.sh` / `sh native/test-pc98.sh`（数秒）/ `sh native/pack-pc98.sh`

## 済んだこと

### ★PC-98 版 ver. 0.1.0-beta（2026-09-27〜28・ブランチ `feat/pc98`）

★**Zenmai は Z-machine、Zork I は同梱の見本の作品**という形で出した（商標の「ZORK」を前に出さない・msonrm の判断）。
画面の設計（QuuBee で表示して決めた）→ 段 1 素の Zork → 段 2 `session.c` を全版で共有 → 段 3 ローマ字 / カナキー →
段 4 24 ラスタ × 16 行・ふりがな・禁則（`kinsoku.h` を PS1 / SDL と共有）→ 段 5 起動画面・英語モード・書庫・Release。
★共有の C を触った段 2・段 4 では、PS1/SDL の検査 8 本を前後とも回して緑。詳細 = `docs/pc98-port-plan.md`。

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
