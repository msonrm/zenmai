'use strict'
/**
 * 登場人物への呼びかけ（Zork II のロボット）: 「呼び名、命令」→ `robot, <命令>`。
 * 中の命令は普通の道（語彙は Zork I のものを借りる）。命令形（とれ・あけろ・いけ）も受ける。
 *   node test/run-actor.js
 */
const fs = require('fs')
const path = require('path')
const { createCommander } = require('../src/command.js')

const asset = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'assets', 'zork1-cmd.json'), 'utf8'))
asset.actors = [{ en: 'ROBOT', word: 'robot', form: 'ロボット', yomi: ['ろぼっと'], alts: ['ロボ', 'ろぼ'] }]
asset.spells = [{ en: 'FREEZE', word: 'freeze', form: 'こおりつけ', alts: [] }]
const c = createCommander(asset)

const cases = [
  ['ろぼっと、きたへいけ', 'robot, north'],
  ['ロボット、きたへいけ', 'robot, north'],
  ['ろぼっと、きたへいく', 'robot, north'],           // 終止形でもよい
  ['ろぼっとよ、にしへいけ', 'robot, west'],
  ['ろぼ、ひがしへいけ', 'robot, east'],
  ['ろぼっと，みなみへいけ', 'robot, south'],         // 全角のコンマ
  ['ろぼっと、ランタンをとれ', 'robot, take lamp'],
  ['ろぼっと、ふくろをあけろ', 'robot, open bag'],
]
let bad = 0
const ok = (cond, label) => { if (!cond) bad++; console.log(`${cond ? '✓' : '✗'} ${label}`) }
for (const [ja, en] of cases) {
  const r = c.toCommand(ja)
  ok(r.command === en && r.trace === '呼びかけ' && !r.unknown.length, `${ja} → ${r.command}${r.command === en ? '' : `（期待 ${en}）`}`)
}
// 読点が無ければ呼びかけではない（物への命令）
let r = c.toCommand('ろぼっときたへいく')
ok(r.trace !== '呼びかけ', `読点なしは呼びかけにしない（${r.trace}）`)
// 空の命令・呪文は止める
r = c.toCommand('ろぼっと、')
ok(r.command === null, `命令が空なら送らない（${r.trace}）`)
r = c.toCommand('ろぼっと、こおりつけ')
ok(r.command === null, `呪文は呼びかけの中で唱えない（${r.trace}）`)
// 知らない言葉は送らない（頭に robot, を付けない）
r = c.toCommand('ろぼっと、ほげほげをとれ')
ok(r.command === null || !/^robot, /.test(r.command || ''), `未知語は黙って送らない（${r.command} / ${r.trace}）`)
// エコーは「呼び名、命令」
r = c.toCommand('ろぼっと、きたへいけ')
ok(/^ロボット、/.test(r.echo), `エコー: ${r.echo}`)
console.log(`\n--- ${cases.length + 5} 件 / 食い違い ${bad} 件 ---`)
process.exit(bad ? 1 : 0)
