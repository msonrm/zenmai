'use strict'
/**
 * スクロールデモ用に、実パイプライン(ZVM + glk-shim + Translator)から
 * 冒頭 + 数手ぶんのターン出力を捕獲して turns.json に書く。
 *
 * 入力欄に見せる日本語(ja)と VM へ送る英語(en)は対で持つ(入力層はまだ無いので)。
 */
const fs = require('fs')
const path = require('path')
const ZVM = require('ifvms/src/zvm.js')
const Z = path.join(__dirname, '..')
const { createGlk } = require(path.join(Z, 'src/glk-shim.js'))
const { Translator } = require(path.join(Z, 'src/translate.js'))

const CMDS = [
  { ja: 'ゆうびんばこをあける', en: 'open mailbox' },
  { ja: 'ちらしをよむ', en: 'read leaflet' },
  { ja: 'みなみへいく', en: 'south' },
  { ja: 'ひがしへいく', en: 'east' },
  { ja: 'まどをあける', en: 'open window' },
  { ja: 'まどからはいる', en: 'enter window' },
]

const tr = new Translator(JSON.parse(fs.readFileSync(path.join(Z, 'assets/zork1-ja.json'), 'utf8')))
const turns = []          // { ja, lines[] } — 先頭は ja=null の冒頭
let buf = ''
let idx = 0

function flushTurn(ja) {
  // プロンプトだけの行を捨て、前後の空行を刈る
  const lines = buf.split('\n').map((s) => s.replace(/^\s*>+\s*$/, '')).map((s) => s.replace(/^\s*>\s*/, ''))
  while (lines.length && !lines[0].trim()) lines.shift()
  while (lines.length && !lines[lines.length - 1].trim()) lines.pop()
  turns.push({ ja, lines })
  buf = ''
}

const Glk = createGlk({
  cols: 64, rows: 24,
  write(t) { buf += tr.feed(t) },
  status() {},
  update() {
    buf += tr.flush()
    if (Glk.waitingFor() !== 'line') return
    flushTurn(idx === 0 ? null : CMDS[idx - 1].ja)
    if (idx >= CMDS.length) {
      fs.writeFileSync(path.join(__dirname, 'turns.json'), JSON.stringify(turns, null, 1))
      console.log(`turns.json: 冒頭 + ${CMDS.length} 手`)
      for (const t of turns) console.log('---', t.ja || '(冒頭)', '---\n' + t.lines.join('\n'))
      process.exit(0)
    }
    const c = CMDS[idx++]
    setImmediate(() => Glk.submitLine(c.en))
  },
})
const vm = new ZVM()
vm.prepare(fs.readFileSync(path.join(Z, 'vendor/zork1/zork1.z3')), { vm, Glk, GlkOte: null, Dialog: null })
Glk.init({ vm })
