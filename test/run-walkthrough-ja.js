'use strict'
/**
 * 日本語の手の列を流して、**入力の取りこぼし**と**未訳**を洗い出す（公開前の通しの確認）。
 *
 *   日本語 ──command.js──> 英語コマンド ──> Z-machine ──> 英語の返事 ──translate.js──> 日本語
 *
 * 英語で流す `run-walkthrough.js` は**訳**の穴を見つけるが、日本語の言い回しが通るかは見ない。
 * こちらは日本語で打ち、次を**問題**として数える（実際に遊んだときの「通らない」を機械で拾う）:
 *   - 入力層が断った（command が null・知らない言葉・原作にない言い方・動詞が見つからない）
 *   - 原作が**入力を受け付けなかった**返事（I don't know the word / That sentence isn't one I recognize /
 *     There was no verb / You can't see any … here!（別案も尽きたとき）/ What do you want to … / Which … do you mean）
 *   - 未訳の行・スロットに英語が残った語
 * ★原作の「失敗」（行けない・持っていない）は問題にしない（乱数や手順の食い違いで起きうるので）。
 *
 * 手の列: 1 行 1 手。`#goto 部屋名|番号` / `#give 物` / `#here 物` / `#hereall`（run-walkthrough.js と同じ検査用の指示）。
 * 使い方: WORK=zork2 node test/run-walkthrough-ja.js test/walkthrough-zork2-ja.txt
 *   SEED=…（乱数）・VERBOSE=1（全部の手と返事を出す）
 */
const fs = require('fs')
const path = require('path')
const ZVM = require('ifvms/src/zvm.js')
const { createGlk } = require('../src/glk-shim.js')
const { Translator } = require('../src/translate.js')
const { createCommander } = require('../src/command.js')

const WORK = process.env.WORK || 'zork2'
const A = (f) => path.join(__dirname, '..', 'assets', f)
const tr = new Translator(JSON.parse(fs.readFileSync(A(`${WORK}-ja.json`), 'utf8')))
const cm = createCommander(JSON.parse(fs.readFileSync(A(`${WORK}-cmd.json`), 'utf8')))
const steps = fs.readFileSync(process.argv[2], 'utf8').split('\n').map((s) => s.trim()).filter((s) => s && !s.startsWith('//'))

// 原作が入力を受け付けなかった返事
//   ★「ここには見当たらない」「どの◯◯のことか」は**状態に依る**（その物が今そこに居るか）ので問題にしない。
//     原作はその語を**知っている**から返せる返事で、入力層の語は通っている。問題にするのは**語や文を知らない**返事だけ
const REJECT = [
  /I don't know the word/, /That sentence isn't one I recognize/, /There was no verb/, /I don't understand/,
  /You used the word/, /What do you want to /, /There seems to be a noun missing/,
  /You can't use multiple/, /That doesn't make sense/,
]
const NOT_HERE = /can't see any .* here!/i      // 別案を順に試す（手数を消費しない）ためにだけ使う

let n = 0
let place = ''
let rawSince = ''
let cur = null                 // いまの手 { ja, r, en, raw[] }
let trial = null               // 別案を順に試している最中
let pendingVerb = null
const problems = []
const log = []
const seenMiss = new Map()

const note = (kind, detail) => problems.push({ n, ja: cur ? cur.ja : '', kind, detail, en: cur ? cur.en : '' })

const Glk = createGlk({
  cols: 64,
  rows: 24,
  write(text) { rawSince += text; tr.feed(text) },
  status(line) { place = tr.word((line.match(/^(.*?)\s{2,}/) || [0, line])[1]) },
  update() {
    tr.flush()
    for (const s of tr.stats.missed) if (!seenMiss.has(s)) { seenMiss.set(s, true); note('未訳', s) }
    if (trial && Glk.waitingFor() === 'line') {
      if (NOT_HERE.test(rawSince) && trial.alts.length) {
        const next = trial.alts.shift()
        rawSince = ''
        cur.en = next
        return setImmediate(() => Glk.submitLine(next))
      }
      trial = null
    }
    if (Glk.waitingFor() === 'char') return setImmediate(() => Glk.submitChar(32))
    if (Glk.waitingFor() !== 'line') return
    // 直前の手の返事を調べる
    if (cur) {
      const flat = rawSince.replace(/\s+/g, ' ')
      const bad = REJECT.find((re) => re.test(flat))
      if (cur.r && cur.r.command && bad) note('原作が受けなかった', `${cur.en} → ${flat.slice(0, 100)}`)
      if (process.env.VERBOSE) log.push(`${String(n).padStart(3)} ${cur.ja}  → ${cur.en || '(送らない)'}\n      ${flat.slice(0, 120)}`)
    }
    // 指示行・次の手
    let c
    for (;;) {
      if (n >= steps.length) return finish()
      c = steps[n++]
      if (c[0] !== '#') break
      const [d, ...r] = c.split(/\s+/)
      const { zobjs, findObj } = require('../tools/zobjs.js')
      const names = zobjs(fs.readFileSync(path.join(__dirname, '..', 'vendor', WORK, `${WORK}.z3`)))
      const player = findObj(names, /^cretin$/)[0]
      const hit = d === '#hereall' ? [] : /^\d+$/.test(r.join(' ')) ? [Number(r[0])] : findObj(names, new RegExp(r.join(' '), 'i'))
      if (!hit.length && d !== '#hereall') console.error(`★指示 ${c}: 名前に合う物が無い`)
      if (d === '#hereall') {
        const story = fs.readFileSync(path.join(__dirname, '..', 'vendor', WORK, `${WORK}.z3`))
        const rw = (a) => (story[a] << 8) | story[a + 1]
        const base = rw(0x0a) + 62
        const room = vm.get_parent(player), roomsId = vm.get_parent(room)
        for (let o = 1; o < 256; o++) {
          const e = base + (o - 1) * 9
          if (rw(e + 7) < 64 || rw(e + 7) >= story.length) break
          if (o === player || o === roomsId || vm.get_parent(o) === roomsId || vm.get_child(o)) continue
          vm.insert_obj(o, room)
        }
      } else if (d === '#goto') vm.insert_obj(player, hit[0])
      else if (d === '#give') for (const o of hit) vm.insert_obj(o, player)
      else if (d === '#here') for (const o of hit) vm.insert_obj(o, vm.get_parent(player))
    }
    const r = cm.toCommand(c, { verb: pendingVerb })
    cur = { ja: c, r, en: r.command || '' }
    rawSince = ''
    pendingVerb = null
    if (r.needsObject) {
      pendingVerb = r.verbKey
      note('入力層が聞き返した', `${r.ask || ''}`)
      return setImmediate(() => Glk.submitLine('look'))
    }
    if (!r.command) {
      note('入力層が断った', `${r.trace}${r.unknown && r.unknown.length ? '（' + r.unknown.join('|') + '）' : ''}${r.note ? ' ' + r.note : ''}`)
      return setImmediate(() => Glk.submitLine('look'))
    }
    if (r.unknown && r.unknown.length) note('読み取れなかった残り', r.unknown.join('|'))
    tr.setEcho(r.echoWord || c)
    tr.setSaid(r.said || '')
    trial = r.alts && r.alts.length ? { alts: r.alts.slice() } : null
    setImmediate(() => Glk.submitLine(r.command))
  },
})

function finish() {
  const m = tr.stats
  console.log(`--- ${steps.filter((s) => s[0] !== '#').length} 手を日本語で流した ---`)
  console.log(`出力: 引けた ${m.hit} 行 / 訳さない ${m.notrans} / ★未訳 ${m.miss} 行 / ★スロットに英語 ${m.rawWords.size} 種`)
  if (process.env.VERBOSE) console.log('\n' + log.join('\n') + '\n')
  const byKind = {}
  for (const p of problems) (byKind[p.kind] = byKind[p.kind] || []).push(p)
  console.log(`★問題 ${problems.length} 件`)
  for (const [k, ps] of Object.entries(byKind)) {
    console.log(`\n[${k}] ${ps.length} 件`)
    for (const p of ps.slice(0, 40)) console.log(`  ${String(p.n).padStart(3)} 手目「${p.ja}」 ${p.detail}`)
  }
  for (const w of m.rawWords) console.log(`[スロットに英語] ${w}`)
  process.exit(problems.length || m.rawWords.size ? 1 : 0)
}

const vm = new ZVM()
vm.prepare(fs.readFileSync(path.join(__dirname, '..', 'vendor', WORK, `${WORK}.z3`)), { vm, Glk, GlkOte: null, Dialog: null })
Glk.init({ vm })
vm.xorshift_seed = process.env.SEED ? Number(process.env.SEED) : 0x5EED5EED
