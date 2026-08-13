'use strict'
/**
 * 日本語 → 英語コマンド。
 *
 * ★定式化: これは「日本語を解析する」問題ではなく「**閉じた命令言語へ翻訳する**」問題。
 * 原作の文法は有限の表（実測: SYNTAX 269 行 / 動詞 135 / 前置詞 18 / 目的語は最大 2 つ）で、
 * こちらはパーサを置き換えない。曖昧解消・ALL・代名詞・名詞の聞き返しは**原作が持っている**。
 *
 * そして日本語は英語より有利な面がある —— ★**格助詞が役を明示する**。
 *   「ランタンで扉を開ける」→ を=直接目的語(door) / で=道具(lantern) → `open door with lantern`
 * 英語は語順で役を決めるが、日本語は助詞が教えてくれるので語順が自由でも正規化できる。
 */

// 方角は原作が単語で受ける（`north` だけで動く）
const DIRS = {
  北東: 'northeast', 北西: 'northwest', 南東: 'southeast', 南西: 'southwest',
  北: 'north', 南: 'south', 東: 'east', 西: 'west',
  上: 'up', 下: 'down', 中: 'in', 外: 'out',
}
// 助詞 → 役。長いものから当てる
const PARTICLES = [
  ['の中に', 'IN'], ['の中へ', 'IN'], ['の下', 'UNDER'], ['の後ろ', 'BEHIND'], ['の裏', 'BEHIND'],
  ['の上に', 'ON'], ['の上', 'ON'], ['を使って', 'WITH'], ['によって', 'WITH'],
  ['から', 'FROM'], ['には', 'TO'], ['へ', 'TO'], ['に', 'TO'], ['で', 'WITH'],
  ['を', 'O'], ['は', 'O'], ['が', 'O'], ['と', 'AND'],
]

const strip = (s) => s.replace(/[。、．，！？\s]+/g, '')

function createCommander(asset) {
  // 語彙をひとつの表に畳んで、長いものから当てる（逐次入力の配列エンジンと同じ最長一致）
  const lex = []
  for (const [key, v] of Object.entries(asset.verbs || {})) {
    for (const ja of v.ja) lex.push({ ja, kind: 'verb', key, shapes: v.shapes })
  }
  // ★同じ名詞を複数の物が共有していると原作が聞き返してくる（`door` = 木の扉 / 揚げ戸）。
  //   形容詞を持っているならそれを添えて、こちらで曖昧さを解いておく
  const nounUse = {}
  for (const o of Object.values(asset.objects || {})) {
    const n = (o.nouns[0] || '').toLowerCase()
    nounUse[n] = (nounUse[n] || 0) + 1
  }
  for (const [key, o] of Object.entries(asset.objects || {})) {
    const noun = (o.nouns[0] || '').toLowerCase()
    const adj = (o.adjs[0] || '').toLowerCase()
    const word = nounUse[noun] > 1 && adj ? adj + ' ' + noun : noun
    for (const ja of o.ja) lex.push({ ja, kind: 'obj', key, word })
  }
  for (const [ja, en] of Object.entries(DIRS)) lex.push({ ja, kind: 'dir', word: en })
  lex.sort((a, b) => b.ja.length - a.ja.length)

  /** @returns {{command:string|null, trace:string, unknown:string[]}} */
  function toCommand(input) {
    const s = strip(String(input || ''))
    if (!s) return { command: null, trace: '空', unknown: [] }
    if (/^[\x20-\x7e]+$/.test(input.trim())) {
      return { command: input.trim(), trace: '英語のまま', unknown: [] }   // 英語で打たれたら素通し
    }
    const found = []          // { kind, key, word, role }
    const unknown = []
    let i = 0
    while (i < s.length) {
      const hit = lex.find((e) => s.startsWith(e.ja, i))
      if (hit) {
        i += hit.ja.length
        // 直後の助詞を読む（役を決める）
        let role = null
        for (const [p, r] of PARTICLES) {
          if (s.startsWith(p, i)) { role = r; i += p.length; break }
        }
        found.push({ ...hit, role })
        continue
      }
      // 語彙にない部分は読み飛ばす（何が落ちたかは残す）
      let j = i + 1
      while (j < s.length && !lex.some((e) => s.startsWith(e.ja, j))) j++
      unknown.push(s.slice(i, j))
      i = j
    }
    const verb = found.find((f) => f.kind === 'verb')
    const dirs = found.filter((f) => f.kind === 'dir')
    const objs = found.filter((f) => f.kind === 'obj')

    // 方角だけ（「北」「北へ行く」）→ 方角の語をそのまま渡す
    if (dirs.length && (!verb || verb.key === 'WALK')) {
      return { command: dirs[0].word, trace: '方角', unknown }
    }
    // 「中に入る」「外に出る」の中／外は方角ではなく動詞のほうに含まれている
    const bareDirs = dirs.filter((d) => !(verb && ['ENTER', 'EXIT', 'LEAVE'].includes(verb.key) && ['in', 'out'].includes(d.word)))
    if (!verb) {
      // 動詞が無い → 名詞だけ（「絨毯」）。原作が「何を？」と聞き返す形に寄せる
      if (objs.length) return { command: objs[0].word, trace: '名詞のみ', unknown }
      return { command: null, trace: '動詞が見つからない', unknown }
    }

    // ★役の割り当て: を/は/が → 直接目的語、で/を使って → 道具、に/へ/の中に… → 第 2 目的語
    let prso = objs.find((o) => o.role === 'O' || o.role === null) || null
    const tool = objs.find((o) => o.role === 'WITH')
    const dest = objs.find((o) => ['TO', 'IN', 'ON', 'UNDER', 'BEHIND', 'FROM'].includes(o.role))
    if (!prso && dest && objs.length === 1) { prso = dest }

    const out = [verb.key.toLowerCase()]
    const shapes = verb.shapes || []
    const has = (p) => shapes.some((sh) => sh.split(' ').includes(p))
    if (prso && prso !== dest) out.push(prso.word)
    if (tool && has('WITH')) out.push('with', tool.word)
    if (dest && dest !== prso) {
      const p = has(dest.role) ? dest.role.toLowerCase() : (has('IN') ? 'in' : 'to')
      out.push(p, dest.word)
    } else if (dest && dest === prso && has(dest.role)) {
      // 「絨毯の下を見る」= 前置詞つきの目的語が 1 つだけ → `look under rug`
      out.length = 1
      out.push(dest.role.toLowerCase(), dest.word)
    }
    if (!prso && !tool && !dest && bareDirs.length) out.push(bareDirs[0].word)
    return { command: out.join(' '), trace: '動詞+役', unknown }
  }

  return { toCommand, lex }
}

{
  const api = { createCommander, DIRS, PARTICLES }
  if (typeof module !== 'undefined' && module.exports) module.exports = api
  if (typeof window !== 'undefined') window.ZenmaiCommand = api
}
