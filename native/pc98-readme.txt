Zenmai（ぜんまい）PC-98 版	ver. @VER@
日本語で読み、日本語で打つ Z-machine
---
名称|Zenmai PC-98 版
版|ver. @VER@
日付|@DATE@
作者|msonrm
種別|フリーソフトウェア（MIT License）
環境|PC-9801 / PC-9821・386 以上・MS-DOS・拡張メモリ 2MB 以上
入手|https://github.com/msonrm/zenmai
@BODY@

■ はじめに
  Z-machine（1979 年に Infocom が作った、テキストアドベンチャーを動かす仮想機械）を PC-98 に載せ、出力を日本語に訳し、かなで打てるようにしたものです。見本の作品として Zork I・II・III を同梱しています（story file は 2025 年に MIT License で公開されたもの）。日本語に訳してあるのは Zork I です。II と III は英語のまま遊べます。

★これは beta 版です。PC-98 の実機で動いたという報告を 1 件いただきました（ありがとうございます）。ほかはブラウザで動く PC-98 のエミュレータ QuuBee で確かめています。実機で試していただけると、とても助かります（「実機で試してくださる方へ」）。

■ 目次
|   はじめに / 目次 / 動作環境 / 起動 / 画面 / 打ち方 / 本文を遡る
|   やめる・セーブ / 起動画面の曲 / 部屋ごとの曲 / 画面の色と縁の絵柄
|   実機で試してくださる方へ / 更新履歴 / 配布・免責 / 商標
|   出どころとライセンス

■ 動作環境（目安）
  - 386 以上の CPU の PC-9801 / PC-9821
  - 16 色（アナログ）表示
  - MS-DOS と拡張メモリ 2MB 以上（HIMEM.SYS など）
  ★本体と作品で約 880KB を使うので、640KB（本体メモリだけ）では動きません。QuuBee では拡張メモリ 1MB でも動きました。1MB の実機で試した方は、ぜひ教えてください。
  - FM 音源（PC-9801-26K / 86 相当）があれば、起動画面と部屋ごとに曲が鳴ります（無くても動きます）。曲は常駐の音楽ドライバ PMD が鳴らします（同梱）。

■ 起動
  ZENMAI.BAT を実行します。中で SET DOS16M=1 をし、音楽ドライバ PMD86 を常駐させてから ZENMAI.EXE を起動します（DOS/4GW を使います）。
  ★FM 音源が 26K だけの機種（86 ボードも内蔵の 86 互換も無い機種）は ZENMAI26.BAT を使います（PMD86 の代わりに PMD.COM を常駐させます）。どちらも、常駐できなければ曲なしで動きます。
  起動画面で ←→ で作品を、↑↓ で言語を選び、RETURN キーで始めます。

  作品は ZENMAI.EXE と同じ場所に置いた 2 種類のファイルです。
|     ZORK1.Z3 など   …… story file（Z-machine のプログラムそのもの）
|     ZORK1.ZMP など  …… Zenmai の層（どの story 向けか・題・訳・語彙・
|                          ふりがな）
  ★ZENMAI は .ZMP を読み、それに合う story を同じ場所から探します（名前が違っても、中身の版が合えば見つけます。版が違う story は使いません）。.ZMP の無い story file（Z-machine の版 3 のもの）も、英語の作品として起動画面に並びます。

■ 画面
  上の帯に場所（左）と得点・手数（右）、真ん中に本文（13 行）、下に入力欄が出ます。左右の縁は部屋に合わせて色と絵柄が替わります。
  漢字の上に小さな灰色のふりがなが付きます（ただし本文の一番上の行だけは、上の帯と重なるので付けません）。遡って読んでいて下に続きがあるときは、本文の下に ▼ が出ます。

■ 打ち方（日本語）
  - ローマ字 …… 普段の打ち方。例: yuubinbakowoakeru → ゆうびんばこをあける
  - カナキー …… カナキーをロックすると、かなをそのまま打てる
  - CAPS …… 英字のまま打つ。英語のコマンド（OPEN MAILBOX など）もそのまま通る
  BS で 1 字消す。RETURN で送る。英語（ENGLISH）を選んだときは、打ったとおりの英字になります。

■ 本文を遡る
  ROLL DOWN / ↑ で遡り、ROLL UP / ↓ で戻ります。

■ やめる・セーブ
  「やめる」（英語は quit）と打つ（ゲームが確かめてきます）。セーブは「せーぶ」（save）、続きは「ろーど」（restore）。ファイルは ZORK1.SAV など（作品ごと）です。
  ★0.2.0-beta までのセーブ ZENMAI.SAV は、名前を ZORK1.SAV に変えると続きから遊べます。

■ 起動画面の曲
  J. S. バッハ『音楽の捧げもの』BWV 1079 より、2 声のカノン「Quaerendo invenietis」（謎カノン）。1 本だけ書かれた旋律を鏡に映して読むと、もう 1 声になる曲です。FM 音源の 2 声にドラムなどを足した、PMD 用の編曲（CANON.M）です。RETURN キーで止まって（数秒かけて消えて）ゲームが始まります。
  ★PMD は常駐します。あとで外したいときは PMD86 /R（26K は PMD /R）。曲を替えたいときは、PMD の .M の曲を CANON.M の名前で置き換えられます（枠は 16KB まで）。

■ 部屋ごとの曲（Zork I）
  ゲームの中では、部屋（状態行の部屋名）に合わせて曲が替わります。外（FIELD.M）・家の中（HOUSE.M）・ダムや川（WATER.M）・神殿（TEMPLE.M）・それ以外の地下（DEEP.M）。旋律というより雰囲気の見本です。同じ曲の部屋どうしを動くときは、曲は頭に戻らず続きます。
  割り当ては ZORK1.INI（作品と同じ名前の .INI）に書いてあります。部屋名は英語の状態行のもので、次のように書きます（メモ帳で直せます）。末尾の * は前方一致、* だけなら既定です。
|     [music]
|     Forest*  = FIELD.M
|     *        = DEEP.M
  ★同じ名前の .M を置けば、曲を替えられます（16KB まで）。ZENMAI.INI の music = off で曲を切れます。

■ 画面の色と縁の絵柄（Zork I）
  ZORK1.INI の [theme]（全体の既定）と [scene]（部屋ごと）で、画面の色を替えられます。背景などは #RGB（16 進 3 桁）、文字は色の名前（white・cyan など）です。部屋名の書き方は曲と同じです。
|     [theme]
|     band  = #237            ; 上の帯の色
|     text  = white           ; 本文の文字色
|     [scene]
|     Forest*  = pattern=FIELD.MAG band=#253 input=#242
  背景などの色: band 上の帯 / side 本文の左右 / input 入力欄 / body 本文の地 / ruby ふりがな
  文字の色: status 場所 / score 得点 / prompt ＞ / input_fg コマンドの文字（キャレットと ▼ も同じ色）/ text 本文 / echo 打ったコマンド
  絵柄: pattern 本文の左右に敷く絵柄（.MAG。本体と同じ場所に置く。- で無し）
  ★text と echo は [theme] にだけ書けます。絵柄（FIELD.MAG など）は適当なタイル模様の見本です。差し替えるときは、80×368・16 色の MAG（この大きさだけ）を作ってください。パレットの 5〜7・9〜15 が絵柄の色になります。

■ 実機で試してくださる方へ
  次を https://github.com/msonrm/zenmai/issues で教えてください。画面の写真があると助かります。
  1. 機種・CPU・メモリ・MS-DOS の版
  2. 起動するか（起動画面が出るか）
  3. ふりがな: 漢字の真上に小さな灰色のかなが出るか（ずれたり重なったりしないか）
  4. 画面: 本文が 13 行出るか。入力欄の細い縦線（キャレット）が点滅するか。左右の縁の絵柄が画面の下まで出るか。遡ったとき本文の下に ▼ が出るか
  5. ローマ字・カナキー・CAPS・ROLL UP / DOWN が効くか
  6. やめたあと、DOS の画面が元どおりに戻るか（画面下のファンクションキーの表示も）
  7. 曲が鳴るか（起動画面とゲームの中）。FM 音源の種類（26K / 86 / 内蔵 など）、どちらの .BAT で試したか、PMD の表示が何と出たか
  8. 拡張メモリが 1MB の機種で動くか（QuuBee では動きました）

■ 更新履歴
  2026.10.01  ver. 0.5.0-beta
  - 画面の仕上げ。本文を 13 行に。キャレットを細い縦線に。入力欄を 1 色にして縁の間へ。▼ を本文の下へ
  - 画面の色を INI で指定可能に。左右の縁に場面ごとの絵柄（.MAG）
  2026.09.30  ver. 0.4.0-beta
  - 曲を PMD で鳴らす。部屋ごとの曲（ZORK1.INI）
  2026.09.30  ver. 0.3.0-beta
  - 作品をファイル（.ZMP + story）に。起動画面で作品を選ぶ。Zork II・III を英語で同梱。拡張メモリ 1MB でも動く
  2026.09.29  ver. 0.2.0-beta
  - 起動画面の曲。DOS に戻ったときのファンクションキー行を直した
  2026.09.28  ver. 0.1.0-beta
  - 最初の公開

■ 配布・免責
  Zenmai の本体は MIT License です（全文は ZENMAI.TXT）。再配布・転載・改造は自由です。同梱する作品・ライブラリ・曲・ドライバには、それぞれのライセンスがあります（「出どころとライセンス」）。
  このソフトを使ったことで生じたいかなる損害にも、作者は責任を負いません。
  ご意見・不具合は https://github.com/msonrm/zenmai/issues へ。

■ 商標
  「ZORK」は商標です。Zenmai は商標の権利者とは関係がありません。作品の画面に出る表記（ZORK is a registered trademark of Infocom, Inc.）は原作のままです。

■ 出どころとライセンス
  - Zenmai 本体: Copyright (c) 2026 msonrm（MIT License）→ ZENMAI.TXT
  - Zork I の story file: historicalsource/zork1（MIT License）→ ZORK1.TXT
  - Zork II の story file: historicalsource/zork2（MIT License）→ ZORK2.TXT
  - Zork III の story file: historicalsource/zork3（MIT License）→ ZORK3.TXT
  - Z-machine（MojoZork）: Copyright (c) 2015-2025 Ryan C. Gordon（zlib License）→ MOJOZORK.TXT
  - ローマ字の表: Mozc の romanji-hiragana.tsv。Copyright 2010-2018, Google Inc.（BSD 3-Clause）→ MOZC.TXT
  - ふりがなの字形: 美咲ゴシック Copyright(C) 2002-2021 Num Kadoma。「改変の有無に関わらず、また商業的な利用であっても、自由にご利用、複製、再配布することができますが、全て無保証」
  - DOS4GW.EXE: DOS/4GW（Tenberry Software）。Open Watcom 1.9 に同梱の royalty-free の実行時版です。
  - 起動画面の曲: J. S. Bach『音楽の捧げもの』BWV 1079（1747 年。著作権は切れている）。音は Zenmai が楽譜から書き起こし、PMD 用に編曲したもの（CANON.M）です。
  - PMD86.COM / PMD.COM: Professional Music Driver（PMD）Copyright (c) M.Kajihara（KAJA）。KAJA さんが 2019 年に公開したソースを「ご自由に使って頂いて構いません」として Zenmai がビルドしたもの（ソース = https://github.com/d2lmirrors/pmd）です。曲の MML の変換にも同じ作者の MC を使っています。
