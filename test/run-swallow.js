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
 * ★**2026-08-31 に 0 件になった。** 直し方は「語境界の規則」ではなく
 * **コマンド不適語の宣言**（原簿 `zork1-cmd-ja.md` の `## コマンド不適語`）——
 * 本文に出るが原作の語彙に無い語を同じ表に載せ、**最長一致で先に当てて**
 * 「その語は知らない」と返す。★原作と同じ答えになる
 * （原作に `look at ceiling` と打つと「`ceiling` という言葉は知らない」）。
 * ★原作が知っている語（`marble` / `glass`）はとぼけずに**物の名前へ足した**
 * —— とぼけるとバグを仕様として固めてしまう。
 *
 * ★だからこれは**もう門**。1 件でも出たら赤にする。
 * ★材料はルビ表だけでなく**カタカナ語も見る** —— `ガラス` `ガス` が `巣(す)` に
 * 食われていたのは、ルビ表しか見ていなかったから見つからなかった。
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

// ★空。**1 件でも出たら赤**（2026-08-31 に 15 件すべて片付いた）。
//   ここに足すのは「直せないと分かっているもの」だけにすること。
const KNOWN = new Set([])

// ★打てる語 = ルビの出ている漢字語 ＋ 本文のカタカナ語。
//   （ひらがなの連続も試したが、切り出せるのは「では」「これは」のような助詞の断片で、
//     名詞が一つも取れなかったので材料にしない）
const kata = (s) => s.replace(/[\u30a1-\u30f6]/g, (c) => String.fromCharCode(c.charCodeAt(0) - 0x60))
const body = []
const walk = (v) => {
  if (typeof v === 'string') body.push(v)
  else if (Array.isArray(v)) v.forEach(walk)
  else if (v && typeof v === 'object') Object.values(v).forEach(walk)
}
const jaAsset = JSON.parse(fs.readFileSync(A('zork1-ja.json'), 'utf8'))
for (const k of ['exact', 'assembled', 'templates', 'props']) walk(jaAsset[k])
const cands = Object.entries(ruby).map(([w, segs]) =>
  [w, segs.map((s) => (Array.isArray(s) ? (s[1] || s[0]) : s)).join('')])
for (const w of new Set(body.join('\n').match(/[\u30a1-\u30f6][\u30a1-\u30f6\u30fc]{1,9}/g) || [])) {
  cands.push([w, kata(w)])
}

const found = []
for (const [w, yomi] of cands) {
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
console.log(`試した語: ${cands.length}（ルビ表 ${Object.keys(ruby).length} + カタカナ ${cands.length - Object.keys(ruby).length}）`)

for (const [w, y, c, s] of found) {
  console.log(`${KNOWN.has(w) ? ' ' : '★'} ${w}(${y})`.padEnd(22, '　') + ` → ${c.padEnd(24)} (指した物: ${s})`)
}
console.log(`--- 語の中を食われた: ${found.length} 件（既知 ${KNOWN.size}）---`)
if (fixed.length) console.log(`✓ 直った: ${fixed.join(' ')}　← KNOWN から外すこと`)
if (added.length) {
  console.log(`★増えた: ${added.join(' ')}`)
  process.exit(1)
}
