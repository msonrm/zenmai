'use strict'
/**
 * ★**画面に出た語を、読みのまま打てるか**を見張る。「ほどうきょうをわたる」が「知らない言葉」になった
 * （Zork II・2026-10-06 の実プレイ）型の穴の門。
 *
 * 原因: 訳が英語の「形容詞 + 名詞」（foot bridge）を**1 語の複合語**（歩道橋）にすると、画面にはその語が
 * ふりがな付きで出るのに、入力語彙は核の名詞（橋）しか持たない。英語なら `foot` を落として `cross bridge`
 * と打てるが、日本語は語の切れ目が無いので落とせず、「ほどうきょう」が丸ごと知らない言葉になる。
 * （「石橋」「球」「岩」「若い女」…も同じ。物の語彙・上位語に足して直した）
 *
 * `run-swallow.js`（打つと**別の物**になる）とは別の穴: こちらは**打つと断られる**。害は小さいが、
 * 画面に出した語は打てなければならない（Zenmai の軸）。
 *
 * 材料は `ruby`（本文に出る難読語）。★物の語彙の語（橋・犬・水…）を**含む**が語彙そのものではない語を、
 * かなと漢字の両方で「〜を見る」に打ち、**知らない語が残る**ものを挙げる。
 * 直し方は 2 つ（原簿 `<作品>-cmd-ja.md`）:
 *   ① 原作がその物を知っているなら → **物の名前**か**上位語**へ足す（歩道橋・球・岩・女）
 *   ② 原作も知らない語なら → **コマンド不適語**へ足す（岩棚・縦穴）。残った断片（「だな」）ではなく
 *      語ぜんたいを名指しして断る（原作の `I don't know the word` と同じ答え）
 *
 * ★限界: 「**原作が知っているのに、日本語の呼び名が無い**語」（訳文は「球」と書くが語彙は「水晶球」だけ、
 *   など）は、語彙を含む複合語の形をしていないのでここでは拾えない。それは原作の SYNONYM と訳文の突き合わせ
 *   （`zork1_typable_check.py`・原簿側）と実プレイで見つける。
 * ★baseline（`test/visible-known-<作品>.txt`）は「物語の文の断片で、打たれない」ものだけ。増えたら赤。
 *
 * 使い方: WORK=zork2 node test/run-visible.js
 */
const fs = require('fs')
const path = require('path')
const { createCommander } = require('../src/command.js')

const A = (f) => path.join(__dirname, '..', 'assets', f)
const WORK = process.env.WORK || 'zork1'
const cmdAsset = JSON.parse(fs.readFileSync(A(`${WORK}-cmd.json`), 'utf8'))
const cm = createCommander(cmdAsset)
const ruby = JSON.parse(fs.readFileSync(A(`${WORK}-ja.json`), 'utf8')).ruby

const forms = new Set()
for (const o of Object.values(cmdAsset.objects)) for (const w of o.words || []) forms.add(w.form)
for (const h of cmdAsset.hypernyms || []) forms.add(h.form)
const kanji = /[一-龥]/
const nouns = [...forms].filter((f) => kanji.test(f))

const knownFile = path.join(__dirname, `visible-known-${WORK}.txt`)
const KNOWN = new Set(fs.existsSync(knownFile)
  ? fs.readFileSync(knownFile, 'utf8').split('\n').map((l) => l.replace(/#.*/, '').trim()).filter(Boolean) : [])

const bad = []
for (const [w, segs] of Object.entries(ruby)) {
  const yomi = segs.map((s) => (Array.isArray(s) ? (s[1] || s[0]) : s)).join('')
  if (forms.has(w) || !nouns.some((n) => w.includes(n))) continue
  for (const [how, t] of [['かな', yomi], ['漢字', w]]) {
    const r = cm.toCommand(t + 'をみる')
    if (r.trace === 'コマンド不適語') continue          // 語ぜんたいを名指しして断っている = 正しい
    const unk = r.unknown || []
    if (r.command && !unk.length) continue
    bad.push([w, yomi, how, unk.join('/') || '(読めない)'])
    break
  }
}

const fresh = bad.filter(([w]) => !KNOWN.has(w))
const stale = [...KNOWN].filter((w) => !bad.some(([b]) => b === w))
for (const [w, y, how, u] of fresh) console.log(`✗ ${w}（${y}）— ${how}で打つと「${u}」が残る`)
for (const w of stale) console.log(`△ baseline に残っているが、もう打てる/名指しされる: ${w}（visible-known から消してよい）`)
console.log(`--- ${WORK}: 語彙の語を含む難読語 ${Object.keys(ruby).filter((w) => !forms.has(w) && nouns.some((n) => w.includes(n))).length} 語のうち、打てない ${bad.length}（baseline ${KNOWN.size}・新規 ${fresh.length}）---`)
process.exit(fresh.length ? 1 : 0)
