'use strict'
/**
 * 入力側（日本語 → 英語コマンド）の固定表。**実プレイで出た取りこぼしをここに足す。**
 *
 * ★この表は「正しい英語コマンド」ではなく「**原作が受け付ける形**」を書く。
 *   原作の文法は有限の表なので、期待値は推測ではなく `zork1-cmd.json` の shapes から決まる。
 *
 * 使い方: node test/run-cmd.js
 */
const fs = require('fs')
const path = require('path')
const { createCommander } = require('../src/command.js')

const asset = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'assets', 'zork1-cmd.json'), 'utf8'))
const cm = createCommander(asset)

const CASES = [
  // --- 基本 ---
  ['ゆうびんばこをあける', 'open mailbox'],
  ['てがみをよむ', 'read advertisement'],   // 原作は代表の名詞を受ける（leaflet も同義語）
  ['けんをとる', 'take sword'],
  ['けんをつかむ', 'take sword'],          // 同じ英単語に寄せる（動詞の言い方は複数）
  ['じゅうたんをめくる', 'move rug'],
  // ★「扉」「窓」は**わざと曖昧なまま渡す**（2026-08-14）。どの扉かは部屋が決めるので
  //   原作に判断を返す（`open door` → 「木の扉と揚げ戸の、どちらのことか。」）。
  //   日本語で一意な言い方（木の扉 / 揚げ戸）は形容詞を添えて聞き返しを省く
  ['あげど', 'trap door'],
  ['いしのとびらをあける', 'open huge door'],
  // --- 連体修飾（AのB）は B が目的語（2026-08-14 実プレイ）---
  ['いえのとびらをあける', 'open door'],
  ['いえのどあをあける', 'open door'],
  ['きのどあをあける', 'open wooden door'],   // 語彙に載っている複合語は最長一致が先に取る
  // --- 助詞が役を決める ---
  ['ランタンでとびらをあける', 'open door with brass lamp'],
  ['じゅうたんのしたをみる', 'look under rug'],
  ['きにのぼる', 'climb tree'],             // 「に」は CLIMB に無い形 → 裸で渡す
  // --- 方角 ---
  ['きたへいく', 'north'],
  ['したへいく', 'down'],
  ['したへおりる', 'down'],                 // 物を伴わない DISEMBARK は方角
  ['うえへのぼる', 'up'],
  ['はしごをおりる', 'climb down ladder'],  // 乗り物でなければ climb down
  ['きをおりる', 'climb down tree'],        // 実プレイ: disembark tree は「乗っていない」
  ['ふねをおりる', 'disembark boat'],       // 乗り物（VEHBIT）なら disembark
  ['なかにはいる', 'enter'],
  // --- 実プレイで出た取りこぼし（2026-08-13）---
  ['いたをはずす', 'take boards'],          // BOARD が動詞と物で衝突していた
  ['どあをあける', 'open door'],           // ドア／戸 が語彙に無かった
  ['ドアをあける', 'open door'],           // カタカナでも当たる
  // --- 動詞が取れない関係は黙って落とさない（2026-08-14 実プレイ）---
  ['いえのうしろにいく', null],             // `walk house` になっていた
  ['いえのうらにいく', null],
  ['きにのぼる', 'climb tree'],             // 「に・へ」だけは例外（ただ目的語を指す）
  // --- 同じ日本語が別の英単語の物を指す（2026-08-14 実プレイ）---
  //   原作は 1 語しか受けないので候補を順に試す。第一候補だけ固定表で見る
  ['とろふぃーけーすをみる', 'look at case'],   // 名前をずらしたので一発で当たる
  ['ゆうびんばこをあける', 'open mailbox'],
  // --- 目的語が要る動詞は、原作に聞き返させずこちらで訊く（2026-08-14 実プレイ）---
  //   原作の聞き返しの最中に完全な文を送るとパーサが混ぜて壊す（eat advertisement → 「eat advert」）
  ['おりる', 'disembark'],                  // needsObject が立つ（下の別検査で見る）
  // --- 原作にない言い方には案内を返す（2026-08-14 実プレイ）---
  ['けんをつかう', null],                  // 「使う」に当たる動詞は原作にない
  ['ほんをよむ', 'read book'],             // 「本」が語彙に無かった
  ['ふたをあける', 'open machine'],        // 蓋 = 機械の蓋 / 揚げ戸の蓋（原作の COVER）。外れたら順に試す
  // --- はい／いいえ（終わるか・最初からやるかの質問。原作は Y/N で受ける）---
  ['はい', 'y'], ['いいえ', 'n'], ['やめる', 'quit'],
  ['しゅうりょう', 'quit'], ['もういちど', 'again'],   // again はパーサが直接受ける語（動詞表に無い）
  // --- 複数対象（ALL / EXCEPT / AND）---
  //   ★捌くのは原作の仕事。z3 の辞書に `all` / `but` / `except` / `and` が入っている
  //   （684 語を直接読んで確認した）ので、こちらは語を渡すだけでよい
  ['ぜんぶとる', 'take all'],
  ['すべてとる', 'take all'],
  ['ぜんぶおく', 'drop all'],
  ['ぜんぶ', 'all'],                                    // 「何を取る？」への答えにもなる
  // ★「ぜんぶとる」の「と」は助詞ではなく**動詞の頭**。助詞を当てた先が行き止まりなら当てない
  //   （直す前は「と」が AND に食われ、「る」だけ残って「知らない言葉」になっていた）
  ['ぜんぶとじる', 'close all'],
  ['らんたんいがいをぜんぶとる', 'take all except brass lamp'],
  ['らんたんいがいをとる', 'take all except brass lamp'],  // 除くものだけ言われたら残り全部を指す
  ['はことけんいがいをぜんぶとる', 'take all except chest and sword'],  // (箱と剣) 以外に分配される
  // ★「と」で並べた物を捨てると**片方が黙って消える**（`take bag` になっていた）
  ['びんとふくろをとる', 'take bottle and bag'],
  ['けんとらんたんをとる', 'take sword and brass lamp'],
  // --- 知らない言葉は黙って捨てない ---
  ['ぶんぶんをあける', null],               // 「を」が付く未知語 = 物のつもり → 止める
  ['そっととびらをあける', 'open door'], // 助詞の付かない未知語は落としてよい
  ['とびらをあけない', null],               // 否定は扱えない
]

let ng = 0
// ★総称は**候補が揃っていること**を見る（第一候補は順位付けの都合で動く）
{
  const r = cm.toCommand('はこをみる')
  const got = [r.command, ...(r.alts || [])].sort().join(' / ')
  const want = ['look at case', 'look at chest', 'look at mailbox', 'look at trunk'].join(' / ')
  const ok = got === want
  if (!ok) ng++
  console.log(`${ok ? '✓' : '✗'} はこをみる（候補）  → ${got}${ok ? '' : ` ★期待: ${want}`}`)
}
{
  const a = cm.toCommand('おりる')
  const b = cm.toCommand('き', { verb: a.verbKey })
  const ok = a.needsObject && a.ask === '何を降りる？' && b.command === 'climb down tree'
  if (!ok) ng++
  console.log(`${ok ? '✓' : '✗'} おりる → 訊く → き　→ ${a.ask} / ${b.command}`)
}
for (const [ja, want] of CASES) {
  const r = cm.toCommand(ja)
  const ok = r.command === want
  if (!ok) ng++
  console.log(`${ok ? '✓' : '✗'} ${ja.padEnd(14, '　')} → ${String(r.command).padEnd(24)}${ok ? '' : ` ★期待: ${want}`}`)
}
// ★轟音の部屋の反響 —— 原作は**プレイヤーが打った語**を返す（`P-INBUF` から切り出す）ので、
//   訳語ではなく打った呼び名が返らなければならない。
//   walkthrough はこの部屋を通らないので、ここで押さえる
{
  const { Translator } = require('../src/translate.js')
  const tr = new Translator(JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'assets', 'zork1-ja.json'), 'utf8')))
  // ★英語モード＝**訳すのをやめる**。原文がそのまま返る（行の貯め込みや
  //   プロンプト剥がしはそのまま使いたいので、訳を引くところだけを止めている）
  {
    const raw = 'West of House'
    const ja = tr.line(raw)
    tr.setPassthrough(true)
    const en = tr.line(raw)
    tr.setPassthrough(false)
    const ok = ja === '家の西' && en === raw
    if (!ok) ng++
    console.log(`${ok ? '✓' : '✗'} 英語モードは訳さず原文  → 日本語「${ja}」/ 英語「${en}」`)
  }
  // ★複数対象になると、原作は対象ごとに `<名前>: <文>` の形で 1 行を出す。
  //   行単位で引く設計なので、名前が頭に付いた瞬間に在庫から外れる（`Dropped.` は持っているのに
  //   `brown sack: Dropped.` が引けなかった）。剥がして両方引いて組み直せているか。
  //   ★在庫に `{OBJ}: Taken.` を 1 本だけ持っていたのはやめて、この規則に一本化した
  {
    const MULTI = [
      ['glass bottle: Taken.', 'ガラス瓶：取った。'],
      ['brown sack: Dropped.', '茶色の袋：置いた。'],
      ['brown sack: You already have that!', '茶色の袋：それはもう持っている。'],
      ['small mailbox: It is securely anchored.', '小さな郵便箱：しっかりと固定されている。'],
      // ★引用符の中でも `{VERB}` は**打った語ではない**（こちらが送った英語）ので訳す。
      //   「ぜんぶあける」と打った人の画面に `「open」に…` と出ていた
      ['You can\'t use multiple direct objects with "open".', '「開ける」に複数の目的語は使えない。'],
      // ★逆に `{WORD}` は打った語そのもの。訳してはいけない
      ['I don\'t know the word "bunbun".', '「bunbun」という言葉は知らない。'],
      // ★名前と本文の**両方**が引けたときだけ組む。片方でも欠けたら未訳のまま出す
      //   （`:` を含むだけの行を、それらしい顔にして隠さない）
      ['xyzzy plugh: Taken.', null],
      ['glass bottle: Xyzzy plugh.', null],
    ]
    for (const [en, want] of MULTI) {
      const got = tr.line(en)
      const ok = got === want
      if (!ok) ng++
      console.log(`${ok ? '✓' : '✗'} 複数対象 ${en.padEnd(42)} → ${got}${ok ? '' : ` ★期待: ${want}`}`)
    }
  }
  for (const [ja, want] of [['のべぼうをとる', 'のべぼう のべぼう ……'], ['はんきょう', 'はんきょう はんきょう ……']]) {
    const r = cm.toCommand(ja)
    tr.setEcho(r.echoWord || ja)
    const last = String(r.command).split(' ').pop()
    const got = tr.line(`${last} ${last} ...`)
    const ok = got === want
    if (!ok) ng++
    console.log(`${ok ? '✓' : '✗'} 反響 ${ja.padEnd(10, '　')} → ${got}${ok ? '' : ` ★期待: ${want}`}`)
  }
}
// ★問い返しは**打った言い方を映す**こと。
//   動詞の言い方に名詞が埋まっているもの（`ぜんまいをまく` `錠を開ける` …）で、
//   正式形だけから問い返すと埋まっていた名詞が消え、答えたくなる語が通らなくなる。
//   実プレイで「ぜんまいをまく → 何を巻く？ → ぜんまい → 知らない言葉」を踏んだ。
//   ★1 件ではなく**類**なので、言い方を全部なめて固定する（増えても勝手に検査される）
{
  let n = 0
  let miss = 0
  for (const v of Object.values(asset.verbs)) {
    for (const ja of v.ja || []) {
      const cut = ja.indexOf('を')
      if (cut <= 0 || cut >= ja.length - 1) continue
      const r = cm.toCommand(ja)
      if (!r || !r.needsObject) continue
      n++
      // 埋まっていた名詞が問い返しに残っているか
      if (!String(r.ask).includes(ja.slice(0, cut))) { miss++; ng++; console.log(`✗ 問い返しが名詞を落とす ${ja} → 「${r.ask}」`) }
    }
  }
  console.log(`${miss ? '✗' : '✓'} 問い返しが打った言い方を映す（${n} 通り / 落ち ${miss}）`)
  // ★実プレイの流れがつながること
  for (const [say, answer, want] of [
    ['ぜんまいをまく', 'かなりあ', 'wind canary'],
    ['じょうをあける', 'とびら', 'unlock door'],
  ]) {
    const r1 = cm.toCommand(say)
    const r2 = cm.toCommand(answer, { verb: r1.verbKey })
    const ok = r2 && r2.command === want
    if (!ok) ng++
    console.log(`${ok ? '✓' : '✗'} ${say} → 「${r1.ask}」 → ${answer} → ${r2 && r2.command}${ok ? '' : ` ★期待: ${want}`}`)
  }
}

console.log(`\n--- ${CASES.length} 件 / 食い違い ${ng} 件 ---`)
process.exit(ng ? 1 : 0)
