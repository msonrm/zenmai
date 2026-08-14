'use strict'
/**
 * セーブと復帰の往復。
 *
 * ★`save` を打つと**固まっていた**。ZVM は `glk_fileref_create_by_prompt` の
 * 返り値を待つのではなく、`glk_blocking_call` を立てて止まり、こちらが
 * `vm.resume(fref)` を呼ぶまで動かない。null を返して放置していたのが原因。
 *
 * 使い方: node test/run-save.js
 */
const fs = require('fs')
const path = require('path')
const ZVM = require('ifvms/src/zvm.js')
const { createGlk } = require('../src/glk-shim.js')

const CMDS = ['open mailbox', 'save', 'north', 'north', 'restore', 'look']
const saved = new Map()
let n = 0
let out = ''
let last = ''

const Glk = createGlk({
  cols: 64,
  rows: 24,
  files: { read: (k) => saved.get(k) || null, write: (k, b) => { saved.set(k, b) } },
  write(t) { out += t },
  status(line) { last = line },
  update() {
    if (Glk.waitingFor() === 'char') return setImmediate(() => Glk.submitChar(32))
    if (Glk.waitingFor() !== 'line') return
    if (n >= CMDS.length) return finish()
    const c = CMDS[n++]
    out = ''
    setImmediate(() => Glk.submitLine(c))
  },
})

function finish() {
  const place = (last.match(/^(.*?)\s{2,}/) || [0, last])[1].trim()
  const bytes = saved.get('zenmai-save')
  const checks = [
    ['保存できた', !!bytes && bytes.length > 0],
    ['保存の中身が Quetzal 形式', !!bytes && String.fromCharCode(...bytes.slice(0, 4)) === 'FORM'],
    ['復帰で場所が戻った', place === 'West of House'],
    ['復帰の返事が出た', /Ok\./.test(out) || /West of House/.test(out)],
  ]
  let ng = 0
  for (const [name, ok] of checks) {
    if (!ok) ng++
    console.log(`${ok ? '✓' : '✗'} ${name}`)
  }
  console.log(`\n--- 保存 ${bytes ? bytes.length : 0} バイト / 復帰後の場所: ${place} ---`)
  process.exit(ng ? 1 : 0)
}

const vm = new ZVM()
vm.prepare(fs.readFileSync(path.join(__dirname, '..', 'vendor', 'zork1', 'zork1.z3')),
  { vm, Glk, GlkOte: null, Dialog: null })
Glk.init({ vm })
