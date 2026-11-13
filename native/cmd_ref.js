'use strict'
/** コマンド変換の参照出力: 入力(1 行 1 件)→ JSON 行。C 移植(cmd.c)との diff 用。
 *   node native/cmd_ref.js corpus.txt ref.jsonl      （WORK=zork2 で作品を替える。既定 zork1）
 * ★相手の `cmd_test_host` は焼き込みの表（`cmd_data.c`）= いまは Zork I だけ。ほかの作品の参照出力は、
 *   C がその作品の語彙を載せる節目（`TODO.md` の「native が追うもの」）まで突き合わせる相手が無い */
const fs = require('fs')
const path = require('path')
const { createCommander } = require(path.join(__dirname, '..', 'src/command.js'))

const WORK = process.env.WORK || 'zork1'
const asset = JSON.parse(fs.readFileSync(path.join(__dirname, '..', `assets/${WORK}-cmd.json`), 'utf8'))
const cm = createCommander(asset)

const TRACE = {
  '空': 'EMPTY', '英語のまま': 'ENGLISH', 'パーサの語': 'PARSER', 'はい／いいえ': 'YESNO',
  '否定は扱えない': 'NEG', '知らない言葉': 'UNKNOWN', '名詞のみ': 'NOUN',
  '動詞が見つからない': 'NOVERB', '原作にない言い方': 'NOSHAPE', '方角': 'DIR',
  '動詞+役': 'OK', 'コマンド不適語': 'NOCMD',
}

const lines = fs.readFileSync(process.argv[2], 'utf8').split('\n').filter((l) => l.length)
const out = []
for (const input of lines) {
  const r = cm.toCommand(input)
  out.push(JSON.stringify({
    // ★C 側（cmd.h の CMD_TR_*）と同じ名前へ写す。**理由が違えば画面の文言が違う**ので、
    //   command が同じでも trace が食い違えば赤にしたい。
    trace: TRACE[r.trace] || '?',
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
