'use strict'
/** コマンド変換の参照出力: 入力(1 行 1 件)→ JSON 行。C 移植(cmd.c)との diff 用。 */
const fs = require('fs')
const path = require('path')
const { createCommander } = require(path.join(__dirname, '..', 'src/command.js'))

const asset = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'assets/zork1-cmd.json'), 'utf8'))
const cm = createCommander(asset)

const lines = fs.readFileSync(process.argv[2], 'utf8').split('\n').filter((l) => l.length)
const out = []
for (const input of lines) {
  const r = cm.toCommand(input)
  out.push(JSON.stringify({
    command: r.command,
    note: r.note || '',
    unknown: r.unknown || [],
    echo: r.echo || '',
    alts: r.alts || [],
    objDisp: r.objDisp || '',
    echoWord: r.needsObject ? '' : (r.echoWord || ''),
    said: r.said || '',
    needsObject: !!r.needsObject,
    ask: r.needsObject ? (r.ask || '') : '',
  }))
}
fs.writeFileSync(process.argv[3], out.join('\n'))
console.log(`${process.argv[3]}: ${out.length} 件`)
