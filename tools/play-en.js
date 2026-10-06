// Zork II を英語のまま走らせる検査用ハーネス（呪文・杖・ロボットの入力方法を確かめる）。
// 使い方: WORK=zork2 node tools/play-en.js [コマンド...]。特別な行: "!名前" = 名前（正規表現）に合う物を持ち物へ / "+名前" = 今いる部屋へ / "#部屋" = プレイヤーをその部屋へ / "@" = 物の名前の表
const fs = require('fs'), path = require('path')
const ROOT = path.join(__dirname, '..')
module.paths.push(path.join(ROOT, 'node_modules'))
const GlkOte = require('glkote-term')
const ZVM = require('ifvms/src/zvm.js')
const STORY = fs.readFileSync(path.join(ROOT, 'vendor', process.env.WORK || 'zork2', (process.env.WORK || 'zork2') + '.z3'))
const Glk = GlkOte.Glk
const cmds = process.argv.slice(2)
const { zobjs, findObj: findObj0 } = require('./zobjs.js')
const names = zobjs(STORY)
const findObj = (re) => findObj0(names, re)
const orig = Glk.glk_put_jstring
let out = ''
Glk.glk_put_jstring = function (t, ...r) { return orig.call(Glk, t, ...r) }
let n = 0
const MuteStream = require('mute-stream')
const stdout = new MuteStream(); stdout.pipe(process.stdout)
const stdin = new (require('stream').Readable)({ read() {} })
const rl = require('readline').createInterface({ input: stdin, output: stdout, terminal: false })
const opts = { rl, stdin, stdout }
const vm = new ZVM()
const options = { vm, Dialog: new GlkOte.Dialog(opts), Glk, GlkOte: new GlkOte.DumbGlkOte(opts) }
vm.prepare(Buffer.from(STORY), options)
const pump = () => {
  if (n >= cmds.length) { setTimeout(() => process.exit(0), 300); return }
  const c = cmds[n++]
  if (c[0] === '#') {  // "#Room name" = プレイヤーをその部屋へ
    const player = findObj(/^cretin$/)[0], room = findObj(new RegExp(c.slice(1), 'i'))[0]
    process.stdout.write(`\n[移動: ${names[room]}]\n`); vm.insert_obj(player, room); return pump()
  }
  if (c[0] === '+') {   // "+name" = 名前に合う物をプレイヤーのいる部屋へ
    const player = findObj(/^cretin$/)[0], room = vm.get_parent(player)
    for (const o of findObj(new RegExp(c.slice(1), 'i'))) vm.insert_obj(o, room)
    process.stdout.write(`\n[部屋へ: ${c.slice(1)}]\n`); return pump()
  }
  if (c[0] === '!') {   // "!wand" = 名前に合う物を持ち物へ（プレイヤー = "cretin"）
    const player = findObj(/^cretin$/)[0], objs = findObj(new RegExp(c.slice(1), 'i'))
    process.stdout.write(`\n[持ち物へ: ${objs.map((o) => o + ':' + names[o]).join(', ')} → player ${player}]\n`)
    for (const o of objs) vm.insert_obj(o, player)
    return pump()
  }
  if (c[0] === '@') { process.stdout.write(`\n[names] ` + JSON.stringify(names) + '\n'); return pump() }
  process.stdout.write('\n> ' + c + '\n')
  stdin.push(c + '\n')
}
setInterval(pump, 120)
Glk.init(options)
