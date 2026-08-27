'use strict'
/**
 * ★**短い読みが語の中を食って、黙って別の物になる**のを見張る。
 *
 * 原簿のヘッダにこうある —— 「`×` を後置した読みは**ルビ専用**（`戸(と×)` ——
 * 1 モーラの読みを入力語彙に入れると語の中の同じ音を食う）」。仕組みはあるのに、
 * **入れてしまった短い読みが残っている**（`葉(は)` `巣(す)` `絵(え)` `木(き)`）。
 *
 *   たき(滝)        → look at tree     (指した物: 木)
 *   てんじょう(天井) → look at grate    (指した物: 錠)
 *
 * ★**「打てない」より悪い。打てないなら断られるが、これは黙って別の命令になる。**
 * ★`{SAID}` では救えない —— 打った語を返す仕組みだが、その「打った物」自体が
 *   誤解釈されているので「木など、ここには見当たらない」と返る。
 *
 * 材料は `ruby`（本文に出る語 → 漢字ごとの読み）。**本文に出る語だけ**を試すので、
 * 実際に画面で見て打ちたくなる語に絞れる。
 *
 * ★これは gate ではなく**ベースライン**。直すには「短い語は前後が語境界のときだけ
 * 有効」という規則が要り、エンジンの中核に触る（`きにのぼる` を殺さずに、が条件）。
 * いまは**増えたら気づける**ようにしてある。
 *
 * 使い方: node test/run-swallow.js
 */
const fs = require('fs')
const path = require('path')
const { createCommander } = require('../src/command.js')

const A = (f) => path.join(__dirname, '..', 'assets', f)
const asset = JSON.parse(fs.readFileSync(A('zork1-cmd.json'), 'utf8'))
const cm = createCommander(asset)
const ruby = JSON.parse(fs.readFileSync(A('zork1-ja.json'), 'utf8')).ruby

// ★表記が違うだけで**同じ読み**なら食い違いではない（`燐寸(まっち)` → マッチ /
//   `船(ふね)` → 舟）。指した物の読みを語彙から引いて突き合わせる
const hira = (s) => s.replace(/[\u30a1-\u30f6]/g, (c) => String.fromCharCode(c.charCodeAt(0) - 0x60))
const yomiOf = {}
const put = (form, yomi) => (yomiOf[form] ||= []).push(hira(form), ...(yomi || []))
for (const o of Object.values(asset.objects)) for (const w of o.words || []) put(w.form, w.yomi)
for (const h of asset.hypernyms || []) put(h.form, h.yomi)   // 舟・箱・扉…は上位語の側にある

// ★既知の 15 件（2026-08-27 の実測）。表記違いで正しく当たるもの（燐寸→マッチ /
//   船→舟）は食い違いではないので入れない
const KNOWN = new Set(['儀式', '地下迷宮', '城壁', '大理石', '天井', '完璧', '岩壁', '斬撃',
                       '水蒸気', '滝', '象嵌', '赤子', '迷宮', '過剰王', '電池式'])

const found = []
for (const [w, segs] of Object.entries(ruby)) {
  const yomi = segs.map((s) => (Array.isArray(s) ? (s[1] || s[0]) : s)).join('')
  if (!yomi) continue
  const r = cm.toCommand(yomi + 'をみる')
  if (!r.command) continue                       // 打てない = 断られる。こちらは害が小さい
  const said = r.said || ''
  if (!said || said === w || said.includes(w) || w.includes(said)) continue
  if ((yomiOf[said] || []).includes(yomi)) continue          // 同じ読みの言い換え
  found.push([w, yomi, r.command, said])
}

const now = new Set(found.map((f) => f[0]))
const added = [...now].filter((w) => !KNOWN.has(w))
const fixed = [...KNOWN].filter((w) => !now.has(w))

for (const [w, y, c, s] of found) {
  console.log(`${KNOWN.has(w) ? ' ' : '★'} ${w}(${y})`.padEnd(22, '　') + ` → ${c.padEnd(24)} (指した物: ${s})`)
}
console.log(`--- 語の中を食われた: ${found.length} 件（既知 ${KNOWN.size}）---`)
if (fixed.length) console.log(`✓ 直った: ${fixed.join(' ')}　← KNOWN から外すこと`)
if (added.length) {
  console.log(`★増えた: ${added.join(' ')}`)
  process.exit(1)
}
