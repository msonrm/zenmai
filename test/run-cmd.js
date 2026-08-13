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
  // --- 助詞が役を決める ---
  ['ランタンでとびらをあける', 'open front door with brass lamp'],
  ['じゅうたんのしたをみる', 'look under rug'],
  ['きにのぼる', 'climb tree'],             // 「に」は CLIMB に無い形 → 裸で渡す
  // --- 方角 ---
  ['きたへいく', 'north'],
  ['したへいく', 'down'],
  ['したへおりる', 'down'],                 // 物を伴わない DISEMBARK は方角
  ['うえへのぼる', 'up'],
  ['はしごをおりる', 'disembark ladder'],   // 物を伴えば動詞のまま
  ['なかにはいる', 'enter'],
  // --- 実プレイで出た取りこぼし（2026-08-13）---
  ['いたをはずす', 'take boards'],          // BOARD が動詞と物で衝突していた
  ['どあをあける', 'open front door'],      // ドア／戸 が語彙に無かった
  ['ドアをあける', 'open front door'],      // カタカナでも当たる
  // --- 知らない言葉は黙って捨てない ---
  ['ぶんぶんをあける', null],               // 「を」が付く未知語 = 物のつもり → 止める
  ['そっととびらをあける', 'open front door'], // 助詞の付かない未知語は落としてよい
  ['とびらをあけない', null],               // 否定は扱えない
]

let ng = 0
for (const [ja, want] of CASES) {
  const r = cm.toCommand(ja)
  const ok = r.command === want
  if (!ok) ng++
  console.log(`${ok ? '✓' : '✗'} ${ja.padEnd(14, '　')} → ${String(r.command).padEnd(24)}${ok ? '' : ` ★期待: ${want}`}`)
}
console.log(`\n--- ${CASES.length} 件 / 食い違い ${ng} 件 ---`)
process.exit(ng ? 1 : 0)
