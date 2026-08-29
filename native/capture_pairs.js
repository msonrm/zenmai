'use strict'
/**
 * 訳の層の参照データ捕獲: walkthrough を流し、Translator.line() の入出力対を全部記録する。
 * C 移植(translate.c)はこの対を 1 行残らず再現しなければならない(JS が正典)。
 *
 * 使い方: node capture_pairs.js ../test/walkthrough.txt pairs.jsonl
 * 形式: 1 行 1 JSON [raw, ja|null](null = 訳せず素通し)
 */
const fs = require('fs')
const path = require('path')
const ZVM = require('ifvms/src/zvm.js')
const Z = path.join(__dirname, '..')
const { createGlk } = require(path.join(Z, 'src/glk-shim.js'))
const { Translator } = require(path.join(Z, 'src/translate.js'))

const cmds = fs.readFileSync(process.argv[2], 'utf8').split('\n').map((s) => s.trim()).filter(Boolean)
const out = process.argv[3]

// ★walkthrough では出ない行（下の update() の末尾で流す）。
//   聞き返しはスロットが空白だけで隣り合う唯一の骨格で、**目的語が 2 語以上のとき**だけ壊れる
const EXTRA = [
  'What do you want to put the nasty knives in?',
  'What do you want to put the jewel-encrusted egg in?',
  'What do you want to put the sword in?',
  'What do you want to attack the troll with?',
  'What do you want to tie the rope to?',
]

const tr = new Translator(JSON.parse(fs.readFileSync(path.join(Z, 'assets/zork1-ja.json'), 'utf8')))
const pairs = []
const origLine = tr.line.bind(tr)
tr.line = (raw) => {
  const ja = origLine(raw)
  pairs.push([raw, ja, tr.echoWord || null])
  return ja
}

let idx = 0
const Glk = createGlk({
  cols: 64, rows: 24,
  write(t) { tr.feed(t) },
  status() {},
  update() {
    tr.flush()
    if (Glk.waitingFor() === 'char') return setImmediate(() => Glk.submitChar(32))
    if (Glk.waitingFor() !== 'line') return
    if (idx >= cmds.length) {
      // ★walkthrough が踏まない行をここで足す。聞き返し（orphan）は最短の勝ち筋に出ないので、
      //   捕獲だけでは C 側のゴールデンに入らない（実プレイ 2026-08-21 の
      //   `何knives innastyを入れる？` は、この穴に落ちていた）。
      //   ここは**流すだけ** —— 期待値の正しさは test/run-cmd.js が持ち、
      //   こちらは「C が JS を再現するか」だけを見る（JS が正典）
      tr.setEcho(null)
      for (const raw of EXTRA) tr.line(raw)
      fs.writeFileSync(out, pairs.map((p) => JSON.stringify(p)).join('\n'))
      console.log(`${out}: ${pairs.length} 行(${cmds.length} 手)/ 未訳 ${tr.stats.miss}`)
      process.exit(0)
    }
    const c = cmds[idx++]
    // ★web 版と同じく、打った語を反響(ECHO)用に渡す(EN 入力なのでコマンドの最終語)
    const words = c.split(/\s+/)
    tr.setEcho(words[words.length - 1])
    setImmediate(() => Glk.submitLine(c))
  },
})
const vm = new ZVM()
vm.prepare(fs.readFileSync(path.join(Z, 'vendor/zork1/zork1.z3')), { vm, Glk, GlkOte: null, Dialog: null })
Glk.init({ vm })
// ★乱数を固定する。ZVM は Z-machine 標準のシードモードを持っていて、`xorshift_seed` が
//   0 以外なら Xorshift で決定的に返す（0 = `Math.random`）。`Glk.init` が 0 で初期化するので
//   その**後**に入れる。
//   ★これが無いと戦闘や泥棒の分岐が毎回ゆれ、`pairs.h` が再生成のたびに 900 行規模で変わる。
//   行数は 709 → 711 でも diff が読めない ＝ **C 側ゴールデンの更新を人間が検算できない**。
//   固定した今は、意味のある変更だけが diff に出る
vm.xorshift_seed = 0x5EED5EED
