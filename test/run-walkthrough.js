'use strict'
/**
 * walkthrough を流して**未訳ログ**を取る。
 *
 * ★walkthrough は「最短の勝ち筋」なので全経路は通らない。拒否メッセージ・乱数メッセージ・
 * 死に方の分岐・迷路の余分な部屋は踏まない。それでも**主要経路の組み立て行**は一気に洗える。
 *
 * 使い方: node test/run-walkthrough.js <コマンドを 1 行ずつ書いたファイル>
 */
const fs = require('fs')
const path = require('path')
const ZVM = require('ifvms/src/zvm.js')
const { createGlk } = require('../src/glk-shim.js')
const { Translator } = require('../src/translate.js')

const A = (f) => path.join(__dirname, '..', 'assets', f)
const tr = new Translator(JSON.parse(fs.readFileSync(A('zork1-ja.json'), 'utf8')))
const cmds = fs.readFileSync(process.argv[2], 'utf8').split('\n').map((s) => s.trim()).filter(Boolean)
let n = 0
let place = ''
const seen = new Map()          // 未訳の行 → 初めて出た手数

const Glk = createGlk({
  cols: 64,
  rows: 24,
  write(text) { tr.feed(text) },                       // 画面には出さない（ログだけ取る）
  status(line) { place = tr.word((line.match(/^(.*?)\s{2,}/) || [0, line])[1]) },
  update() {
    tr.flush()
    for (const s of tr.stats.missed) if (!seen.has(s)) seen.set(s, `${n} 手目 / ${place}`)
    if (Glk.waitingFor() === 'char') return setImmediate(() => Glk.submitChar(32))
    if (Glk.waitingFor() !== 'line') return
    if (n >= cmds.length) return finish()
    const c = cmds[n++]
    setImmediate(() => Glk.submitLine(c))
  },
})

function finish() {
  const m = tr.stats
  console.log(`--- ${cmds.length} 手を流した ---`)
  console.log(`引けた ${m.hit} 行（うち貪欲 ${m.greedy}）/ 訳さない ${m.notrans} / ★未訳 ${m.miss} 行`)
  console.log(`未訳の異なり: ${seen.size} 種\n`)
  for (const [s, where] of seen) console.log(`[${where}] ${s}`)
  process.exit(0)
}

const vm = new ZVM()
vm.prepare(fs.readFileSync(path.join(__dirname, '..', 'vendor', 'zork1', 'zork1.z3')), { vm, Glk, GlkOte: null, Dialog: null })
Glk.init({ vm })
