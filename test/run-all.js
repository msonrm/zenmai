'use strict'
/**
 * 全作品の検査をまとめて流す入口（`npm test`）。
 *
 *   npm test                 全作品（`assets/<作品>-cmd.json` と `-ja.json` が揃っているもの）
 *   npm test -- zork2        作品を選ぶ（いくつでも）
 *
 * ★ここは**流す順番と合否の集計だけ**。検査の中身は各 `run-*.js`、作品ごとの材料は `test/works/<作品>.js`。
 * ★合否は終了コード。終了コードを返さない道具（通しの記録・煙の検査）は、出力の 1 行（`expect`）で見る ——
 *   `run-walkthrough.js` は未訳を**数える道具**で、赤にするかはここが決める（未訳 0 行を守る）。
 *   ★通しは「未訳 0 行」に加えて「**英字が残った 0 行**」も守る（2026-11-15。行の後ろだけ訳せて前が英語のまま出る穴は、未訳には数えられない）
 * ★流さないもの: native の照合（`cmd_ref.js` ↔ `cmd_test_host`）。節目まで食い違っていてよい約束（`CLAUDE.md`）
 */
const { spawnSync } = require('child_process')
const fs = require('fs')
const path = require('path')

const ROOT = path.join(__dirname, '..')
const all = require('./work.js').works()
const picked = process.argv.slice(2)
for (const w of picked) {
  if (!all.includes(w)) { console.error(`作品「${w}」は無い（ある作品: ${all.join(' ')}）`); process.exit(2) }
}
const works = picked.length ? picked : all

/** 1 本流す。返り値 = 通ったか */
function run(label, script, { args = [], env = {}, expect } = {}) {
  const t0 = Date.now()
  const r = spawnSync(process.execPath, [path.join(__dirname, script), ...args], {
    cwd: ROOT, env: { ...process.env, ...env }, encoding: 'utf8', timeout: 10 * 60 * 1000, maxBuffer: 64 << 20,
  })
  const out = (r.stdout || '') + (r.stderr || '')
  const lines = out.split('\n').filter((l) => l.trim())
  let why = ''
  if (r.error) why = String(r.error.message)
  else if (r.status !== 0) why = `終了コード ${r.status}`
  else if (expect && !expect.test(out)) why = `出力に ${expect} が無い`
  const ok = !why
  // ★1 行で言う。通ったら各検査の最後の行（件数）だけ、落ちたら末尾を見せる
  const last = (expect && lines.find((l) => expect.test(l))) || lines[lines.length - 1] || ''
  console.log(`${ok ? '✓' : '✗'} ${label.padEnd(26)} ${((Date.now() - t0) / 1000).toFixed(1).padStart(5)}s  ${ok ? last.trim().slice(0, 100) : '★' + why}`)
  if (!ok) console.log(lines.slice(-15).map((l) => '    ' + l).join('\n'))
  return ok
}

const failed = []
const step = (label, ...rest) => { if (!run(label, ...rest)) failed.push(label) }

for (const WORK of works) {
  const env = { WORK }
  const spec = fs.existsSync(path.join(__dirname, 'works', `${WORK}.js`)) ? require(`./works/${WORK}.js`) : {}
  console.log(`\n=== ${WORK} ===`)
  step(`${WORK} 入力の固定表`, 'run-cmd.js', { env })
  step(`${WORK} 呼びかけ`, 'run-actor.js', { env })
  step(`${WORK} 送る語が辞書にある`, 'run-dict.js', { env })
  step(`${WORK} 語の食い合い`, 'run-swallow.js', { env })
  step(`${WORK} 画面の語を打てる`, 'run-visible.js', { env })
  if (spec.save) step(`${WORK} セーブ`, 'run-save.js', { env })
  else console.log(`– ${WORK} セーブ: test/works/${WORK}.js に start・save が無いので流さない`)
  step(`${WORK} Web の配線`, 'run-web-dom.js', { env })
  // 煙の検査（VM が立って 1 手打てる・未訳なし）
  step(`${WORK} Glk シム`, 'run-shim.js', { env, args: ['look'], expect: /★未訳 0 ---/ })
  step(`${WORK} 端末（glkote-term）`, 'run-ja.js', { env, args: ['look'], expect: /★引けなかった 0 行/ })
  // ★通しは「最後の場所」も見る（`walkthroughEnd` / `walkthroughJaEnd` = 状態行の英語の部屋名・2026-11-15）。
  //   手の列の途中で 1 手ずれると、未訳 0 行・問題 0 件のまま結末に着かない（Zork III の木の扉で踏んだ）
  const esc = (x) => x.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')
  if (spec.walkthrough) step(`${WORK} 通し（英語の手）`, 'run-walkthrough.js', { env, args: [path.join('test', spec.walkthrough)],
    expect: new RegExp('★未訳 0 行[\\s\\S]*★英字が残った 0 行' + (spec.walkthroughEnd ? '[\\s\\S]*★最後の場所: ' + esc(spec.walkthroughEnd) + '\\s*$' : ''), 'm') })
  else console.log(`– ${WORK} 通し（英語の手）: 手の列が無い`)
  if (spec.walkthroughJa) step(`${WORK} 通し（日本語の手）`, 'run-walkthrough-ja.js', { env: { ...env, ...(spec.walkthroughJaEnd ? { ZENMAI_END: spec.walkthroughJaEnd } : {}) }, args: [path.join('test', spec.walkthroughJa)] })
  else console.log(`– ${WORK} 通し（日本語の手）: 手の列が無い`)
}

// --- 作品によらないもの（1 回だけ）---
console.log('\n=== 共通 ===')
step('呪文・口に出す語（仕組み）', 'run-spell.js')
step('Web のメニュー', 'run-web-dom.js', { env: { ZENMAI_MENU: '1' } })

console.log(`\n--- ${works.join(' ')}: ${failed.length ? `★落ちた ${failed.length} 本: ${failed.join(' / ')}` : 'すべて通った'} ---`)
process.exit(failed.length ? 1 : 0)
