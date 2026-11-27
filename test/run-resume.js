'use strict'
/**
 * 「前回の続きから」と「やめる」（2026-11-27・プレイ日記から）。
 *
 * ★Web 版を**開き直す**ところまで試す: 同じ保存先（localStorage）で `run-web-dom.js` を 3 回走らせる。
 *   1. 何手か進める → 入力を待つたびに ZVM の自動保存が `zenmai-auto-<作品>` に覚える
 *   2. 開き直す → 「前回の続きから」と出て、状態行の場所と手数が 1 の続きになっている。前の段落も描き戻されている
 *   3. 「やめる」→「はい」→ 黙って止まらず「ゲームを終えた」と言う。続きの記録は消える（次は最初から）
 * ★以前は「やめる」で ZVM の `Glk.glk_exit()` が無く、「[エラー] … is not a function」が出ていた
 *
 * 使い方: node test/run-resume.js（WORK=zork2 で作品を替える）
 */
const { spawnSync } = require('child_process')
const fs = require('fs')
const os = require('os')
const path = require('path')
const work = require('./work.js')

const SPEC = work.spec()
// 手は作品のセーブの検査の手から、保存と復帰を除いたもの（場所が動く）
const steps = ((SPEC.save && SPEC.save.cmds) || ['look', 'inventory']).filter((c) => !/^(save|restore)$/.test(c))
const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'zenmai-resume-'))
const file = path.join(dir, 'store.json')
const run = (cmds) => spawnSync(process.execPath, [path.join(__dirname, 'run-web-dom.js'), ...cmds],
  { env: { ...process.env, WORK: work.WORK, ZENMAI_STORE: file }, encoding: 'utf8' })
const stat = (out) => ((out.match(/^--- 状態行: (.*)$/m) || [])[1] || '').trim()
const AUTO = `zenmai-auto-${work.WORK}`

const r1 = run(steps)
const s1 = stat(r1.stdout)
const had = AUTO in JSON.parse(fs.readFileSync(file, 'utf8'))
const r2 = run([])
const s2 = stat(r2.stdout)
const r2b = run(['look'])                   // 続きから 1 手打てる（復帰した VM が入力を受けて答える）
const r3 = run(['やめる', 'はい'])
const gone = !(AUTO in JSON.parse(fs.readFileSync(file, 'utf8')))
const r4 = run([])
fs.rmSync(dir, { recursive: true, force: true })

const lastLines = (out) => out.split('\n').filter((l) => l.trim() && !l.startsWith('---')).slice(-3).join(' / ')
const checks = [
  ['1 回目が動いた', r1.status === 0 && !!s1],
  ['入力を待つたびに続きを覚えた', had],
  ['開き直すと「前回の続きから」', /前回の続きから/.test(r2.stdout)],
  [`開き直しても場所と手数が同じ（${s1}）`, s2 === s1],
  ['続きから 1 手打てる', r2b.status === 0 && !/\[エラー\]/.test(r2b.stdout) && /場所名の段落: [1-9]/.test(r2b.stdout)],
  ['前の段落が描き戻されている', r2.stdout.split('前回の続きから')[0].includes(steps[steps.length - 1])],
  ['「やめる」で黙らない', /ゲームを終えた/.test(r3.stdout) && !/\[エラー\]/.test(r3.stdout)],
  ['やめたら続きの記録が消える', gone],
  ['やめた次は最初から', !/前回の続きから/.test(r4.stdout) && stat(r4.stdout) !== s1],
]
let ng = 0
for (const [name, ok] of checks) { if (!ok) ng++; console.log(`${ok ? '✓' : '✗'} ${name}`) }
if (ng) {
  for (const [k, r] of [['1', r1], ['2', r2], ['2b', r2b], ['3', r3], ['4', r4]]) console.log(`  [${k}] status=${r.status} ${lastLines(r.stdout)} ${r.stderr.slice(0, 300)}`)
}
console.log(`\n--- 続き: ${s1} → 開き直し: ${s2} / やめたあと: ${stat(r4.stdout)} ---`)
process.exit(ng ? 1 : 0)
