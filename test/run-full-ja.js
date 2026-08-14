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
// ★別案（箱 = mailbox / case / …）を順に試す。「ここには見当たらない」は手数を消費しない
let trial = null
let pendingVerb = null   // 原作が聞き返している最中の動詞
let rawSince = ''
const NOT_HERE = /can't see any .* here!/i
const sink = (t) => { if (trial) trial.buf += t; else process.stdout.write(t) }

// ★セーブの往復もここで試せるように、記憶の上のファイル置き場を渡す
const saved = new Map()
const Glk = createGlk({
  cols: 64,
  rows: 24,
  files: { read: (n) => saved.get(n) || null, write: (n, b) => { saved.set(n, b) } },
  write(text) { rawSince += text; sink(tr.feed(text).replace(/^[ \t]*>+[ \t]*$/gm, '')) },
  status(line) { place = tr.word((line.match(/^(.*?)\s{2,}/) || [0, line])[1]) },
  update() {
    const rest = tr.flush().replace(/^[ \t]*>+[ \t]*$/gm, '')
    if (rest.trim()) sink(rest)
    if (trial) {
      if (NOT_HERE.test(rawSince) && trial.alts.length) {
        const next = trial.alts.shift()
        // ★全部外れたときは**最初の返事**を見せる（打っていない名前を出さないため）
        if (trial.first === null) trial.first = trial.buf
        trial.buf = ''
        process.stdout.write(`   → ${next}  ……別の物として試す\n`)
        rawSince = ''
        return setImmediate(() => Glk.submitLine(next))
      }
      // ★どれも無かった。**打った言葉**で断る（第一候補の名前を出さない）
      const t = NOT_HERE.test(rawSince) && trial.first !== null
        ? `${trial.disp}など、ここには見当たらない。\n` : trial.buf
      trial = null
      if (t.trim()) process.stdout.write(t)
    }
    if (Glk.waitingFor() === 'char') return setImmediate(() => Glk.submitChar(32))
    if (Glk.waitingFor() !== 'line') return
    if (n >= inputs.length) {
      const m = tr.stats
      process.stderr.write(`\n--- 出力: 引けた ${m.hit} 行 / 未訳 ${m.miss} 行`
        + (m.rawWords.size ? ` / ★スロットに英語 ${m.rawWords.size} 種: ${[...m.rawWords].join(' | ')}` : '') + ' ---\n')
      return process.exit(0)
    }
    const ja = inputs[n++]
    const r = cm.toCommand(ja, { verb: pendingVerb })
    process.stdout.write(`\n[${place}]\n> ${ja}`)
    if (r.needsObject) {
      pendingVerb = r.verbKey
      process.stdout.write(`  ……（${r.ask}）\n`)
      return setImmediate(() => Glk.submitLine('look'))
    }
    if (!r.command) { process.stdout.write(`  ……（${r.note || r.trace}）\n`); return setImmediate(() => Glk.submitLine('look')) }
    process.stdout.write(`   → ${r.command}${r.unknown.length ? '  ※残: ' + r.unknown.join('|') : ''}\n`)
    trial = r.alts && r.alts.length ? { alts: r.alts.slice(), buf: '', first: null, disp: r.objDisp } : null
    pendingVerb = null
    rawSince = ''
    setImmediate(() => Glk.submitLine(r.command))
  },
})

const vm = new ZVM()
vm.prepare(fs.readFileSync(path.join(__dirname, '..', 'vendor', 'zork1', 'zork1.z3')), { vm, Glk, GlkOte: null, Dialog: null })
Glk.init({ vm })
