'use strict'
/**
 * 入力側（日本語 → 英語コマンド）の固定表を流す。
 *
 * ★表は作品ごと = `test/works/<作品>.js` の `cmd`（**実プレイで出た取りこぼしはそこに足す**）。
 *   ここにあるのは作品によらない検査（語の途中で切らない・問い返しが打った言い方を映す）と、表の流し方だけ。
 *
 * 使い方: node test/run-cmd.js            （WORK=zork2 で作品を替える。既定 zork1）
 */
const { Translator } = require('../src/translate.js')
const work = require('./work.js')

const asset = work.cmdAsset()
const cm = work.commander(asset)
const T = work.spec().cmd || {}
const CASES = T.cases || []

let ng = 0
let count = 0
const check = (ok, line) => { count++; if (!ok) ng++; console.log(`${ok ? '✓' : '✗'} ${line}`) }

// ★語彙は**語の途中で切ってはいけない**。日本語の語は小書きかな・長音では始まらないので、
//   一致の直後がそれなら、その一致は語の途中を食っている。
//   実プレイ: 「きゅうをとる」で「き」（木）が食い、残った「ゅう」が
//   **「『ゅう』は知らない言葉」として画面に出た** —— 打った人には何が起きたか分からない。
//   ★1 件ではなく類なので、**小書きで始まる断片が 1 つでも出たら赤**にする
{
  const SMALL = /^[ぁぃぅぇぉっゃゅょゎー]/
  const probes = ['きょうかしょをよむ', 'しゅうりする', 'きゃくをよぶ', 'ちょきんする', 'きゅうけいする']
  const bad = []
  for (const p of probes) {
    for (const w of cm.toCommand(p).unknown || []) if (SMALL.test(w)) bad.push(`${p} → 「${w}」`)
  }
  check(bad.length === 0, `★語の途中で切らない（${probes.length} 通り）${bad.length ? ' ★' + bad.join(' / ') : ''}`)
}
// ★総称は**候補が揃っていること**を見る（第一候補は順位付けの都合で動く）
for (const { say, label, want } of T.candidates || []) {
  const r = cm.toCommand(say)
  const got = [r.command, ...(r.alts || [])].sort().join(' / ')
  const ok = got === [...want].sort().join(' / ')
  check(ok, `${say}（${label}）  → ${got}${ok ? '' : ` ★期待: ${[...want].sort().join(' / ')}`}`)
}
for (const [ja, want] of CASES) {
  const r = cm.toCommand(ja)
  const ok = r.command === want
  check(ok, `${ja.padEnd(14, '　')} → ${String(r.command).padEnd(24)}${ok ? '' : ` ★期待: ${want}`}`)
}
// --- 訳の側（原作の 1 行 → 日本語）。打った語が訳にどう映るかは入力と対なので、ここで見る ---
if (T.passthrough || T.lines || T.echo || T.said || T.noSaid) {
  const tr = new Translator(work.jaAsset())
  if (T.passthrough) {
    const raw = T.passthrough.en
    const ja = tr.line(raw)
    tr.setPassthrough(true)
    const en = tr.line(raw)
    tr.setPassthrough(false)
    check(ja === T.passthrough.ja && en === raw, `英語モードは訳さず原文  → 日本語「${ja}」/ 英語「${en}」`)
  }
  for (const [label, rows] of Object.entries(T.lines || {})) {
    for (const [en, want] of rows) {
      const got = tr.line(en)
      check(got === want, `${label} ${en.padEnd(42)} → ${got}${got === want ? '' : ` ★期待: ${want}`}`)
    }
  }
  for (const [ja, want] of T.echo || []) {
    const r = cm.toCommand(ja)
    tr.setEcho(r.echoWord || ja)
    tr.setSaid(r.said || '')
    const last = String(r.command).split(' ').pop()
    const got = tr.line(`${last} ${last} ...`)
    check(got === want, `反響 ${ja.padEnd(10, '　')} → ${got}${got === want ? '' : ` ★期待: ${want}`}`)
  }
  for (const [ja, en, want] of T.said || []) {
    const r = cm.toCommand(ja)
    tr.setEcho(r.echoWord || ja)
    tr.setSaid(r.said || '')
    const got = tr.line(en)
    check(got === want, `打った語 ${ja.padEnd(16, '　')} → ${got}${got === want ? '' : ` ★期待: ${want}`}`)
  }
  if (T.noSaid) {
    tr.setSaid('')
    const got = tr.line(T.noSaid.en)
    check(got === T.noSaid.ja, `打った語なし（辞書へ落とす） → ${got}${got === T.noSaid.ja ? '' : ` ★期待: ${T.noSaid.ja}`}`)
  }
}
// ★問い返しは**打った言い方を映す**こと。
//   動詞の言い方に名詞が埋まっているもの（`ぜんまいをまく` `錠を開ける` …）で、
//   正式形だけから問い返すと埋まっていた名詞が消え、答えたくなる語が通らなくなる。
//   実プレイで「ぜんまいをまく → 何を巻く？ → ぜんまい → 知らない言葉」を踏んだ。
//   ★1 件ではなく**類**なので、言い方を全部なめて固定する（増えても勝手に検査される）
{
  let n = 0
  const miss = []
  for (const v of Object.values(asset.verbs)) {
    for (const ja of v.ja || []) {
      const cut = ja.indexOf('を')
      if (cut <= 0 || cut >= ja.length - 1) continue
      const r = cm.toCommand(ja)
      if (!r || !r.needsObject) continue
      n++
      // 埋まっていた名詞が問い返しに残っているか
      if (!String(r.ask).includes(ja.slice(0, cut))) miss.push(`${ja} → 「${r.ask}」`)
    }
  }
  check(miss.length === 0, `問い返しが打った言い方を映す（${n} 通り / 落ち ${miss.length}）${miss.length ? ' ★' + miss.join(' / ') : ''}`)
}
// ★実プレイの流れがつながること（問い返し → 答え）
for (const [say, answer, want, ask] of T.flows || []) {
  const r1 = cm.toCommand(say)
  const r2 = cm.toCommand(answer, { verb: r1.verbKey })
  const ok = r1.needsObject && (!ask || r1.ask === ask) && r2 && r2.command === want
  check(ok, `${say} → 「${r1.ask}」 → ${answer} → ${r2 && r2.command}${ok ? '' : ` ★期待: ${ask ? `「${ask}」 → ` : ''}${want}`}`)
}

console.log(`\n--- ${work.WORK}: ${count} 項目（固定表 ${CASES.length} 件）/ 食い違い ${ng} 件 ---`)
process.exit(ng ? 1 : 0)
