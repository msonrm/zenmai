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
    this.checked = false; this.hidden = false
    // ★操作図はホストが自前で描く（クラスの付け外しで押下を見せる）ので、器にも classList が要る
    this._cls = new Set()
    this.classList = {
      _s: this._cls,
      add(c) { this._s.add(c) },
      remove(c) { this._s.delete(c) },
      contains(c) { return this._s.has(c) },
      toggle(c, on) {
        const want = on === undefined ? !this._s.has(c) : !!on
        if (want) this._s.add(c); else this._s.delete(c)
        return want
      },
    }
  }
  get textContent() { return this._text }
  // ★本物の `textContent =` は**子要素を消す**。消さないと、札を書き替えるたびに
  //   span が積み上がって器だけが壊れる（2 段の札を入れたときに実際に起きた）
  set textContent(v) { this._text = String(v); this._html = null; this.children.length = 0 }
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
// ★id の一覧も**index.html から読み取る**。書き写していたので、ページに要素を足すたび
//   ここが古くなった（script の一覧は自動なのに、id だけ手作業という非対称が残っていた）
const WEB = fs.existsSync(path.join(ROOT, 'web', 'index.html')) ? 'web' : '.'
const html = fs.readFileSync(path.join(ROOT, WEB, 'index.html'), 'utf8')
const ids = [...new Set([...html.matchAll(/\bid="([^"]+)"/g)].map((m) => m[1]))]
const els = Object.fromEntries(ids.map((id) => [id, new El('div')]))
// ★`hidden` 付きで始まる要素は器でも隠しておく —— 器のほうが「開いた状態」で始まっていると、
//   開閉のトグルが本番と逆に動き、**器だけ通る**（本番より優しい器は本番でだけ壊れる）
for (const m of html.matchAll(/<div id="([^"]+)"[^>]*\shidden\b/g)) if (els[m[1]]) els[m[1]].hidden = true
const document = { getElementById: (id) => els[id], createElement: (t) => new El(t) }
const window = { innerWidth: 1024, addEventListener() {} }
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
// ★手引きは Escape でも閉じる（document で受ける）
document.handlers = {}
document.addEventListener = (ev, fn) => { (document.handlers[ev] = document.handlers[ev] || []).push(fn) }
global.window = window
global.document = document
global.fetch = async (url) => {
  const p = path.join(ROOT, WEB, url)
  // ★Cloudflare Pages は**無いパスにも index.html を 200 で返す**。器も同じ意地悪をして、
  //   ホストが中身を確かめているかを試す（これを入れる前は本番だけで壊れた）
  if (!fs.existsSync(p)) {
    const fallback = fs.readFileSync(path.join(ROOT, WEB, 'index.html'))
    return { ok: true, json: async () => JSON.parse(fallback.toString('utf8')), arrayBuffer: async () => fallback }
  }
  const buf = fs.readFileSync(p)
  return { ok: true, json: async () => JSON.parse(buf.toString('utf8')), arrayBuffer: async () => buf }
}

// --- 本番のスクリプトを順に読み込む ---
// ★一覧は index.html から読み取る。ここに書き写すと、ページに script を足したとき
//   テストだけ古くなる（実際に command.js を足した回で落ちた）
// ★`?v=<中身のハッシュ>` は**ブラウザのキャッシュ破棄**のために build.mjs が付ける。
//   ファイルを読むときは剥がす（付けた回に dist の検査だけが落ちた）
const srcs = [...html.matchAll(/<script src="([^"]+)"><\/script>/g)].map((m) => m[1].split('?')[0])
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
// ★**本物のエンジンを土台にして**、rAF を回す start と DOM を作る mount だけ差し替える。
//   丸ごと偽物にすると、入力表のような「本物にしかないもの」が器から消えて、
//   **器だけが落ちる**（v1.8.0 で表が公開されたときに実際に落ちた）
const realGP = require('../vendor/gamepad-engine.js')
window.GamepadEngine = {
  ...realGP,
  start(o) {
    pad.onOp = o.onOp; pad.onState = o.onState; pad.tail = o.getComposingTail
    let lang = o.lang || 'japanese'
    pad.ctl = { stop() {}, setEnabled() {}, setLang(l) { lang = l }, get lang() { return lang } }
    return pad.ctl
  },
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
// ★案内を閉じる前に本文が出ていないか（言語を選ぶ前に冒頭が印字されると手遅れになる）
let introGate = false
const tick = () => {
  if (els.input.disabled) return setTimeout(tick, 30)          // まだ入力待ちではない
  if (n >= cmds.length) return finish()
  els.input.value = cmds[n++]
  els.input.dispatch('keydown', { key: 'Enter' })
  setTimeout(tick, 30)
}
const finish = async () => {
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
    // ★日本語の R🕹↓（、）は**何もしない**。誤送信のもとだったので送信から外した。
    //   送るのは LS 押し込み（Enter）だけ —— 遅延確定も `autoConfirm: false` で止めてある
    inp.value = 'きた'
    inp.selectionStart = 2
    const before = els.screen.children.length
    pad.onOp({ type: 'kana', text: '、', replace: 0 })
    const punctNoop = inp.value === 'きた' && els.screen.children.length === before
    pad.onOp({ type: 'key', tap: { key: 'Enter' } })            // LS 押し込み＝送信
    const sent = els.screen.children.length > before && inp.value === ''
    const before2 = els.screen.children.length
    pad.onOp({ type: 'key', tap: { key: 'Enter' } })            // 空のまま連打しても送らない
    const noEmpty = els.screen.children.length === before2
    // ★操作図（自前）が組み立ち、押下が映るか。★labo のビジュアライザをやめたので、
    //   ここが壊れても実機まで気づけない —— 器で押さえる
    const padRoot = els.pad
    const allBtns = []
    const walk = (n) => { for (const c of n.children || []) { allBtns.push(c); walk(c) } }
    walk(padRoot)
    const btnCount = allBtns.filter((e) => e.className && /\bgp-btn\b|\bgp-trigger\b/.test(e.className)).length
    const isOn = (pred) => allBtns.some((e) => e.classList.contains('on') && pred(e))
    pad.onState({ connected: true, activeRow: 0, activeLayer: 'base', previewChar: null, pressed: new Set([6]), axes: [] })
    const ltOn = isOn((e) => /gp-trigger/.test(e.className))
    const centerOnIdle = allBtns.some((e) => /gp-center/.test(e.className) && e.classList.contains('on'))
    pad.onState({ connected: true, activeRow: 0, activeLayer: 'base', previewChar: null, pressed: new Set([12]), axes: [] })
    const centerOffWhenDir = !allBtns.some((e) => /gp-center/.test(e.className) && e.classList.contains('on'))
    // ★図に出る字と、エンジンが実際に入れる字が一致するか。
    //   かな表はホスト（web/main.js）が**写して持っている** —— UMD が公開していないため。
    //   ★ずれると「図に出ている字と、押して入る字が違う」という最悪の事故になるので、
    //   vendor のソースから表を取り出して、**実際に描かれた札**と突き合わせる
    let vsrc = null
    for (const p of ['../vendor/gamepad-engine.js', 'vendor/gamepad-engine.js']) {
      try {
        const f = path.join(ROOT, WEB, p)
        if (fs.existsSync(f)) { vsrc = fs.readFileSync(f, 'utf8'); break }
      } catch (e) { /* 次 */ }
    }
    const flat = (vsrc || '').replace(/\s+/g, '')
    // この企画で落としている記号（や行の「」・わ行の ？）は、図でも空になっていなければならない
    const DROPPED = new Set(['「', '」', '？', '、', '。'])
    const VOWEL_BTNS = [5, 2, 3, 1, 0]   // RB, 左(X), 上(Y), 右(B), 下(A)
    const rowLabels = (r, extra) => {
      pad.onState(Object.assign({ connected: true, activeRow: r, activeLayer: 'base', previewChar: null, pressed: new Set(), axes: [] }, extra))
      return VOWEL_BTNS.map((n) => {
        const e = allBtns.find((x) => x._btn === n)
        return e ? (e.textContent || '') : '(無し)'
      })
    }
    const drift = []
    for (let r = 0; r < 10; r++) {
      const shown = rowLabels(r)
      // vendor のソースに、その行がそのまま入っているか（整形に依存しないよう空白を潰して探す）
      const want = shown.map((c, i) => c || null)
      // 図に出ている字は、必ず vendor の表にある並びの一部でなければならない
      for (let i = 0; i < 5; i++) {
        const c = shown[i]
        if (!c) continue
        if (!flat.includes(`"${c}"`)) drift.push(`行${r} 列${i}: 「${c}」が vendor の表に無い`)
      }
      void want
    }
    // 落とす記号が図に残っていないか
    const leftover = []
    for (let r = 0; r < 10; r++) for (const c of rowLabels(r)) if (DROPPED.has(c)) leftover.push(c)
    // 代表例（か行）が正しく並ぶか —— 表の**順序**（RB=あ段, 左=い段, …）まで見る
    const ka = rowLabels(1).join('')
    const wa = rowLabels(9).join('')

    // ★かな / 英字の切替。★**札だけ変えても入る字は変わらない**ので、
    //   エンジン側の言語も切り替わっているかを見る
    const langBtns = allBtns.filter((e) => /\bgp-lang\b/.test(e.className || ''))
    const langOf = (code) => langBtns.find((b) => b.textContent === code)
    // ★選んでいるほうに `on` が付く（枠線）
    
    if (langOf('EN')) langOf('EN').dispatch('click')
    const engLang = pad.ctl ? pad.ctl.lang : null
    const enLabels = rowLabels(1)               // 英語の行 1 = 2abc
    const enDpad = (() => {
      pad.onState({ connected: true, activeRow: 0, activeLayer: 'base', previewChar: null, pressed: new Set(), axes: [] })
      const e = allBtns.find((x) => x._btn === 14)   // ←
      // 2 段（数字＋英字）は子要素に入る
      return (e && e.children.length ? e.children.map((c) => c.textContent).join('|') : (e ? e.textContent : ''))
    })()
    // ★肩（LT / LB / RT）も言語で変わるか —— buildPad で 1 度書くだけだと日本語のまま残る
    const shoulderOf = () => [6, 4, 7].map((n) => {
      const e = allBtns.find((x) => x._btn === n)
      return e ? e.textContent : ''
    }).join(' ')
    const enShoulders = shoulderOf()
    // ★シフトの 3 段。札の字面（shift → Shift → CAPS）と、英字の大小が状態に追随するか
    const ltOf = () => { const e = allBtns.find((x) => x._btn === 6); return e ? e.textContent : '' }
    rowLabels(1); const ltIdle = ltOf(); const enLower = rowLabels(1).join('')
    rowLabels(1, { englishShiftNext: true }); const ltShift = ltOf()
    const enShifted = rowLabels(1, { englishShiftNext: true }).join('')
    rowLabels(1, { englishCapsLock: true }); const ltCaps = ltOf()
    const enCaps = rowLabels(1, { englishCapsLock: true }).join('')
    // ★英語では R🕹↓ は**空白**（単語の区切りに要る）。送信は LS 押し込み（Enter）
    els.input.value = 'take'
    els.input.selectionStart = 4
    const scrBefore = els.screen.children.length
    pad.onOp({ type: 'kana', text: ' ', replace: 0 })
    const enSpace = els.input.value === 'take ' && els.screen.children.length === scrBefore
    pad.onOp({ type: 'key', tap: { key: 'Enter' } })
    const enSend = els.input.value === '' && els.screen.children.length > scrBefore
    // ★コマンド欄の隣の札。英語は 2 段（数字＋英字）、日本語は 1 段
    rowLabels(1)
    const btnEn = els['pad-btn'].children.length
      ? els['pad-btn'].children.map((c) => c.textContent).join('|') : els['pad-btn'].textContent
    if (langOf('JA')) langOf('JA').dispatch('click')      // 日本語へ戻す
    const backToKana = rowLabels(1).join('') === 'かきくけこ'
    const jaShoulders = shoulderOf()

    // ★本文の言語と、コントローラの自動追随
    const padLabel = () => ((langBtns.find((b) => b.classList.contains('on')) || {}).textContent || '')
    // ★本文を日本語で始めたので、コントローラも かな で始まる
    const padFollowsBody = padLabel() === 'JA'

    // ★使い方の「?」が置かれ、押すと開くか
    const helpBtn = allBtns.find((e) => /gp-help/.test(e.className || ''))
    if (helpBtn) helpBtn.dispatch('click')
    const helpOpen = els['pad-help'].hidden === false
    els['pad-help-close'].dispatch('click')
    const helpClosed = els['pad-help'].hidden === true
    // ★使い方の中身がモードに追随するか（盤に出ているものと食い違わせない）
    const helpJa = els['help-ja'].hidden === false && els['help-en'].hidden === true
    if (langOf('EN')) langOf('EN').dispatch('click')   // 英語へ
    if (helpBtn) helpBtn.dispatch('click')
    const helpEn = els['help-en'].hidden === false && els['help-ja'].hidden === true
    els['pad-help-close'].dispatch('click')
    if (langOf('JA')) langOf('JA').dispatch('click')   // 日本語へ戻す
    const checks = [
      // ★十字 5（中央込み）+ フェイス 4 + 空 8 + 肩 4 = 21
      ['操作図が組み立つ', btnCount >= 20],
      ['使い方の「?」がある', !!helpBtn],
      ['「?」で使い方が開く', helpOpen],
      ['使い方を閉じられる', helpClosed],
      ['★使い方の中身がモードに追随する（かな / 英字）', helpJa && helpEn],
      ['★案内を閉じるまで本文は始まらない（言語を選べる）', introGate],
      ['★コントローラは本文と同じ言語で始まる', padFollowsBody],
      ['★図の字がエンジンの表と一致する', !!vsrc && drift.length === 0],
      ['JA / EN の 2 つが並ぶ', langBtns.length === 2],
      ['★選んでいるほうに印が付く', padLabel() === 'JA'],
      ['★切替でエンジン側の言語も変わる', engLang === 'english'],
      ['英語では母音ボタンが英字になる', enLabels.join('') === '2abc'],
      ['十字はプッシュホン式（数字＋英字の 2 段）', enDpad === '2|abc'],
      ['戻すと かな に復帰する', backToKana],
      ['★肩の札も英語になる（LT / LB / RT）', enShoulders === 'shift 6〜0 0'],
      ['★LT の札が状態を示す（shift → Shift → CAPS）', ltIdle === 'shift' && ltShift === 'Shift' && ltCaps === 'CAPS'],
      ['シフト中は図の英字も大文字になる', enLower === '2abc' && enShifted === '2ABC' && enCaps === '2ABC'],
      ['★コマンド欄の隣の札も英語では 2 段（数字＋英字）', btnEn === '2|abc'],
      ['★英語では R🕹↓ は空白（送信ではない）', enSpace],
      ['英語の送信は LS 押し込み', enSend],
      ['戻すと肩の札も かな に戻る', jaShoulders === '拗/小 は〜わ ん/を'],
      ['落とす記号は図でも空', leftover.length === 0],
      ['母音の並びが RB=あ段/左=い段/上=う段/右=え段/下=お段', ka === 'かきくけこ'],
      ['わ行は落とす記号を抜いて並ぶ', wa === 'わゐゑを'],
      ['肩ボタンの押下が映る（LT）', ltOn],
      ['どれも押していないと十字の中央が点く', centerOnIdle],
      ['方角を押すと中央が消える', centerOffWhenDir],
      ['子音の札が出る（か行）', consonant === 'か'],
      ['かなが入る', typed === 'かきく'],
      ['要らない記号を落とす', dropped],
      ['R🕹← で 1 字消える', afterBs === 'かき'],
      ['濁点が付く（末尾を見ている）', dakuten === 'かきぐ'],
      ['L🕹↓ で本文が送られる', scrolled],
      ['L🕹↑ で戻る', scrolledBack],
      ['★R🕹↓（、）では送らない（誤送信を断つ）', punctNoop],
      ['LS 押し込みで送信される', sent],
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

  // ★設定と手引き（ヘッダの歯車）
  els['menu-btn'].dispatch('click')
  const opened = els.panel.hidden === false
  const rows = els['sys-list'].children
  const words = rows.map((r) => r.children[1].textContent.split('/')).flat().map((s) => s.trim())
  // ★案内した言葉が**実際に打てるか**を確かめる ——
  //   ここが「画面に出した語は、打てなければならない」の機械化。
  //   手引きに言葉を書き写すと必ずずれるので、器のほうで塞ぐ
  let cmdAsset = null
  for (const p of ['../assets/zork1-cmd.json', 'assets/zork1-cmd.json']) {
    // ★器は無いパスにも index.html を返す（本番と同じ意地悪）ので、中身で見分ける
    try { const j = await (await fetch(p)).json(); if (j && j.verbs) { cmdAsset = j; break } } catch (e) { /* 次 */ }
  }
  const cm2 = cmdAsset ? window.ZenmaiCommand.createCommander(cmdAsset) : null
  const cannot = cm2 ? words.filter((w) => !cm2.toCommand(w).command) : ['(アセットが読めない)']
  // ★かなだけで書かれているか（漢字の形を案内しても打てない）
  const notKana = words.filter((w) => !/^[ぁ-んァ-ヶーa-z]+$/.test(w))
  // ふりがなの入切
  els['ruby-chk'].checked = false; els['ruby-chk'].dispatch('change')
  const rubyOff = document.body.classList.contains('no-ruby')
  els['ruby-chk'].checked = true; els['ruby-chk'].dispatch('change')
  const rubyOn = !document.body.classList.contains('no-ruby')
  // Escape で閉じる
  for (const fn of document.handlers.keydown || []) fn({ key: 'Escape' })
  const closed = els.panel.hidden === true
  // ★題を押すと入口の案内が戻る（2 度目は「とじる」）
  // ★遊び始めたあとに題から開き直したときは、**言語ボタンを出さない**
  //   （途中で言語は変えられないので、選ばせると嘘になる）。代わりに「とじる」だけ
  els.intro.hidden = true
  els.title.dispatch('click')
  const introBack = els.intro.hidden === false
    && els['intro-lang'].hidden === true && els['intro-ok'].hidden === false
  els['intro-ok'].dispatch('click')
  const introClosed = els.intro.hidden === true
  const pchecks = [
    ['★題から開き直すと言語ボタンは出ない（とじるだけ）', introBack],
    ['案内を閉じられる', introClosed],
    ['歯車で開く', opened],
    ['Escape で閉じる', closed],
    ['システムの言葉が並ぶ', rows.length >= 10],
    // ★`words` が空だと「打てない語は 0 件」で**素通りしてしまう**（弱い assert）。
    //   件数の下限を同じ行に入れて、空振りを緑にしない
    ['★案内した言葉がすべて打てる', words.length >= 12 && cannot.length === 0],
    ['案内はかなの形だけ', words.length >= 12 && notKana.length === 0],
    ['ふりがなを切れる', rubyOff],
    ['ふりがなを戻せる', rubyOn],
    ['成績が出ている', /引けた \d+ 行/.test(els.stat.textContent)],
  ]
  for (const [name, ok] of pchecks) console.log(`--- 手引き ${ok ? '✓' : '✗'} ${name}`)
  if (cannot.length) console.error('  打てない言葉: ' + cannot.join(' / '))
  if (notKana.length) console.error('  かなでない案内: ' + notKana.join(' / '))
  if (pchecks.some(([, ok]) => !ok)) { console.error('★手引きの配線が壊れている'); process.exit(1) }
  console.log('--- 手引きの項目: ' + rows.length + ' 件　'
    + rows.slice(0, 4).map((r) => r.children[0].textContent + '=' + r.children[1].textContent).join(' / '))
  process.exit(0)
}
// ★入口の案内を閉じるまでゲームは始まらない（言語を選ぶ前に冒頭を印字しないため）。
//   器も実際の手順どおり、まず閉じる
setTimeout(() => {
  // ★閉じる前に本文が出ていたら、言語を選んでも手遅れになっている（実機で出た不具合）
  introGate = els.screen.children.length === 0
  els['intro-ja'].dispatch('click')      // ★これが「はじめる」を兼ねる
  setTimeout(tick, 100)
}, 150)
