'use strict'
/**
 * 検査が相手にする**作品**の口（1 か所）。作品名を各検査に散らさない。
 *
 *   WORK=zork2 node test/run-cmd.js      ← 環境変数 WORK（既定 zork1）
 *   npm test                             ← 全作品をまとめて流す（test/run-all.js）
 *
 * ★作品ごとに違うもの（固定表・開始の部屋・通しの手の列）は `test/works/<作品>.js` に置く。
 *   検査の本体（`run-*.js`）には作品名を書かない —— Zork III を足すときに触るのは
 *   `assets/zork3-*.json`・`web/works.json`・`test/works/zork3.js` だけ、にする（2026-11-14）
 * ★`ZENMAI_WORK` は `run-web-dom.js` の昔の名前。文書に残っているので受け続ける
 */
const fs = require('fs')
const path = require('path')

const ROOT = path.join(__dirname, '..')
const WORK = process.env.WORK || process.env.ZENMAI_WORK || 'zork1'
const asset = (work, kind) => path.join(ROOT, 'assets', `${work}-${kind}.json`)

/** 日本語の資産（語彙）がある作品の一覧。`npm test` が回す相手 */
const works = () => fs.readdirSync(path.join(ROOT, 'assets'))
  .map((f) => (f.match(/^(.+)-cmd\.json$/) || [])[1])
  .filter((w) => w && fs.existsSync(asset(w, 'ja')))
  .sort()

if (!fs.existsSync(asset(WORK, 'cmd'))) {
  console.error(`作品「${WORK}」の語彙が無い: ${asset(WORK, 'cmd')}\n（ある作品: ${works().join(' ')}）`)
  process.exit(2)
}

const read = (p) => JSON.parse(fs.readFileSync(p, 'utf8'))
const storyPath = path.join(ROOT, 'vendor', WORK, `${WORK}.z3`)
const specPath = path.join(__dirname, 'works', `${WORK}.js`)

module.exports = {
  WORK,
  ROOT,
  works,
  /** 語彙（`assets/<作品>-cmd.json`）。呼ぶたびに読み直す = 検査が書き足しても他へ漏れない */
  cmdAsset: () => read(asset(WORK, 'cmd')),
  /** 訳（`assets/<作品>-ja.json`） */
  jaAsset: () => read(asset(WORK, 'ja')),
  storyPath,
  story: () => fs.readFileSync(storyPath),
  /** 題など（`web/works.json` の 1 件）。載っていなければ id だけ */
  info: () => (read(path.join(ROOT, 'web', 'works.json')).works || []).find((w) => w.id === WORK) || { id: WORK, title: WORK },
  /** 作品ごとの検査の材料（`test/works/<作品>.js`）。無ければ空 = 作品によらない検査だけが流れる */
  spec: () => (fs.existsSync(specPath) ? require(specPath) : {}),
}
