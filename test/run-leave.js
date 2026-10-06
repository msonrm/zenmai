'use strict'
/**
 * 「〜から出る／降りる」（Zork II の桶と気球）。作品の語彙を使う。
 *   WORK=zork2 node test/run-leave.js
 */
const fs = require('fs')
const path = require('path')
const { createCommander } = require('../src/command.js')
const WORK = process.env.WORK || 'zork2'
const c = createCommander(JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'assets', `${WORK}-cmd.json`), 'utf8')))
const cases = [
  ['ばけつからでる', 'exit bucket'],
  ['おけからでる', 'exit bucket'],
  ['かごからおりる', 'disembark balloon'],
  ['かごからさる', 'leave balloon'],
  ['でる', 'exit'],
]
let bad = 0
for (const [ja, en] of cases) {
  const r = c.toCommand(ja)
  const ok = r.command === en
  if (!ok) bad++
  console.log(`${ok ? '✓' : '✗'} ${ja} → ${r.command}${ok ? '' : `（期待 ${en} / ${r.trace}）`}`)
}
console.log(`\n--- ${cases.length} 件 / 食い違い ${bad} 件 ---`)
process.exit(bad ? 1 : 0)
