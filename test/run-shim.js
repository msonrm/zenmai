'use strict'
/**
 * 自前 Glk シムの検証。glkote-term を使わず、`src/glk-shim.js` だけで走らせる。
 * これが通れば、ブラウザ側は「シムに DOM を繋ぐだけ」になる。
 *
 * 使い方: node test/run-shim.js "open mailbox" "north" ...
 */
const fs = require('fs')
const path = require('path')
const ZVM = require('ifvms/src/zvm.js')
const { createGlk } = require('../src/glk-shim.js')
const { Translator } = require('../src/translate.js')

const STORY = path.join(__dirname, '..', 'vendor', 'zork1', 'zork1.z3')
const tr = new Translator(JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'assets', 'zork1-ja.json'), 'utf8')))
const cmds = process.argv.slice(2)
let n = 0
let statusLine = ''

const Glk = createGlk({
  cols: 80,
  rows: 24,
  write(text) { process.stdout.write(tr.feed(text)) },
  status(line) { statusLine = tr.word(line.replace(/\s{2,}.*$/, '')) + '  ' + line.replace(/^.*?\s{2,}/, '') },
  update() {
    const rest = tr.flush()
    if (rest) process.stdout.write(rest)
    if (Glk.waitingFor() === 'line') {
      if (n < cmds.length) {
        const c = cmds[n++]
        process.stdout.write(`\n[${statusLine.trim()}]\n> ${c}\n`)
        setImmediate(() => Glk.submitLine(c))
      } else {
        const m = tr.stats
        process.stderr.write(`\n--- 引けた ${m.hit} 行（うち貪欲 ${m.greedy}）/ 訳さない ${m.notrans} / ★未訳 ${m.miss} ---\n`)
        for (const s of [...m.missed].slice(0, 10)) process.stderr.write('  未訳: ' + s + '\n')
        process.exit(0)
      }
    } else if (Glk.waitingFor() === 'char') {
      setImmediate(() => Glk.submitChar(32))
    }
  },
})

const vm = new ZVM()
vm.prepare(fs.readFileSync(STORY), { vm, Glk, GlkOte: null, Dialog: null })
Glk.init({ vm })
