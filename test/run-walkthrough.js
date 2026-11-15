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
let lastRoom = ''               // ★状態行の英語の部屋名（最後にどこへ着いたか = 結末の見張り）
const shown = []                // 訳したあとの出力（英字の残りを数える）

const Glk = createGlk({
  cols: 64,
  rows: 24,
  write(text) { shown.push(tr.feed(text)) },           // 画面には出さない（ログだけ取る。訳したあとの字は英字の見張りに使う）
  status(line) { lastRoom = ((line.match(/^(.*?)\s{2,}/) || [0, line])[1]).trim(); place = tr.word(lastRoom) },
  update() {
    shown.push(tr.flush() || '')
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
  // ★**英字が残った行**（2026-11-15）。1 行のうち後ろの文だけ訳せたとき（貪欲な照合）や、「訳さない行」の部分一致に
  //   引っかかったとき、**英語のまま画面に出るのに未訳 0 行と数えられる**（Zork III の石の扉・胸壁の説明で踏んだ）。
  //   訳したあとの字から、英字の語が 2 つ以上並ぶ行を拾う。「訳さない」と決めた行（版権表示など）は**完全一致のときだけ**除く
  const nz = (x) => x.replace(/\s+/g, ' ').trim()
  const keep = new Set()
  for (const en of tr.notrans) keep.add(nz(en))
  //   ★除くもの: ① 起動時の版権表示と版の行（訳さない・作品ごとに年が違うので形で見る）
  //   ② **打った語の復唱**（`You can't see any blue sphere here!` の blue sphere は原作が入力をそのまま印字する。
  //      この検査は英語の手で流すので英語になる。日本語で打てば日本語）—— 手の列に出てくる語を消してから数える
  const BANNER = /^(Copyright\b|Release \d+ \/ Serial number )/
  const typed = new Set(cmds.join(' ').toLowerCase().split(/[^a-z0-9]+/).filter(Boolean))
  const untyped = (l) => l.replace(/[A-Za-z][A-Za-z0-9'-]*/g, (w) => (typed.has(w.toLowerCase()) ? '' : w))
  const latin = [...new Set(shown.join('').split('\n').map(nz))]
    .filter((l) => !keep.has(l) && !BANNER.test(l) && /[A-Za-z]{3,} +[A-Za-z]{2,}/.test(untyped(l)))
  console.log(`★英字が残った ${latin.length} 行`)
  // ★**最後に着いた場所**（2026-11-15）。手の列の途中で崩れると（聞き返しで 1 手ずれる・乱数がずれる）、未訳 0 行のまま
  //   結末に着かない。`npm test` は作品の `walkthroughEnd`（英語の部屋名）と突き合わせる
  console.log(`★最後の場所: ${lastRoom}`)
  for (const l of latin) console.log(`[英字] ${l.slice(0, 160)}`)
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
