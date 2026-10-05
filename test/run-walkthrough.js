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
// ★作品は環境変数 WORK（既定 zork1）。乱数は SEED（既定 0x5EED5EED）
const WORK = process.env.WORK || 'zork1'
const tr = new Translator(JSON.parse(fs.readFileSync(A(`${WORK}-ja.json`), 'utf8')))
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
    let c = cmds[n++]
    // ★検査用の指示（乱数の部屋・遠い部屋を飛ばす）: `#goto 部屋名` = プレイヤーをその部屋へ / `#give 物` = 持ち物へ / `#here 物` = いまの部屋へ（名前は正規表現・story の名前表）
    while (c && c[0] === '#') {
      const [d, ...r] = c.split(/\s+/)
      const re = new RegExp(r.join(' '), 'i')
      const { zobjs, findObj } = require('../tools/zobjs.js')
      const names = zobjs(fs.readFileSync(path.join(__dirname, '..', 'vendor', WORK, `${WORK}.z3`)))
      const player = findObj(names, /^cretin$/)[0]
      const hit = d === '#hereall' ? [] : /^\d+$/.test(r.join(' ')) ? [Number(r[0])] : findObj(names, re)   // 数字 = 物の番号そのもの
      if (!hit.length && d !== '#hereall') console.error(`★指示 ${c}: 名前に合う物が無い`)
      if (d === '#hereall') {   // 部屋でない物（プレイヤーと明かり以外）を全部いまの部屋へ呼ぶ（fuzz 用）
        const story = fs.readFileSync(path.join(__dirname, '..', 'vendor', WORK, `${WORK}.z3`))
        const rw = (a) => (story[a] << 8) | story[a + 1]
        const base = rw(0x0a) + 62
        const room = vm.get_parent(player), roomsId = vm.get_parent(room)
        const lamp = findObj(names, /^lamp$/)
        for (let o = 1; o < 256; o++) {
          const e = base + (o - 1) * 9
          if (rw(e + 7) < 64 || rw(e + 7) >= story.length) break
          if (o === player || o === roomsId || vm.get_parent(o) === roomsId || lamp.includes(o) || vm.get_child(o)) continue
          vm.insert_obj(o, room)
        }
      } else if (d === '#goto') vm.insert_obj(player, hit[0])
      else if (d === '#give') for (const o of hit) vm.insert_obj(o, player)
      else if (d === '#here') for (const o of hit) vm.insert_obj(o, vm.get_parent(player))
      c = n < cmds.length ? cmds[n++] : ''
    }
    if (!c) return finish()
    setImmediate(() => Glk.submitLine(c))
  },
})

function finish() {
  const m = tr.stats
  console.log(`--- ${cmds.length} 手を流した ---`)
  console.log(`引けた ${m.hit} 行（うち貪欲 ${m.greedy}）/ 訳さない ${m.notrans} / ★未訳 ${m.miss} 行`)
  console.log(`未訳の異なり: ${seen.size} 種`)
  // ★スロットに英語が残ったもの。**行としては引けている**ので未訳には出ない
  //   （実プレイで `a clove of garlic, and a lunch` が英語のまま出た）
  console.log(`★スロットに英語が残った語: ${m.rawWords.size} 種\n`)
  for (const [s, where] of seen) console.log(`[${where}] ${s}`)
  for (const w of m.rawWords) console.log(`[スロット] ${w}`)
  process.exit(0)
}

const vm = new ZVM()
vm.prepare(fs.readFileSync(path.join(__dirname, '..', 'vendor', WORK, `${WORK}.z3`)), { vm, Glk, GlkOte: null, Dialog: null })
Glk.init({ vm })
// ★乱数を固定して回帰を安定させる（`Glk.init` が 0 で初期化するのでその後）。
//   固定しないと「引けた行数」が実行ごとに 274〜278 と揺れ、**退行と乱数の区別がつかない**。
//   詳しくは native/capture_pairs.js
vm.xorshift_seed = process.env.SEED ? Number(process.env.SEED) : 0x5EED5EED
