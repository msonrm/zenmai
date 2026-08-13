'use strict'
/**
 * 日本語で打って日本語で返る、通しの検証。
 *
 *   日本語 ──command.js──> 英語コマンド ──> Z-machine（英語のまま動く）
 *                                            │
 *          画面 <──translate.js── 英語の出力 <┘
 *
 * 使い方: node test/run-full-ja.js "郵便箱を開ける" "北へ行く" ...
 */
const fs = require('fs')
const path = require('path')
const ZVM = require('ifvms/src/zvm.js')
const { createGlk } = require('../src/glk-shim.js')
const { Translator } = require('../src/translate.js')
const { createCommander } = require('../src/command.js')

const A = (f) => path.join(__dirname, '..', 'assets', f)
const tr = new Translator(JSON.parse(fs.readFileSync(A('zork1-ja.json'), 'utf8')))
const cm = createCommander(JSON.parse(fs.readFileSync(A('zork1-cmd.json'), 'utf8')))
const inputs = process.argv.slice(2)
let n = 0
let place = ''

const Glk = createGlk({
  cols: 64,
  rows: 24,
  write(text) { process.stdout.write(tr.feed(text).replace(/^[ \t]*>+[ \t]*$/gm, '')) },
  status(line) { place = tr.word((line.match(/^(.*?)\s{2,}/) || [0, line])[1]) },
  update() {
    const rest = tr.flush().replace(/^[ \t]*>+[ \t]*$/gm, '')
    if (rest.trim()) process.stdout.write(rest)
    if (Glk.waitingFor() === 'char') return setImmediate(() => Glk.submitChar(32))
    if (Glk.waitingFor() !== 'line') return
    if (n >= inputs.length) {
      const m = tr.stats
      process.stderr.write(`\n--- 出力: 引けた ${m.hit} 行 / 未訳 ${m.miss} 行 ---\n`)
      return process.exit(0)
    }
    const ja = inputs[n++]
    const r = cm.toCommand(ja)
    process.stdout.write(`\n[${place}]\n> ${ja}`)
    if (!r.command) { process.stdout.write(`  ……（${r.note || r.trace}）\n`); return setImmediate(() => Glk.submitLine('look')) }
    process.stdout.write(`   → ${r.command}${r.unknown.length ? '  ※残: ' + r.unknown.join('|') : ''}\n`)
    setImmediate(() => Glk.submitLine(r.command))
  },
})

const vm = new ZVM()
vm.prepare(fs.readFileSync(path.join(__dirname, '..', 'vendor', 'zork1', 'zork1.z3')), { vm, Glk, GlkOte: null, Dialog: null })
Glk.init({ vm })
