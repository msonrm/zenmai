'use strict'
/**
 * 最小の Glk 実装（ZVM が呼ぶ分だけ）。
 *
 * GlkOte は使わない。こちらは日本語の描画・コントローラ入力・独自の配置を持つので、
 * 表示層を自分で書く必要があり、**Glk と表示の境界を自分の手に持っておきたい**。
 *
 * ZVM との契約（`ifvms/src/zvm.js` を読んで確定した）:
 *   vm.prepare(data, { vm, Glk, GlkOte, Dialog })
 *   Glk.init(opts)  → vm.init() を呼ぶ。VM は run() して glk_select(RefStruct) で戻ってくる
 *   Glk.update()    → ここで画面を描く
 *   入力が来たら host が event を詰めて vm.resume() を呼ぶ
 *
 * 必要なのは 30 関数。うち 10 ほどはセーブ用のファイル系で、いまは素通り。
 */

const wintype_TextBuffer = 3
const wintype_TextGrid = 4
const evtype_CharInput = 2
const evtype_LineInput = 3

class RefBox {
  constructor() { this.value = 0 }
  set_value(v) { this.value = v }
  get_value() { return this.value }
}

class RefStruct {
  constructor() { this.fields = [] }
  push_field(v) { this.fields.push(v) }
  set_field(i, v) { this.fields[i] = v }
  get_field(i) { return this.fields[i] }
}

/**
 * @param {object} host
 *   host.write(text)      本文（バッファ窓）への出力
 *   host.status(line)     ステータス行（グリッド窓）の 1 行目
 *   host.update()         描画のタイミング
 *   host.cols, host.rows  画面の桁数・行数
 */
function createGlk(host) {
  let vm = null
  let curwin = null
  let pending = null          // glk_select が待っている RefStruct
  let lineReq = null          // { win, buf }
  let charReq = null
  const windows = []

  const newWindow = (type) => {
    const w = {
      type,
      grid: [],               // グリッド窓のみ: 行ごとの文字配列
      cx: 0, cy: 0,
      stream: null,
      parent: null,
    }
    w.stream = { win: w }
    windows.push(w)
    return w
  }

  const putStream = (str, text) => {
    if (!str || !text) return
    const w = str.win
    if (!w) return                                   // メモリ／ファイルストリームは捨てる
    if (w.type === wintype_TextGrid) {
      // ステータス行はカーソル位置に上書きする
      const row = (w.grid[w.cy] = w.grid[w.cy] || [])
      for (const ch of text) { row[w.cx++] = ch }
      if (w.cy === 0) host.status((w.grid[0] || []).join('').replace(/\s+$/, ''))
    } else {
      host.write(text)
    }
  }

  const Glk = {
    RefBox, RefStruct,

    init(opts) { vm = opts.vm; vm.init() },
    update() { host.update() },
    fatal_error(e) { host.write('\n[エラー] ' + (e && e.message ? e.message : e) + '\n'); host.update() },

    // --- 窓 ---
    glk_window_open(parent, method, size, wintype) {
      const w = newWindow(wintype)
      w.parent = parent || null
      return w
    },
    glk_window_close(win) {
      const i = windows.indexOf(win)
      if (i >= 0) windows.splice(i, 1)
    },
    glk_window_clear(win) {
      if (!win) return
      win.grid = []; win.cx = 0; win.cy = 0
      if (win.type === wintype_TextGrid) host.status('')
    },
    glk_window_get_parent(win) { return win ? win.parent : null },
    glk_window_get_stream(win) { return win ? win.stream : null },
    glk_window_get_size(win, widthbox, heightbox) {
      if (widthbox && widthbox.set_value) widthbox.set_value(host.cols)
      if (heightbox && heightbox.set_value) heightbox.set_value(win && win.type === wintype_TextGrid ? 1 : host.rows)
    },
    glk_window_move_cursor(win, x, y) { if (win) { win.cx = x; win.cy = y } },
    glk_window_set_arrangement() {},
    glk_window_iterate() { return null },
    glk_set_window(win) { curwin = win },

    // --- 出力 ---
    glk_put_jstring(text) { putStream(curwin && curwin.stream, text) },
    glk_put_jstring_stream(str, text) { putStream(str, text) },
    glk_put_char_stream_uni(str, code) { putStream(str, String.fromCharCode(code)) },
    glk_put_buffer_stream(str, buf) {
      putStream(str, Array.from(buf || []).map((c) => String.fromCharCode(c)).join(''))
    },
    glk_set_style() {},
    glk_stylehint_set() {},
    glk_stylehint_clear() {},
    glk_gestalt() { return 0 },

    // --- 入力 ---
    glk_request_line_event_uni(win, buf) { lineReq = { win, buf } },
    glk_request_char_event_uni(win) { charReq = { win } },
    glk_select(ev) { pending = ev },

    // --- セーブ関連（いまは素通り）---
    glk_fileref_create_by_prompt() { return null },
    glk_fileref_destroy() {},
    glk_stream_open_file() { return null },
    glk_stream_open_file_uni() { return null },
    glk_stream_close() {},
    glk_stream_iterate() { return null },
    glk_get_buffer_stream() { return -1 },
    glk_get_char_stream_uni() { return -1 },
    glk_get_line_stream_uni() { return -1 },

    // --- ホストが呼ぶ側 ---
    /** 1 行の入力を VM に渡して再開する */
    submitLine(text) {
      if (!lineReq || !pending) return false
      const { win, buf } = lineReq
      const s = String(text)
      for (let i = 0; i < s.length; i++) buf[i] = s.charCodeAt(i)
      pending.push_field(evtype_LineInput)
      pending.push_field(win)
      pending.push_field(s.length)
      pending.push_field(0)
      lineReq = null; pending = null
      vm.resume()
      return true
    },
    /** 1 文字の入力（「キーを押してください」用） */
    submitChar(code) {
      if (!charReq || !pending) return false
      pending.push_field(evtype_CharInput)
      pending.push_field(charReq.win)
      pending.push_field(code)
      pending.push_field(0)
      charReq = null; pending = null
      vm.resume()
      return true
    },
    /** いま何を待っているか */
    waitingFor() { return lineReq ? 'line' : charReq ? 'char' : null },
  }
  return Glk
}

// ★`module` を素で触るとブラウザ（<script> 読み込み）でその行で死ぬ。
//   node の require() では通ってしまうので、DOM スタブの検証で初めて出た
// ★`<script>` はグローバルを共有するので、トップレベルで同じ名前の const を
//   二重に宣言するとブラウザだけ構文エラーになる。ブロックで閉じる
{
  const api = { createGlk, RefBox, RefStruct }
  if (typeof module !== 'undefined' && module.exports) module.exports = api
  if (typeof window !== 'undefined') window.GlkShim = api
}
