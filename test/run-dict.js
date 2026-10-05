'use strict'
/**
 * 入力層が**原作へ送る英語の語**が、story の辞書に全部あるかを調べる（静的な検査）。
 *
 * 辞書に無い語を送ると原作は「I don't know the word …」と返す。日本語を通しても、送る英語が辞書に無ければ
 * その物・動詞は**打てない**（語彙の書き間違い・原作の綴りの食い違いを機械で拾う）。
 * ★版 3 の辞書は語を**6 字で切って**持つ（`newspaper` = `newspa`）ので、6 字に切って突き合わせる。
 *
 * 使い方: WORK=zork2 node test/run-dict.js
 */
const fs = require('fs')
const path = require('path')
const WORK = process.env.WORK || 'zork1'
const story = fs.readFileSync(path.join(__dirname, '..', 'vendor', WORK, `${WORK}.z3`))
const cmd = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'assets', `${WORK}-cmd.json`), 'utf8'))

// --- 辞書（ヘッダ 08h）---
const rw = (a) => (story[a] << 8) | story[a + 1]
const dict = rw(0x08)
const nsep = story[dict]
const entLen = story[dict + 1 + nsep]
const n = rw(dict + 2 + nsep)
const alpha = ['abcdefghijklmnopqrstuvwxyz', 'ABCDEFGHIJKLMNOPQRSTUVWXYZ', ' \n0123456789.,!?_#\'"/\\-:()']
const words = new Set()
for (let i = 0; i < n; i++) {
  const a = dict + 4 + nsep + i * entLen
  let s = '', shift = 0
  for (let k = 0; k < 2; k++) {
    const w = rw(a + k * 2)
    for (const c of [(w >> 10) & 31, (w >> 5) & 31, w & 31]) {
      if (c === 0) s += ' '
      else if (c === 4) shift = 1
      else if (c === 5) shift = 2
      else if (c >= 6) { s += alpha[shift][c - 6]; shift = 0 }
    }
  }
  words.add(s.trim().toLowerCase())
}
const inDict = (w) => words.has(w.toLowerCase().slice(0, 6))

const need = new Map()   // 語 → 出どころ
const want = (w, from) => { for (const t of String(w || '').toLowerCase().split(/\s+/).filter(Boolean)) if (!need.has(t)) need.set(t, from) }
for (const [k, o] of Object.entries(cmd.objects || {})) {
  // ★送るのは**先頭の名詞・先頭の形容詞**だけ（残りの同義語は送らない。ZIL の綴りの癖が混じるものを数えると偽の赤になる）
  if ((o.nouns || [])[0]) want(o.nouns[0], `物 ${k} の名詞`)
  if ((o.adjs || [])[0]) want(o.adjs[0], `物 ${k} の形容詞`)
}
for (const k of Object.keys(cmd.verbs || {})) want(k, `動詞 ${k}`)
for (const k of Object.keys(cmd.preps || {})) want(k, `前置詞 ${k}`)
for (const h of cmd.hypernyms || []) if (h.noun) want(h.noun, `上位語 ${h.form}`)
for (const a of cmd.actors || []) want(a.word, `呼びかけ ${a.en}`)
for (const s of cmd.spells || []) want(s.word, `呪文 ${s.en}`)
for (const s of cmd.speech || []) want(s.word, `口に出す語 ${s.en}`)
for (const d of ['north', 'south', 'east', 'west', 'northeast', 'northwest', 'southeast', 'southwest', 'up', 'down', 'in', 'out', 'land', 'all', 'except', 'again', 'oops']) want(d, `方角・パーサの語 ${d}`)

const miss = [...need].filter(([w]) => !inDict(w))
console.log(`辞書 ${words.size} 語 / 調べた語 ${need.size}`)
for (const [w, from] of miss) console.log(`✗ ${w}（${from}）`)
console.log(`\n--- ${miss.length} 件が辞書に無い ---`)
process.exit(miss.length ? 1 : 0)
