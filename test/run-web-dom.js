'use strict'
/**
 * ブラウザ用ホスト（web/main.js）を node で走らせる検証。
 *
 * 最小の DOM と fetch を用意して、**本番と同じコード**を通す。
 * ブラウザを開けない環境でも、配線の壊れ（要素 id の取り違え・イベントの繋ぎ忘れ・
 * 段落の組み立て）はここで落ちる。描画の見た目だけは目で見るしかない。
 *
 * 使い方: node test/run-web-dom.js "north" "east" ...
 */
const fs = require('fs')
const path = require('path')
const vm2 = require('vm')

const ROOT = process.env.ZENMAI_ROOT ? path.resolve(process.env.ZENMAI_ROOT) : path.join(__dirname, '..')
process.on('unhandledRejection', (e) => { console.error('★未処理の拒否:', (e && e.stack) || e) })
const cmds = process.argv.slice(2)

// --- 最小の DOM ---
class El {
  constructor(tag) {
    this.tag = tag; this.children = []; this._text = ''
    this.className = ''; this.value = ''; this.disabled = false; this.placeholder = ''
    this.scrollTop = 0; this.scrollHeight = 0; this.handlers = {}
  }
  get textContent() { return this._text }
  set textContent(v) { this._text = String(v); this._html = null }
  // ★ふりがなは innerHTML で入る。テストは字面を見たいのでタグを剥がして持つ
  get innerHTML() { return this._html == null ? this._text : this._html }
  set innerHTML(v) {
    this._html = String(v)
    this._text = this._html.replace(/<rt>.*?<\/rt>/g, '').replace(/<[^>]*>/g, '')
      .replace(/&lt;/g, '<').replace(/&gt;/g, '>').replace(/&amp;/g, '&')
  }
  appendChild(c) { this.children.push(c); return c }
  insertBefore(c, ref) { this.children.splice(this.children.indexOf(ref), 0, c); return c }
  remove() { for (const el of Object.values(els)) { const i = el.children.indexOf(this); if (i >= 0) el.children.splice(i, 1) } }
  get lastElementChild() { return this.children[this.children.length - 1] }
  addEventListener(ev, fn) { (this.handlers[ev] = this.handlers[ev] || []).push(fn) }
  // 入力欄のキャレット（ゲームパッドの挿入・削除に要る）
  setSelectionRange(a, b) { this.selectionStart = a; this.selectionEnd = b }
  setAttribute(k, v) { this[k] = v }
  getAttribute(k) { return this[k] }
  querySelectorAll() { return [] }
  querySelector(sel) { return this.children.find((c) => c.tag === sel) || null }
  insertBefore(c, ref) { this.children.splice(this.children.indexOf(ref), 0, c); return c }
  dispatch(ev, arg) { for (const fn of this.handlers[ev] || []) fn(arg) }
  focus() {}
}
const ids = ['status', 'place', 'score', 'screen', 'bar', 'input', 'hint', 'stat', 'ruby-btn', 'debug-btn', 'meta', 'pad', 'pad-btn', 'flick', 'flick-btn', 'intro', 'intro-ok']
const els = Object.fromEntries(ids.map((id) => [id, new El('div')]))
const document = { getElementById: (id) => els[id], createElement: (t) => new El(t) }
const window = {}
// ふりがなの入切は body の class と localStorage を使う
const store = {}
global.localStorage = { getItem: (k) => (k in store ? store[k] : null), setItem: (k, v) => { store[k] = String(v) } }
global.atob = (b64) => Buffer.from(b64, 'base64').toString('binary')
global.btoa = (bin) => Buffer.from(bin, 'binary').toString('base64')
document.body = {
  classList: {
    _s: new Set(),
    add(c) { this._s.add(c) },
    contains(c) { return this._s.has(c) },
    toggle(c, on) {
      const want = on === undefined ? !this._s.has(c) : !!on
      if (want) this._s.add(c); else this._s.delete(c)
      return want
    },
  },
}
document.querySelectorAll = () => []
document.createElement = (t) => new El(t)
global.window = window
global.document = document
global.fetch = async (url) => {
  const p = path.join(ROOT, WEB, url)
  if (!fs.existsSync(p)) return { ok: false }
  const buf = fs.readFileSync(p)
  return { ok: true, json: async () => JSON.parse(buf.toString('utf8')), arrayBuffer: async () => buf }
}

// --- 本番のスクリプトを順に読み込む ---
// ★一覧は index.html から読み取る。ここに書き写すと、ページに script を足したとき
//   テストだけ古くなる（実際に command.js を足した回で落ちた）
const WEB = fs.existsSync(path.join(ROOT, 'web', 'index.html')) ? 'web' : '.'
const html = fs.readFileSync(path.join(ROOT, WEB, 'index.html'), 'utf8')
const srcs = [...html.matchAll(/<script src="([^"]+)"><\/script>/g)].map((m) => m[1])
for (const rel of srcs) {
  if (rel === 'main.js') continue                    // main.js は DOM を用意してから
  const f = path.normalize(path.join(WEB, rel))
  vm2.runInThisContext(fs.readFileSync(path.join(ROOT, f), 'utf8'), { filename: f })
}
// ★UMD は `globalThis` に登録する。ブラウザでは window と同じものだが、
//   この器では window を別に作っているので、載せ替えてやる必要がある
for (const name of ['FlickEngine', 'GamepadEngine', 'ZenmaiRuby', 'ZenmaiTranslate', 'ZenmaiCommand', 'GlkShim']) {
  if (!window[name] && globalThis[name]) window[name] = globalThis[name]
}
// ★フリックは**本物の resolver** を使う（flickmap の中身ごと確かめられる）。
//   mount だけ差し替えて onOp を捕まえる
const flick = {}
store['zenmai-flick'] = 'on'
// ★ゲームパッドの配線を器のまま試す。実機が無くても**こちらが書いた部分**は確かめられる
const pad = {}
// node 22 の navigator は getter だけなので上書きする
Object.defineProperty(global, 'navigator', { value: { getGamepads: () => [] }, configurable: true })
window.GamepadEngine = {
  version: 'fake',
  start(o) { pad.onOp = o.onOp; pad.onState = o.onState; pad.tail = o.getComposingTail; return { stop() {}, setEnabled() {} } },
  mount() { return { update() {}, destroy() {} } },
}
if (!window.ZVM) window.ZVM = module.exports && module.exports.prototype ? module.exports : global.ZVM
if (window.FlickEngine) {
  const real = window.FlickEngine
  window.FlickEngine = {
    version: real.version,
    decodeFlickmap: (j) => real.decodeFlickmap(j),
    createResolver: (m, h) => real.createResolver(m, h),
    mount(el, map, opts) { flick.map = map; flick.opts = opts; return { destroy() {} } },
  }
}
vm2.runInThisContext(fs.readFileSync(path.join(ROOT, WEB, 'main.js'), 'utf8'), { filename: 'main.js' })

// --- コマンドを順に流す ---
let n = 0
const tick = () => {
  if (els.input.disabled) return setTimeout(tick, 30)          // まだ入力待ちではない
  if (n >= cmds.length) return finish()
  els.input.value = cmds[n++]
  els.input.dispatch('keydown', { key: 'Enter' })
  setTimeout(tick, 30)
}
const finish = () => {
  const text = els.screen.children.map((p) => (p.className === 'cmd' ? '> ' : '') + p.textContent).join('\n')
  console.log(text)
  console.log('\n--- 状態行: [' + els.place.textContent + '] ' + els.score.textContent)
  console.log('--- ' + els.stat.textContent.trim())
  const raw = els.screen.children.filter((p) => p.className === 'raw')
  console.log('--- 未訳として描かれた段落: ' + raw.length)
  // ★場所名の行が段落の頭として切り出されているか
  const rooms = els.screen.children.filter((p) => p.className === 'room')
  console.log('--- 場所名の段落: ' + rooms.length + ' 件　' + rooms.map((p) => p.textContent).join(' / '))
  if (cmds.length && !rooms.length) { console.error('★場所名が切り出されていない'); process.exit(1) }
  // ★ゲームパッドの配線（実機が無くても op から先は同じ道を通る）
  if (pad.onOp) {
    const inp = els.input
    pad.onState({ connected: true, activeRow: 1, activeLayer: 'base', previewChar: null, pressed: new Set(), axes: [] })
    const consonant = els['pad-btn'].textContent
    inp.value = ''; inp.selectionStart = 0
    for (const t of ['か', 'き', 'く']) { pad.onOp({ type: 'kana', text: t, replace: 0 }) }
    const typed = inp.value
    pad.onOp({ type: 'kana', text: '？', replace: 0 })           // 要らない記号は落とす
    const dropped = inp.value === typed
    // ★濁点は「合成末尾」を見て置換を決める。実機と同じ経路で確かめる
    const G = require('../vendor/gamepad-engine.js')
    const dak = G.resolveDakutenOp(pad.tail())
    if (dak) pad.onOp(dak)
    const dakuten = inp.value
    pad.onOp({ type: 'key', tap: { key: 'Backspace' } })
    const afterBs = inp.value
    const top0 = els.screen.scrollTop
    pad.onOp({ type: 'key', tap: { key: 'ArrowDown' } })
    const scrolled = els.screen.scrollTop > top0
    pad.onOp({ type: 'key', tap: { key: 'ArrowUp' } })
    const scrolledBack = els.screen.scrollTop === top0
    const before = els.screen.children.length
    pad.onOp({ type: 'kana', text: '、', replace: 0 })           // ★R🕹↓ は送信
    const sent = els.screen.children.length > before && inp.value === ''
    const before2 = els.screen.children.length
    pad.onOp({ type: 'kana', text: '、', replace: 0 })           // 空のまま連打しても送らない
    const noEmpty = els.screen.children.length === before2
    const checks = [
      ['子音の札が出る（か行）', consonant === 'か'],
      ['かなが入る', typed === 'かきく'],
      ['要らない記号を落とす', dropped],
      ['R🕹← で 1 字消える', afterBs === 'かき'],
      ['濁点が付く（末尾を見ている）', dakuten === 'かきぐ'],
      ['L🕹↓ で本文が送られる', scrolled],
      ['L🕹↑ で戻る', scrolledBack],
      ['R🕹↓ で送信される', sent],
      ['空のまま連打しても送らない', noEmpty],
    ]
    for (const [name, ok] of checks) console.log(`--- ゲームパッド ${ok ? '✓' : '✗'} ${name}`)
    if (checks.some(([, ok]) => !ok)) { console.error('★ゲームパッドの配線が壊れている'); process.exit(1) }
  }
  // ★版権表示と本文のあいだに線が入っているか
  const hrAt = els.screen.children.findIndex((c) => c.tag === 'hr')
  const roomAt = els.screen.children.findIndex((c) => c.className === 'room')
  console.log(`--- 区切り線: ${hrAt >= 0 && hrAt < roomAt ? '✓ 版権表示の後・最初の場所名の前' : '✗ 位置がおかしい'}`)
  if (!(hrAt >= 0 && hrAt < roomAt)) process.exit(1)
  // ★フリック（本物の flickmap + resolver を通す）
  if (!flick.opts) { console.error('★フリックが載っていない'); process.exit(1) }
  if (flick.opts) {
    const R = require('../vendor/flick-engine.js').createResolver(flick.map, {
      getComposingTail: flick.opts.getComposingTail,
    })
    const inp = els.input
    inp.value = ''; inp.selectionStart = 0
    const play = (row, col, dir) => {
      for (const op of R.resolve({ row, col, kind: dir ? 'flick' : 'tap', dir })) flick.opts.onOp(op)
    }
    play(1, 2)            // な
    play(1, 2, 'up')      // ぬ
    play(0, 2)            // か
    const typed = inp.value
    play(3, 1)            // ゛゜小 → が
    const dakuten = inp.value
    play(2, 2, 'left')    // や← は消してある
    const noParen = inp.value === dakuten
    play(0, 4)            // ⌫
    const afterBs = inp.value
    const before = els.screen.children.length
    play(3, 4)            // 送信（↵）
    const sent = els.screen.children.length > before && inp.value === ''
    const checks = [
      ['かなが入る', typed === 'なぬか'],
      ['゛゜小で濁点', dakuten === 'なぬが'],
      ['や の左右は無い（括弧を消した）', noParen],
      ['左端列に ← がある（標準の位置）', flick.map.layers.kana.keys.some((k) => k.row === 1 && k.col === 0)],
      ['⌫ で 1 字消える', afterBs === 'なぬ'],
      ['送信される', sent],
    ]
    for (const [name, ok] of checks) console.log(`--- フリック ${ok ? '✓' : '✗'} ${name}`)
    if (checks.some(([, ok]) => !ok)) { console.error('★フリックの配線が壊れている'); process.exit(1) }
  }
  // ★ふりがなが実際に振られているか（入力はかなだけなので、これは操作系）
  const html = els.screen.children.map((p) => p.innerHTML).join('')
  const rt = [...html.matchAll(/<ruby>([^<]*)<rt>([^<]*)<\/rt><\/ruby>/g)]
  console.log('--- ふりがな: ' + rt.length + ' 箇所　例: '
    + rt.slice(0, 6).map((m) => m[1] + '(' + m[2] + ')').join(' '))
  if (!rt.length) { console.error('★ふりがなが 1 つも振られていない'); process.exit(1) }
  process.exit(0)
}
setTimeout(tick, 200)
