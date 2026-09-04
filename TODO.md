# TODO

> **現在地（2026-09-04）**: 実機（R36H）で動き、PS1 版とも画素一致。
> ★次は **PortMaster への公開申請**で、★**Zenmai が 1 本目**（[Higgins](https://github.com/msonrm/higgins) より先）——
> 依存が SDL2 + FreeType だけなので、**glibc の問題だけを解けばよい**。
> 申請の手順を 1 本目で覚えてから Higgins に進む（msonrm さんの判断）。

## ★★1. Start メニューに「やめる / QUIT」を足す（実機の指摘・2026-09-04）

コマンド（「やめる」/ `quit`）でやめられるのは確かだが、
★**気軽にやめられないゲームは良くない**（msonrm さん）。★セーブはコマンドのままでよい。

- ★★**コマンドとして投げる**（`feed_cmd`）—— `GState->quit` を直接立てず、**原作の QUIT を
  走らせて「本当にやめますか」を出す**。★**原作がそのまま動く**が Zenmai の主張なので省かない
- ★投げる語は**語彙の原簿から引く**（日本語 = 「やめる」/ 英語 = `quit`）——
  `native/gen_ui.py` がシステムコマンド一覧（`P_CMDS`）で既にそうしている。
  ★書き写すと、語を変えたときにここだけ古くなる
- ★**送信処理が対話ループ 2 つ（`interactive_ja` / `interactive_en`）にある**ので、
  先に関数へ切り出し、**台本で挙動不変を確かめてから**メニューに繋ぐ
  （Higgins で `apply_frame` を切り出したのと同じ手順）
- ★メニューの項目数（4）と読み物の頁数（3）が**別になる** —— いまは `UI_PAGE_N` が
  両方を兼ねている。`opt_sel` の回り方と、決定時の分岐を分ける
- ★回帰は既にある: `native/quit_ja.script` / `quit_en.script` / `native/test-quit.sh`

## ★★2. glibc 2.31 で焼き直す

★開発機は Debian trixie（**glibc 2.41**）。**ArkOS / AmberELEC は 2.31** なので、
そのままでは動かない（`portmaster/README.md` にも書いてある）。

- **podman** で `docker.io/arm64v8/debian:bullseye`（glibc 2.31）を立てて焼く
  （開発機は aarch64 なのでクロスは要らない）
- 要る開発パッケージ: `libsdl2-dev` / `libfreetype-dev`
- ★焼いたら **`ldd` で 2.31 を要求していること**と、実機で動くことを確かめる

## ★★3. PortMaster への申請

要件は [packaging.html](https://portmaster.games/packaging.html)（2026-09-04 に実測）。
★**`port.json`（version 2）/ `.sh` / `licenses/` / `gameinfo.xml` は既に満たしている**
（`.sh` は `control.txt` / `get_controls` / `pm_platform_helper` / `pm_finish` の作法どおり）。

- ★スクリーンショットは **実際のプレイ画面**（タイトル画面だけは不可）・4:3・640×480 以上
- ★README は**原作者への謝辞**と操作表が要る —— ★`portmaster/README.md` は既にある
  （MojoZork の Ryan C. Gordon、Infocom、ZIL を MIT で公開した Microsoft / Activision）
- ★★**テストの記録が必須** —— AmberELEC / ArkOS / ROCKNIX / muOS で試した記録を
  Discord の #testing-n-dev に出す。**記録の無い PR は却下される**。ここは実機が要る
- 手順: [PortMaster-New](https://github.com/PortsMaster/PortMaster-New) を fork →
  Actions を切る → `tools/prepare_repo.sh` → `ports/` に置く →
  `python3 tools/build_release.py --do-check` → PR

★**Zork I の権利は問題ない** —— ZIL ソースが
[MIT で公開されている](https://github.com/historicalsource/zork1)（2025-11-20・
Microsoft Open Source Programs Office / Team Xbox / Activision）。story ファイルは
そこから自前で焼いているので、**同梱して配れる**。
