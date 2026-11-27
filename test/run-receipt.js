'use strict'
/**
 * レシートに印字（2026-11-27）。`src/receipt.js` の純粋な部分と、Web の流れ（偽の USB プリンタ）。
 *
 * ★器には canvas が無いので、**絵（帯）の中身は見ない**。見るのは ① 折り返し・帯の形・QR の模様（純粋な関数）
 *   ② 印字に積んだ順番と中身（文字のまま・`receipt.log`）③ 送ったバイト列の形（初期化 → … → 紙送り）。
 *   ★実際の刷り上がり（字の濃さ・QR が読めるか）は実機で見る
 *
 * 使い方: node test/run-receipt.js（WORK=zork2 で作品を替える）
 */
const { spawnSync } = require('child_process')
const path = require('path')
const R = require('../src/receipt.js')
const qrcode = require('../vendor/qrcode.js')
const work = require('./work.js')

let ng = 0
const check = (ok, name) => { if (!ok) ng++; console.log(`${ok ? '✓' : '✗'} ${name}`) }

// --- 純粋な部分 ---
{
  const m = (s) => [...s].length * 24          // 全角 24 ドット = 1 行 16 字
  const lines = R.wrapLines('あいうえおかきくけこさしすせそた。なにぬ', R.WIDTH, m)
  check(lines[0].length === 15 && lines[1].startsWith('た。'), `禁則: 行頭の「。」は前の 1 字を連れて送る（${JSON.stringify(lines)}）`)
  check(R.wrapLines('あ\n\nい', R.WIDTH, m).length === 3, '空行は 1 行として残す')
  check(R.normalize('a\r\nb\t\n\n') === 'a\nb', '改行の統一・末尾の空行を落とす')
  const w = R.WIDTH, h = 30
  const px = new Uint8ClampedArray(w * h * 4).fill(255)
  const dot = (x, y) => { const i = (y * w + x) * 4; px[i] = px[i + 1] = px[i + 2] = 0 }
  dot(0, 0); dot(w - 1, 25)
  const bands = R.bandsFromRgba(px, w, h)
  check(bands.length === 2 && bands[0].length === 8 + 48 * 24 && bands[1].length === 8 + 48 * 6, '帯は 24 行ずつ（30 行 → 24 + 6）')
  check(bands[0][0] === 0x1d && bands[0][1] === 0x76 && bands[0][2] === 0x30 && bands[0][4] === 48, '帯の頭は GS v 0・幅 48 バイト')
  check(bands[0][8] === 0x80 && bands[1][8 + 48 * 1 + 47] === 0x01, '暗い画素が印字のビットになる（左上・右端）')
  // ★字の濃さ（実機で「かすれ気味」・2026-11-27）: 濃いほど灰色を黒に寄せる。既定（1）は太字
  check(R.WEIGHTS.length === 3 && R.WEIGHTS[1].bold && R.WEIGHTS.every((w, i) => i === 0 || w.cut > R.WEIGHTS[i - 1].cut), '字の濃さは 3 段・濃いほどしきい値が上がる・既定は太字')
  check(R.feed(4).length === 4 && R.feed(4).every((b) => b === 0x0a), '紙送りは LF × n')
  const q = R.qrMatrix(qrcode, 'https://zenmai.pages.dev/')
  const side = (q.length + 8) * 4
  check(q.length >= 21 && q.every((r) => r.length === q.length) && q[0][0] && q[0][6] && q[6][0] && !q[7][7], `QR の模様（${q.length}×${q.length}・角の検出模様）`)
  const q2 = R.qrMatrix(qrcode, 'https://msonrm.works/')
  check(Math.max(side, (q2.length + 8) * 4) * 2 <= R.WIDTH, `QR 2 つが幅 384 に並ぶ（1 つ ${side} ドット）`)
  check(R.turnText([{ cls: 'cmd', text: 'みる' }, { cls: 'sent', text: 'look' }, { cls: '', text: '森だ。' }]) === '＞ みる\n\n森だ。', '1 手の文: 打った言葉に「＞」・送った英語は載せない')
}

// --- Web の流れ（偽の USB プリンタ）---
const run = (mode, cmds) => {
  const r = spawnSync(process.execPath, [path.join(__dirname, 'run-web-dom.js'), ...cmds],
    { env: { ...process.env, WORK: work.WORK, ZENMAI_RECEIPT: mode }, encoding: 'utf8' })
  const get = (k) => { const m = r.stdout.match(new RegExp(`^--- ${k}: (.*)$`, 'm')); return m ? JSON.parse(m[1]) : null }
  return { ...r, log: get('receipt-log') || [], sent: get('receipt-sent') || [] }
}
{
  const r = run('1', ['いんじする', 'みる', 'いんじをやめる', 'みる'])
  const L = r.log
  const texts = L.filter((x) => x.text).map((x) => x.text)
  check(r.status === 0, '流れが最後まで動いた')
  check(!!L[0] && !!L[0].header && L[0].header.title === 'Zenmai', '最初に頭書き（Zenmai）')
  const urls = ((L[0] && L[0].header && L[0].header.qrs) || []).map((x) => x.url)
  check(urls.includes('https://zenmai.pages.dev/') && urls.includes('https://msonrm.works/'), '頭書きの QR は zenmai.pages.dev と msonrm.works')
  check(!/zork/i.test(JSON.stringify(L[0] && L[0].header.lines.slice(0, 2))), '頭書きの紹介は作品名を前に出さない')
  check(/^＞ いんじする/.test(texts[0] || '') && /Copyright/.test(texts[0] || ''), '最初の 1 手に原作の版権表示（印字の始まりの返事）')
  check(/^＞ みる/.test(texts[1] || '') && (texts[1] || '').split('\n\n').length >= 2, '1 手ごとに積む（打った言葉と返事。★印字の直後の手が落ちない = シムの待ちの直し）')
  check(/^＞ いんじをやめる/.test(texts[2] || '') && /Zenmai/.test(texts[3] || '') && L[L.length - 1].feed === 4, 'やめる手のあとに締めと紙送り')
  check(texts.length === 4, `やめたあとの手は載らない（積んだ文 ${texts.length} 個）`)
  check(r.sent.length >= 2 && r.sent[0] === 2 && r.sent[r.sent.length - 1] === 4, `送ったのは初期化（ESC @）で始まり紙送りで終わる（${JSON.stringify(r.sent)}）`)
}
{
  const r = run('idle', ['いんじする', 'みる'])
  check(r.log.length === 0 && /レシートプリンタにつないでいない/.test(r.stdout), 'つないでいないなら積まず、1 度だけ言う')
  check((r.stdout.match(/つないでいない/g) || []).length === 1, 'その断りは 1 度だけ')
}
console.log(`\n--- 食い違い ${ng} 件 ---`)
process.exit(ng ? 1 : 0)
