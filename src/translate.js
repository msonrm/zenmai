'use strict'
/**
 * 出力の日本語化。
 *
 * ★設計の芯: **照合は行単位。** Z-machine は 1 行の出力を複数の印字呼び出しに割るので、
 * 改行まで貯めてから「英語の完成行」をキーに日本語を引く。
 * 断片単位で引くと、英語の `.` に日本語の述語が載る行（`Opening the X reveals ...`）や
 * 三単現を連結で作る行（`The X do` + `es` + `n't lead `）で必ず壊れる。
 *
 * 実行時の機械翻訳は使わない。引くのは事前に訳した完成文だけ。
 */

// テンプレートの骨格（`The {PRSO} is closed.`）を名前付きスロットの正規表現にする
function compile(en) {
  const names = []
  let re = ''
  let i = 0
  for (const m of en.matchAll(/\{([A-Z0-9,?!-]+)\}/g)) {
    re += escapeRe(en.slice(i, m.index))
    names.push(m[1].replace(/,$/, ''))
    re += '([\\s\\S]+?)'
    i = m.index + m[0].length
  }
  re += escapeRe(en.slice(i))
  return { re: new RegExp('^' + re + '$'), names }
}

function escapeRe(s) {
  return s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')
}

const norm = (s) => s.replace(/\s+/g, ' ').trim()

class Translator {
  constructor(asset) {
    this.exact = new Map()
    for (const [en, ja] of Object.entries(asset.exact)) this.exact.set(norm(en), ja)
    // 部屋名・物の名前。スロットに差し込まれるのでこれも引けるようにする
    this.props = new Map()
    for (const [en, v] of Object.entries(asset.props)) this.props.set(norm(en), v.ja)
    // 完成行とテンプレート。スロットの無いものは完全一致側へ入れる
    this.patterns = []
    for (const t of [...asset.assembled, ...asset.templates]) {
      const en = norm(t.en)
      if (!/\{[A-Z]/.test(en)) { if (!this.exact.has(en)) this.exact.set(en, t.ja); continue }
      this.patterns.push({ ...compile(en), ja: t.ja, len: en.length })
    }
    // 長い骨格から当てる（短いものが先に食うのを防ぐ）
    this.patterns.sort((a, b) => b.len - a.len)
    this.buf = ''
    this.stats = { hit: 0, miss: 0, missed: new Set() }
  }

  /** 1 語（または名詞句）を日本語へ。無ければそのまま返す */
  word(en) {
    const k = norm(en)
    // ★冠詞を剥がしてから引く（`a leaflet` は `leaflet` で登録されている）。
    //   日本語に冠詞は無いので、剥がすだけで済む
    const bare = k.replace(/^(a|an|the)\s+/i, '')
    return this.props.get(k) || this.exact.get(k) || this.props.get(bare) || this.exact.get(bare) || en
  }

  /** 完成した 1 行を日本語にする。引けなければ null */
  line(raw) {
    // ★プロンプト `>` は改行を伴わずに出るので、次の行の頭に貼りつく。
    //   剥がしてから照合し、訳文の前に戻す（剥がさないと 1 行も引けない）
    const p = raw.match(/^(\s*>+\s*)([\s\S]*)$/)
    if (p && p[2].trim()) {
      const inner = this.line(p[2])
      return inner === null ? null : p[1] + inner
    }
    const key = norm(raw)
    if (!key) return raw
    const hit = this.exact.get(key) ?? this.props.get(key)
    if (hit !== undefined) { this.stats.hit++; return hit }
    for (const p of this.patterns) {
      const m = key.match(p.re)
      if (!m) continue
      let out = p.ja
      p.names.forEach((name, idx) => {
        out = out.replace('{' + name + '}', this.word(m[idx + 1]))
      })
      this.stats.hit++
      return out
    }
    this.stats.miss++
    this.stats.missed.add(key)
    return null
  }

  /**
   * 印字された文字列を食わせる。改行が来た行だけを訳して返す。
   * 返り値は「今すぐ出力してよい文字列」（未完の行は溜めておく）。
   */
  feed(text) {
    this.buf += text
    let out = ''
    let nl
    while ((nl = this.buf.indexOf('\n')) >= 0) {
      const raw = this.buf.slice(0, nl)
      this.buf = this.buf.slice(nl + 1)
      const ja = this.line(raw)
      out += (ja === null ? raw : ja) + '\n'
    }
    return out
  }

  /** 入力待ちの直前など、溜めた分を吐き出したいとき */
  flush() {
    if (!this.buf) return ''
    const raw = this.buf
    this.buf = ''
    const ja = this.line(raw)
    return ja === null ? raw : ja
  }
}

module.exports = { Translator, compile, norm }
