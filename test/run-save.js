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
const ZVM = require('ifvms/src/zvm.js')
const { createGlk } = require('../src/glk-shim.js')

// ★作品は環境変数 WORK（既定 zork1）。セーブ枠は作品ごと（`slot`）: Zork I は従来の `zenmai-save`、ほかは `zenmai-save-<作品>`
// ★始まりの部屋と手の列は作品の事実 = `test/works/<作品>.js`（`start`・`save.cmds`: 何かする → save → 2 歩 → restore → look）
const work = require('./work.js')
const WORK = work.WORK
const SPEC = work.spec()
if (!SPEC.start || !SPEC.save) { console.error(`test/works/${WORK}.js に start と save.cmds が要る`); process.exit(2) }
const SLOT = WORK === 'zork1' ? 'zenmai-save' : `zenmai-save-${WORK}`   // ★枠の名前は web/main.js の決まり（Zork I だけ昔の名前）
const START = SPEC.start
const CMDS = SPEC.save.cmds
const saved = new Map()
let n = 0
let out = ''
let last = ''

const Glk = createGlk({
  cols: 64,
  rows: 24,
  slot: WORK === 'zork1' ? undefined : SLOT,
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
  const bytes = saved.get(SLOT)
  const checks = [
    ['保存できた（作品ごとの枠）', !!bytes && bytes.length > 0],
    ['★ほかの作品の枠に書いていない', [...saved.keys()].every((k) => k === SLOT)],
    ['保存の中身が Quetzal 形式', !!bytes && String.fromCharCode(...bytes.slice(0, 4)) === 'FORM'],
    ['復帰で場所が戻った', place === START],
    ['復帰の返事が出た', /Ok\./.test(out) || new RegExp(START).test(out)],
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
vm.prepare(work.story(),
  { vm, Glk, GlkOte: null, Dialog: null })
Glk.init({ vm })
