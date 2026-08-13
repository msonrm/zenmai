'use strict'
/**
 * 実証用のハーネス: 本物の Z-machine（ZVM）を走らせ、印字を横取りして日本語に差し替える。
 *
 * 差し込むのは Glk の 2 経路だけ:
 *   glk_put_jstring(text)              本文
 *   glk_put_jstring_stream(str, text)  ステータス行など、ストリーム指定つき
 *
 * 使い方: node test/run-ja.js [コマンド...]
 *   例: node test/run-ja.js "open mailbox" "read leaflet"
 */
const fs = require('fs')
const path = require('path')
const GlkOte = require('glkote-term')
const ZVM = require('ifvms/src/zvm.js')
const { Translator } = require('../src/translate.js')

const STORY = path.join(__dirname, '..', 'vendor', 'zork1', 'zork1.z3')
const ASSET = path.join(__dirname, '..', 'assets', 'zork1-ja.json')
if (!fs.existsSync(STORY)) {
  console.error('story file がない: ' + STORY + '\n（historicalsource/zork1 から取得するか ZILF でビルドする）')
  process.exit(2)
}

const tr = new Translator(JSON.parse(fs.readFileSync(ASSET, 'utf8')))
const Glk = GlkOte.Glk

// --- 印字の横取り（ここが翻訳の注ぎ口）---
const origJ = Glk.glk_put_jstring
const origJS = Glk.glk_put_jstring_stream
Glk.glk_put_jstring = function (text, ...rest) {
  return origJ.call(Glk, tr.feed(text), ...rest)
}
Glk.glk_put_jstring_stream = function (str, text, ...rest) {
  // ステータス行は行単位で来ないので、語として引く（部屋名がここに出る）
  return origJS.call(Glk, str, tr.word(text), ...rest)
}

// --- 入力: 引数で渡したコマンドを順に流し、尽きたら終了する ---
const cmds = process.argv.slice(2)
let n = 0
const MuteStream = require('mute-stream')
const stdout = new MuteStream()
stdout.pipe(process.stdout)
const stdin = new (require('stream').Readable)({ read() {} })
const rl = require('readline').createInterface({ input: stdin, output: stdout, terminal: false })
const opts = { rl, stdin, stdout }

const vm = new ZVM()
const options = { vm, Dialog: new GlkOte.Dialog(opts), Glk, GlkOte: new GlkOte.DumbGlkOte(opts) }
vm.prepare(fs.readFileSync(STORY), options)

// 入力を待つタイミングで次のコマンドを差し込む
const pump = () => {
  if (n < cmds.length) {
    const c = cmds[n++]
    process.stdout.write('\n> ' + c + '\n')
    stdin.push(c + '\n')
  } else {
    const rest = tr.flush()
    if (rest) process.stdout.write(rest + '\n')
    const m = tr.stats
    process.stderr.write(`\n--- 引けた ${m.hit} 行（うち貪欲 ${m.greedy}）/ 訳さない ${m.notrans} 行 / ★引けなかった ${m.miss} 行 ---\n`)
    for (const s of [...m.missed].slice(0, 30)) process.stderr.write('  未訳: ' + s + '\n')
    process.exit(0)
  }
}
setInterval(pump, 150)
Glk.init(options)   // これが vm.init() を呼ぶ
