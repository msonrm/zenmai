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

const ROOT = path.join(__dirname, '..')
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
  get lastElementChild() { return this.children[this.children.length - 1] }
  addEventListener(ev, fn) { (this.handlers[ev] = this.handlers[ev] || []).push(fn) }
  dispatch(ev, arg) { for (const fn of this.handlers[ev] || []) fn(arg) }
  focus() {}
}
const ids = ['status', 'place', 'score', 'screen', 'bar', 'input', 'hint', 'stat', 'ruby-btn']
const els = Object.fromEntries(ids.map((id) => [id, new El('div')]))
const document = { getElementById: (id) => els[id], createElement: (t) => new El(t) }
const window = {}
// ふりがなの入切は body の class と localStorage を使う
const store = {}
global.localStorage = { getItem: (k) => (k in store ? store[k] : null), setItem: (k, v) => { store[k] = String(v) } }
document.body = { classList: { _s: new Set(), add(c) { this._s.add(c) }, toggle(c) { this._s.has(c) ? this._s.delete(c) : this._s.add(c); return this._s.has(c) } } }
global.window = window
global.document = document
global.fetch = async (url) => {
  const p = path.join(ROOT, 'web', url)
  if (!fs.existsSync(p)) return { ok: false }
  const buf = fs.readFileSync(p)
  return { ok: true, json: async () => JSON.parse(buf.toString('utf8')), arrayBuffer: async () => buf }
}

// --- 本番のスクリプトを順に読み込む ---
// ★一覧は index.html から読み取る。ここに書き写すと、ページに script を足したとき
//   テストだけ古くなる（実際に command.js を足した回で落ちた）
const html = fs.readFileSync(path.join(ROOT, 'web', 'index.html'), 'utf8')
const srcs = [...html.matchAll(/<script src="([^"]+)"><\/script>/g)].map((m) => m[1])
for (const rel of srcs) {
  if (rel === 'main.js') continue                    // main.js は DOM を用意してから
  const f = path.normalize(path.join('web', rel))
  vm2.runInThisContext(fs.readFileSync(path.join(ROOT, f), 'utf8'), { filename: f })
}
if (!window.ZVM) window.ZVM = module.exports && module.exports.prototype ? module.exports : global.ZVM
vm2.runInThisContext(fs.readFileSync(path.join(ROOT, 'web', 'main.js'), 'utf8'), { filename: 'web/main.js' })

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
  // ★ふりがなが実際に振られているか（入力はかなだけなので、これは操作系）
  const html = els.screen.children.map((p) => p.innerHTML).join('')
  const rt = [...html.matchAll(/<ruby>([^<]*)<rt>([^<]*)<\/rt><\/ruby>/g)]
  console.log('--- ふりがな: ' + rt.length + ' 箇所　例: '
    + rt.slice(0, 6).map((m) => m[1] + '(' + m[2] + ')').join(' '))
  if (!rt.length) { console.error('★ふりがなが 1 つも振られていない'); process.exit(1) }
  process.exit(0)
}
setTimeout(tick, 200)
