'use strict'
/**
 * 登場人物への呼びかけ（Zork II のロボット）: 「呼び名、命令」→ `robot, <命令>`。
 * 中の命令は普通の道。命令形（とれ・あけろ・いけ）も受ける。
 *   node test/run-actor.js            （WORK=zork2 で作品を替える。既定 zork1）
 *
 * ★相手と呪文は**作品の語彙にあればそれを使う**（Zork II = 本物のロボット）。無い作品（Zork I）は
 *   仕組みだけを見るために、ここで 1 人と 1 つを書き足す。
 * ★命令の中の物は作品の語彙なので、物を伴う命令は `test/works/<作品>.js` の `actorOrders`
 */
const work = require('./work.js')

const asset = work.cmdAsset()
if (!(asset.actors || []).some((a) => a.word === 'robot')) {
  asset.actors = [...(asset.actors || []), { en: 'ROBOT', word: 'robot', form: 'ロボット', yomi: ['ろぼっと'], alts: ['ロボ', 'ろぼ'] }]
}
if (!(asset.spells || []).some((s) => s.form === 'こおりつけ')) {
  asset.spells = [...(asset.spells || []), { en: 'FREEZE', word: 'freeze', form: 'こおりつけ', alts: [] }]
}
const c = work.commander(asset)
const orders = work.spec().actorOrders || [
  ['ろぼっと、ランタンをとれ', 'robot, take lamp'],
  ['ろぼっと、ふくろをあけろ', 'robot, open bag'],
]

const cases = [
  ['ろぼっと、きたへいけ', 'robot, north'],
  ['ロボット、きたへいけ', 'robot, north'],
  ['ろぼっと、きたへいく', 'robot, north'],           // 終止形でもよい
  ['ろぼっとよ、にしへいけ', 'robot, west'],
  ['ろぼ、ひがしへいけ', 'robot, east'],
  ['ろぼっと，みなみへいけ', 'robot, south'],         // 全角のコンマ
  ...orders,
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
console.log(`\n--- ${work.WORK}: ${cases.length + 5} 件 / 食い違い ${bad} 件 ---`)
process.exit(bad ? 1 : 0)
