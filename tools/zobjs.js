'use strict'
// 版 3 の story の**物の名前の表**（検査用）。★略語（Room など）は展開しないので名前は欠けうる。
//   `findObj(names, /^cretin$/)` で物の番号を引く。プレイヤーは "cretin"。
const zobjs = (STORY) => {
const rw = (a) => (STORY[a] << 8) | STORY[a + 1]
const A0 = '                          ', A1 = ' ABCDEFGHIJKLMNOPQRSTUVWXYZ'.replace(/^ /, ' ')
function zstr(addr, words) {
  let out = ''
  let shift = 0
  const alpha = ['abcdefghijklmnopqrstuvwxyz', 'ABCDEFGHIJKLMNOPQRSTUVWXYZ', ' \n0123456789.,!?_#\'"/\\-:()']
  let abbr = 0
  for (let i = 0; i < words; i++) {
    const w = rw(addr + i * 2)
    for (const c of [(w >> 10) & 31, (w >> 5) & 31, w & 31]) {
      if (abbr) { abbr = 0; continue }
      if (c === 0) out += ' '
      else if (c <= 3) abbr = c
      else if (c === 4) shift = 1
      else if (c === 5) shift = 2
      else { out += alpha[shift][c - 6]; shift = 0 }
    }
  }
  return out
}
const objBase = rw(0x0a) + 62
const names = {}
for (let n = 1; n < 255; n++) {
  const ent = objBase + (n - 1) * 9
  const p = rw(ent + 7)
  if (p < 64 || p >= STORY.length) break
  const len = STORY[p]
  names[n] = zstr(p + 1, len)
}
  return names
}
const findObj = (names, re) => Object.keys(names).filter((k) => re.test(names[k])).map(Number)
module.exports = { zobjs, findObj }
