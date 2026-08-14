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
  // --- 原作にない言い方には案内を返す（2026-08-14 実プレイ）---
  ['けんをつかう', null],                  // 「使う」に当たる動詞は原作にない
  ['ほんをよむ', 'read book'],             // 「本」が語彙に無かった
  ['ふたをあける', 'open machine'],        // 「蓋」は機械の一部
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
for (const [ja, want] of CASES) {
  const r = cm.toCommand(ja)
  const ok = r.command === want
  if (!ok) ng++
  console.log(`${ok ? '✓' : '✗'} ${ja.padEnd(14, '　')} → ${String(r.command).padEnd(24)}${ok ? '' : ` ★期待: ${want}`}`)
}
console.log(`\n--- ${CASES.length} 件 / 食い違い ${ng} 件 ---`)
process.exit(ng ? 1 : 0)
