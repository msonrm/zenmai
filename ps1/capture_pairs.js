'use strict'
/**
 * 訳の層の参照データ捕獲: walkthrough を流し、Translator.line() の入出力対を全部記録する。
 * C 移植(translate.c)はこの対を 1 行残らず再現しなければならない(JS が正典)。
 *
 * 使い方: node capture_pairs.js ../test/walkthrough.txt pairs.jsonl
 * 形式: 1 行 1 JSON [raw, ja|null](null = 訳せず素通し)
 */
const fs = require('fs')
const path = require('path')
const ZVM = require('ifvms/src/zvm.js')
const Z = path.join(__dirname, '..')
const { createGlk } = require(path.join(Z, 'src/glk-shim.js'))
const { Translator } = require(path.join(Z, 'src/translate.js'))

const cmds = fs.readFileSync(process.argv[2], 'utf8').split('\n').map((s) => s.trim()).filter(Boolean)
const out = process.argv[3]

const tr = new Translator(JSON.parse(fs.readFileSync(path.join(Z, 'assets/zork1-ja.json'), 'utf8')))
const pairs = []
const origLine = tr.line.bind(tr)
tr.line = (raw) => {
  const ja = origLine(raw)
  pairs.push([raw, ja, tr.echoWord || null])
  return ja
}

let idx = 0
const Glk = createGlk({
  cols: 64, rows: 24,
  write(t) { tr.feed(t) },
  status() {},
  update() {
    tr.flush()
    if (Glk.waitingFor() === 'char') return setImmediate(() => Glk.submitChar(32))
    if (Glk.waitingFor() !== 'line') return
    if (idx >= cmds.length) {
      fs.writeFileSync(out, pairs.map((p) => JSON.stringify(p)).join('\n'))
      console.log(`${out}: ${pairs.length} 行(${cmds.length} 手)/ 未訳 ${tr.stats.miss}`)
      process.exit(0)
    }
    const c = cmds[idx++]
    // ★web 版と同じく、打った語を反響(ECHO)用に渡す(EN 入力なのでコマンドの最終語)
    const words = c.split(/\s+/)
    tr.setEcho(words[words.length - 1])
    setImmediate(() => Glk.submitLine(c))
  },
})
const vm = new ZVM()
vm.prepare(fs.readFileSync(path.join(Z, 'vendor/zork1/zork1.z3')), { vm, Glk, GlkOte: null, Dialog: null })
Glk.init({ vm })
