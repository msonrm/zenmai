'use strict'
/**
 * 呪文（Zork II の杖）の入力: 句まるごとの固定句 → 原作の `say "<語>"`。
 * 呪文表の本物は原簿（非公開）の `## 呪文`。ここでは仕組みだけを小さな表で確かめる。
 *   node test/run-spell.js
 */
const { createCommander } = require('../src/command.js')

const asset = {
  spells: [
    { en: 'FREEZE', word: 'freeze', form: 'こおりつけ', alts: ['凍りつけ', 'こおれ'] },
    { en: 'FLOAT', word: 'float', form: 'こうどをとれ', alts: ['うけ', '浮け'] },
    { en: 'FRY', word: 'fry', form: 'こげろ', alts: ['やけろ'] },
    { en: 'FIREPROOF', word: 'fireproof', form: 'こげるな', alts: ['もえるな'] },
  ],
  // 口に出す語（Zork II のなぞなぞ）。裸の語は物の名前なので、カギカッコか「答える・言う」が要る
  speech: [{ en: 'WELL', word: 'well', form: '井戸', yomi: ['いど'], alts: ['井戸だ', 'いどだ', '井戸です', 'ウェル'] }],
}
const c = createCommander(asset)
const cases = [
  ['こおりつけ', 'say "freeze"'],
  ['こおれ', 'say "freeze"'],
  ['「こおりつけ」と唱える', 'say "freeze"'],
  ['こおりつけとなえる', 'say "freeze"'],
  ['凍りつけ！', 'say "freeze"'],
  ['コオリツケ', 'say "freeze"'],                   // カタカナでも当たる
  ['こうどをとれ。', 'say "float"'],
  ['うけと言う', 'say "float"'],
  ['こげろ', 'say "fry"'],
  ['こげるな', 'say "fireproof"'],                  // 「な」で終わっても否定として止めない
  ['もえるな', 'say "fireproof"'],
]
let bad = 0
for (const [ja, en] of cases) {
  const r = c.toCommand(ja)
  const ok = r.command === en && r.trace === '呪文'
  if (!ok) bad++
  console.log(`${ok ? '✓' : '✗'} ${ja} → ${r.command}${ok ? '' : `（期待 ${en} / ${r.trace}）`}`)
}
// 口に出す語
const speechCases = [
  ['「井戸」と答える', 'answer "well"'], ['井戸と答える', 'answer "well"'], ['いどとこたえる', 'answer "well"'],
  ['「井戸」', 'say "well"'], ['井戸と言う', 'say "well"'], ['「いど」と言う', 'say "well"'], ['井戸だ', 'say "well"'], ['ウェルと答える', 'answer "well"'],
]
for (const [ja, en] of speechCases) {
  const r = c.toCommand(ja)
  const ok = r.command === en && r.trace === '口に出す語'
  if (!ok) bad++
  console.log(`${ok ? '✓' : '✗'} ${ja} → ${r.command}${ok ? '' : `（期待 ${en} / ${r.trace}）`}`)
}
// 裸の「井戸」は物の名前のまま（答えにしない）
{
  const r = c.toCommand('井戸')
  const ok = r.trace !== '口に出す語'
  if (!ok) bad++
  console.log(`${ok ? '✓' : '✗'} 裸の「井戸」は答えにしない（${r.trace}）`)
}
// 呪文でない語は呪文にしない（部分一致で食わない）
for (const ja of ['こおり', 'こげ', 'こうど']) {
  const r = c.toCommand(ja)
  const ok = r.trace !== '呪文'
  if (!ok) bad++
  console.log(`${ok ? '✓' : '✗'} ${ja} は呪文にしない（${r.trace}）`)
}
console.log(`\n--- ${cases.length + speechCases.length + 4} 件 / 食い違い ${bad} 件 ---`)
process.exit(bad ? 1 : 0)
