'use strict'
/**
 * レシートプリンタ（58mm 感熱・Jieli 4c4a:4155 / BLE 名 YHK-…）へ、遊んだ記録を印字する（2026-11-27）。
 *
 * ★出どころは へちま言語ラボ（`hechima/site/src/qr/receipt.ts` と `printer.ts`・同じ作者）。
 *   機器の値（USB の vendor・BLE のサービス・帯ごとに 500ms 待つ）は、そこで実機を使って確かめたもの
 *   （`receipt-printer/` の 2026-11-04 の調査）。★折り返し・帯づくりは `receipt.ts` と同じ形のまま写した
 *   （向こうを直したらこちらも直す）。描画と機器の出し入れは `printer.ts` に倣った素の JS。
 * ★この機種は ESC/POS の**ラスタ画像（`GS v 0`）しか印字しない** —— テキスト命令は黙って無視される。
 *   だから日本語も英数字も、幅 384 ドットの白黒画像にしてから送る。
 * ★**流量制御が無い**（あふれた分は黙って捨てる）ので、帯（24 行）ごとに待つ。500ms で欠けず、150ms では欠けた。
 * ★経路は WebUSB と Web Bluetooth。どちらも Chrome 系（Android / ChromeOS / PC）だけで、**iPhone / iPad の Safari には無い**。
 *
 * 純粋な部分（`normalize` / `wrapLines` / `bandsFromRgba` / `feed` / `qrMatrix` / `turnText`）は DOM を触らない
 * = `test/run-receipt.js` が node で直接読む。
 */

/** 印字幅（ドット）。203dpi で約 48mm。 */
const WIDTH = 384
/** 1 つの `GS v 0` に入れる行数（＝ 送る側が待つ単位）。 */
const BAND = 24
/** 初期化（ESC @）。 */
const INIT = Uint8Array.of(0x1b, 0x40)

/** 末尾の紙送り（LF × n）。★切り取り位置まで本文を出すため。 */
function feed(lines) {
  return new Uint8Array(Math.max(0, Math.min(20, Math.floor(lines)))).fill(0x0a)
}

/* 行頭に来てはいけない文字（簡易の禁則）。★来そうになったら、前の 1 字を連れて次の行へ送る（追い出し）。
   ぶら下げは使えない —— 幅 384 ドットの外は印字されない。 */
const NO_START = new Set([...'、。，．,.・：；:;？！?!」』）)］]｝}】〕〉》〟’”ーぁぃぅぇぉっゃゅょゎァィゥェォッャュョヮヵヶ…‥々ゝゞヽヾ'])

/** 印字用に本文を整える（改行の統一・タブ・末尾の空行）。 */
function normalize(text) {
  return String(text).replace(/\r\n?/g, '\n').replace(/\t/g, '    ').replace(/\s+$/, '')
}

/**
 * 本文を幅 `width` に収まる行に割る。`measure` は文字列の幅を返す（canvas の measureText）。
 * ★空の段落（空行）はそのまま 1 行として残す。★サロゲートペアは 1 字として扱う。
 */
function wrapLines(text, width, measure) {
  const out = []
  for (const para of text.split('\n')) {
    let cur = []
    for (const ch of para) {
      if (cur.length && measure(cur.join('') + ch) > width) {
        if (NO_START.has(ch) && cur.length > 1) {
          const last = cur.pop()
          out.push(cur.join(''))
          cur = [last, ch]
        } else {
          out.push(cur.join(''))
          cur = [ch]
        }
      } else cur.push(ch)
    }
    out.push(cur.join(''))
  }
  return out
}

/**
 * RGBA の画像（白地に黒）を `GS v 0` の帯に切る。★暗い（R < 128）画素が印字される。
 * `w` は 8 の倍数であること（幅 384 は 48 バイト）。
 */
function bandsFromRgba(px, w, h) {
  const wb = w >> 3
  const out = []
  for (let y0 = 0; y0 < h; y0 += BAND) {
    const rows = Math.min(BAND, h - y0)
    const band = new Uint8Array(8 + wb * rows)
    band.set([0x1d, 0x76, 0x30, 0x00, wb & 0xff, wb >> 8, rows & 0xff, rows >> 8])
    for (let y = 0; y < rows; y++) {
      for (let xb = 0; xb < wb; xb++) {
        let byte = 0
        for (let bit = 0; bit < 8; bit++) {
          if (px[((y0 + y) * w + xb * 8 + bit) * 4] < 128) byte |= 0x80 >> bit
        }
        band[8 + y * wb + xb] = byte
      }
    }
    out.push(band)
  }
  return out
}

/** QR の模様（`true` = 黒）。`qrcode` は qrcode-generator（vendor/qrcode.js）。誤り訂正は M（印字のかすれに少し強い） */
function qrMatrix(qrcode, text) {
  const qr = qrcode(0, 'M')
  qr.addData(text)
  qr.make()
  const n = qr.getModuleCount()
  const m = []
  for (let r = 0; r < n; r++) {
    const row = []
    for (let c = 0; c < n; c++) row.push(qr.isDark(r, c))
    m.push(row)
  }
  return m
}

/**
 * 1 手ぶんの記録の文。`items` は画面に足した段落を `{ cls, text }` にしたもの（打った言葉 = `cmd`）。
 * ★送った英語（`sent`・「英語コマンドを表示」）は載せない —— 画面で薄く見せている補助で、遊んだ記録ではない。
 * ★打った言葉は「＞」を頭に付ける（画面の入力欄の印と同じ）。段落は空行で区切る。
 */
function turnText(items) {
  const out = []
  for (const it of items) {
    const t = String(it.text || '').replace(/\s+$/, '')
    if (it.cls === 'sent' || !t.trim()) continue
    if (it.cls === 'hr') { out.push('――――――――'); continue }
    out.push(it.cls === 'cmd' ? '＞ ' + t.trim() : t)
  }
  return out.join('\n\n')
}

// ---- ここから下はブラウザ（canvas・WebUSB・Web Bluetooth）----

const USB_VENDOR = 0x4c4a                                       /* Jieli */
const BLE_SVC = '49535343-fe7d-4ae5-8fa9-9fafd205e455'          /* ISSC 透過シリアル */
const BLE_RX = '49535343-8841-43f4-a8d4-ecbe34729bb3'           /* 書き込み先 */
const BLE_CHUNK = 200                                           /* 1 回に書くバイト数 */
const BLE_GAP = 10                                              /* その間隔（ms） */
const PACE = 500                                                /* 帯ごとに待つ（ms）。150 では欠けた */
/* ★本文は明朝体だが、感熱の 1 ドットでは明朝の細い横線が消える。印字はゴシックで描く */
const FONT = '"Noto Sans JP", "Noto Sans CJK JP", "Hiragino Sans", "Hiragino Kaku Gothic ProN", "Yu Gothic", "Meiryo", sans-serif'

const sleep = (ms) => new Promise((r) => setTimeout(r, ms))

async function openUsb(nav, onLost) {
  const usb = nav.usb
  const dev = await usb.requestDevice({ filters: [{ vendorId: USB_VENDOR }, { classCode: 0x07 }] })
  await dev.open()
  if (dev.configuration === null) await dev.selectConfiguration(1)
  let iface = -1
  let ep = -1
  for (const itf of dev.configuration.interfaces) {
    const out = itf.alternates[0] && itf.alternates[0].endpoints.find((e) => e.direction === 'out' && e.type === 'bulk')
    if (out) { iface = itf.interfaceNumber; ep = out.endpointNumber; break }
  }
  if (ep < 0) throw new Error('印字用の出口（bulk OUT）が見つからない')
  await dev.claimInterface(iface)
  const lost = (e) => { if (e.device === dev) { usb.removeEventListener('disconnect', lost); onLost() } }
  usb.addEventListener('disconnect', lost)
  return {
    name: `${dev.productName || 'USB プリンタ'}（USB）`,
    async send(part) {
      const r = await dev.transferOut(ep, part)
      if (r.status !== 'ok') throw new Error(`USB 送信 ${r.status}`)
    },
    close() { usb.removeEventListener('disconnect', lost); dev.close().catch(() => {}) },
  }
}

async function openBle(nav, onLost) {
  const dev = await nav.bluetooth.requestDevice({
    filters: [{ namePrefix: 'YHK' }, { services: [BLE_SVC] }],
    optionalServices: [BLE_SVC],
  })
  if (!dev.gatt) throw new Error('この機器には GATT が無い')
  const lost = () => { dev.removeEventListener('gattserverdisconnected', lost); onLost() }
  dev.addEventListener('gattserverdisconnected', lost)
  const server = await dev.gatt.connect()
  /* ★最初のサービス一覧が空で返ることがある（実機）ので、少し待って取り直す */
  let svc = null
  for (let i = 0; i < 3 && !svc; i++) {
    try { svc = await server.getPrimaryService(BLE_SVC) } catch (e) { await sleep(1000) }
  }
  if (!svc) { dev.gatt.disconnect(); throw new Error('印字用のサービスが見つからない') }
  const ch = await svc.getCharacteristic(BLE_RX)
  const write = ch.writeValueWithoutResponse ? ch.writeValueWithoutResponse.bind(ch) : ch.writeValue.bind(ch)
  return {
    name: `${dev.name || 'Bluetooth プリンタ'}（Bluetooth）`,
    async send(part) {
      for (let i = 0; i < part.length; i += BLE_CHUNK) {
        await write(part.subarray(i, i + BLE_CHUNK))
        await sleep(BLE_GAP)
      }
    },
    close() { dev.removeEventListener('gattserverdisconnected', lost); if (dev.gatt.connected) dev.gatt.disconnect() },
  }
}

/** canvas を作る。★器（node の検査）には canvas が無いので null を返し、描かずに記録だけ残す */
function newCanvas(w, h) {
  if (typeof document === 'undefined') return null
  const c = document.createElement('canvas')
  if (!c || typeof c.getContext !== 'function') return null
  c.width = w
  c.height = Math.max(1, h)
  const ctx = c.getContext('2d', { willReadFrequently: true })
  if (!ctx) return null
  ctx.fillStyle = '#fff'
  ctx.fillRect(0, 0, c.width, c.height)
  ctx.fillStyle = '#000'
  ctx.textBaseline = 'top'
  return { c, ctx }
}

/**
 * 字の濃さ（2026-11-27・実機で「かすれ気味」）。細いゴシックの画数の多い字は、縁のぼかし（灰色）を
 * しきい値 128 で白に落とすと線が削れる。★濃くするほど、灰色を黒に寄せ（しきい値を上げ）、字を太くする。
 *   0 = ふつう（へちま言語ラボの /qr/ と同じ）/ 1 = 太字（既定）/ 2 = もっと濃く（太字を 1 ドットずらして重ね描き）
 */
const WEIGHTS = [
  { bold: false, over: 0, cut: 128 },
  { bold: true, over: 0, cut: 160 },
  { bold: true, over: 1, cut: 176 },
]
const weightOf = (w) => WEIGHTS[Math.max(0, Math.min(WEIGHTS.length - 1, Number(w) || 0))]

/** 字を描く（「もっと濃く」は横に 1 ドットずらして重ねる） */
function drawText(ctx, text, x, y, wt) {
  ctx.fillText(text, x, y)
  for (let d = 1; d <= wt.over; d++) ctx.fillText(text, x + d, y)
}

/** canvas を二値にして帯へ。`cut` より暗い画素を黒にする */
function bandsOf(cv, cut) {
  const img = cv.ctx.getImageData(0, 0, cv.c.width, cv.c.height)
  const p = img.data
  const th = cut || 128
  for (let i = 0; i < p.length; i += 4) {
    const v = p[i] * 0.299 + p[i + 1] * 0.587 + p[i + 2] * 0.114 < th ? 0 : 255
    p[i] = p[i + 1] = p[i + 2] = v
  }
  return bandsFromRgba(p, cv.c.width, cv.c.height)
}

/** 本文を描いた帯の列。★長くても canvas の高さの上限に当たらないよう、2,400 ドットごとに描く */
function textBands(text, size, weight) {
  const wt = weightOf(weight)
  const font = `${wt.bold ? 'bold ' : ''}${size}px ${FONT}`
  const m = newCanvas(8, 8)
  if (!m) return []
  m.ctx.font = font
  const t = normalize(text)
  // ★重ね描きでずらす分だけ幅を詰めて割る（右端が 384 ドットの外へ出ないように）
  const lines = t === '' ? [] : wrapLines(t, WIDTH - wt.over, (s) => m.ctx.measureText(s).width)
  const lh = Math.round(size * 1.35)
  const step = Math.max(1, Math.floor(2400 / lh))
  const out = []
  for (let from = 0; from < lines.length; from += step) {
    const n = Math.min(step, lines.length - from)
    const cv = newCanvas(WIDTH, n * lh)
    cv.ctx.font = font
    const pad = Math.round((lh - size) / 2)
    for (let i = 0; i < n; i++) drawText(cv.ctx, lines[from + i], 0, i * lh + pad, wt)
    out.push(...bandsOf(cv, wt.cut))
  }
  return out
}

/**
 * 頭書き: 名前・ひとこと・QR を 2 つ横に並べ、下に URL。`qrs` = [{ url, label }]。
 * ★QR の 1 模様は 4 ドット（≒ 0.5mm）。スマホのカメラで読める大きさで、2 つ並べても 384 に収まる
 */
function headerBands(qrcode, head, weight) {
  const wt = weightOf(weight)
  const b = wt.bold ? 'bold ' : ''
  const cell = 4
  const quiet = 4
  const mats = (head.qrs || []).map((q) => qrMatrix(qrcode, q.url))
  const side = Math.max(0, ...mats.map((m) => (m.length + quiet * 2) * cell))
  const titleH = 56
  const lineH = 30
  const subLines = head.lines || []
  const h = titleH + subLines.length * lineH + 12 + side + 30 + 8
  const cv = newCanvas(WIDTH, h)
  if (!cv) return []
  const { ctx } = cv
  ctx.textAlign = 'center'
  ctx.font = `bold 44px ${FONT}`
  ctx.fillText(head.title, WIDTH / 2, 4)
  ctx.font = `${b}20px ${FONT}`
  subLines.forEach((l, i) => drawText(ctx, l, WIDTH / 2, titleH + i * lineH, wt))
  const top = titleH + subLines.length * lineH + 12
  const slot = WIDTH / Math.max(1, mats.length)
  mats.forEach((m, k) => {
    const x0 = Math.round(slot * k + (slot - (m.length + quiet * 2) * cell) / 2) + quiet * cell
    const y0 = top + quiet * cell
    for (let r = 0; r < m.length; r++) {
      for (let c = 0; c < m.length; c++) if (m[r][c]) ctx.fillRect(x0 + c * cell, y0 + r * cell, cell, cell)
    }
    ctx.font = `${b}18px ${FONT}`
    drawText(ctx, head.qrs[k].label, Math.round(slot * k + slot / 2), top + side + 2, wt)
  })
  return bandsOf(cv, wt.cut)
}

/**
 * プリンタ。`print(item)` は順番待ちに積むだけで、すぐ返る（遊びを止めない）。
 *   item = { text } | { header: { title, lines, qrs } } | { feed: n }
 * ★積んだものは `log` にも残す（文字のまま。検査が中身を見る）
 */
function createReceipt(opts) {
  const o = opts || {}
  const nav = o.navigator || (typeof navigator !== 'undefined' ? navigator : {})
  const qrcode = o.qrcode || (typeof globalThis !== 'undefined' ? globalThis.qrcode : null)
  const onStatus = o.onStatus || (() => {})
  const size = o.size || 24
  // ★字の濃さは**印字するときに**引く（設定を変えたら次の 1 手から効く）
  const weight = typeof o.weight === 'function' ? o.weight : () => (o.weight == null ? 1 : o.weight)
  let link = null
  const queue = []
  let running = false
  const log = []

  const lost = () => { link = null; queue.length = 0; onStatus('プリンタとの接続が切れた。', 'err') }

  async function connect(kind) {
    if (link) link.close()
    link = null
    try {
      onStatus('プリンタを選ぶ…')
      link = kind === 'usb' ? await openUsb(nav, lost) : await openBle(nav, lost)
      await link.send(INIT)
      onStatus(`つないだ: ${link.name}`, 'ok')
      return true
    } catch (e) {
      // ★選ぶ画面を閉じただけ（NotFoundError）は失敗として騒がない
      onStatus(e && e.name === 'NotFoundError' ? 'つなぐのをやめた。' : `つなげなかった —— ${(e && e.message) || e}`, 'err')
      return false
    }
  }

  function bandsFor(item) {
    if (item.header) return qrcode ? headerBands(qrcode, item.header, weight()) : textBands([item.header.title, ...(item.header.lines || []), ...(item.header.qrs || []).map((q) => q.url)].join('\n'), size, weight())
    if (item.feed) return [feed(item.feed)]
    return textBands(item.text || '', size, weight())
  }

  async function pump() {
    if (running) return
    running = true
    try {
      while (queue.length && link) {
        const item = queue.shift()
        const bands = bandsFor(item)
        for (const b of bands) {
          if (!link) break
          await link.send(b)
          // ★帯ごとに印字が追いつくのを待つ（流量制御が無いので、あふれると黙って欠ける）
          if (b[0] === 0x1d) await sleep(PACE)
        }
      }
    } catch (e) {
      onStatus(`印字できなかった —— ${(e && e.message) || e}`, 'err')
      queue.length = 0
    }
    running = false
    if (link) onStatus(`つないだ: ${link.name}`, 'ok')
  }

  return {
    usb: !!nav.usb,
    ble: !!nav.bluetooth,
    log,
    connect,
    connected: () => !!link,
    waiting: () => queue.length,
    print(item) {
      log.push(item.header ? { header: item.header } : item.feed ? { feed: item.feed } : { text: item.text })
      if (!link) return false
      queue.push(item)
      if (queue.length > 1 || running) onStatus(`印字の順番待ち: ${queue.length}`)
      pump()
      return true
    },
    close() { if (link) link.close(); link = null; queue.length = 0 },
  }
}

{
  const api = { WIDTH, BAND, INIT, WEIGHTS, feed, normalize, wrapLines, bandsFromRgba, qrMatrix, turnText, createReceipt }
  if (typeof module !== 'undefined' && module.exports) module.exports = api
  if (typeof window !== 'undefined') window.ZenmaiReceipt = api
}
