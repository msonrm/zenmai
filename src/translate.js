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
  const quotes = []      // ★引用符の中は**打った語そのもの**。訳してはいけない
  let re = ''
  let i = 0
  for (const m of en.matchAll(/\{([A-Z0-9,?!-]+)\}/g)) {
    re += escapeRe(en.slice(i, m.index))
    names.push(m[1].replace(/,$/, ''))
    // ★スロットは文をまたげない。`[\\s\\S]` だと `A {OBJ}` のような緩い骨格が行全体を食う
    //   （`A path leads into the forest to the east.` が丸ごとスロットに入って `A ` が消えた）
    // ただし**引用符で囲まれたスロット**は別 —— 未知語がそのまま入るので記号もありうる
    //   （`I don't know the word "!)".` が外れていた）
    const quoted = en[m.index - 1] === '"' && en[m.index + m[0].length] === '"'
    quotes.push(quoted)
    re += quoted ? '([^"]*)' : '([^.!?]+?)'
    i = m.index + m[0].length
  }
  re += escapeRe(en.slice(i))
  return { re: new RegExp('^' + re + '$'), names, quotes }
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
      // 具体性（スロットを除いた字数）が高いものから当てる
      this.patterns.push({ ...compile(en), ja: t.ja, len: en.replace(/\{[^}]*\}/g, '').length })
    }
    // 長い骨格から当てる（短いものが先に食うのを防ぐ）
    this.patterns.sort((a, b) => b.len - a.len)
    // ★ZIL は文字列の中の改行を `|` で表す。実行時は本物の改行として届くので、
    //   `|` を境に英日を対で割って登録する（読み物が行単位で引けるようになる）
    for (const [en, ja] of [...this.exact, ...this.props]) {
      if (!en.includes('|')) continue
      const e = en.split('|').map((x) => x.trim())
      const j = ja.split('|').map((x) => x.trim())
      if (e.length !== j.length) continue
      for (let i = 0; i < e.length; i++) if (e[i] && j[i] && !this.exact.has(e[i])) this.exact.set(e[i], j[i])
    }
    // 版権表示など「訳さない行」は miss と分けて数える。★こちらも `|` で割る
    this.notrans = new Set()
    for (const en of asset.notrans || []) {
      this.notrans.add(norm(en))
      for (const part of en.split('|')) if (norm(part)) this.notrans.add(norm(part))
    }
    // ★パーサが**プレイヤーの打った語をそのまま復唱する**ときの語訳
    //   （`You can't see any window here!` の window は DESC ではなく辞書の裸の名詞）
    this.words = new Map(Object.entries(asset.words || {}))
    this.buf = ''
    // ★rawWords = **スロットに英語が残った**もの。行としては引けているので
    //   miss には出ない（実プレイで `a clove of garlic, and a lunch` が英語のまま出た）
    this.stats = { hit: 0, greedy: 0, miss: 0, notrans: 0, missed: new Set(), rawWords: new Set() }
  }

  /** 1 語（または名詞句）を日本語へ。無ければ null */
  one(en) {
    const k = norm(en)
    // ★冠詞を剥がしてから引く（`a leaflet` は `leaflet` で登録されている）。
    //   日本語に冠詞は無いので、剥がすだけで済む
    const bare = k.replace(/^(a|an|the)\s+/i, '')
    return this.props.get(k) ?? this.exact.get(k) ?? this.words.get(k)
      ?? this.props.get(bare) ?? this.exact.get(bare) ?? this.words.get(bare) ?? null
  }

  /** 1 語（または名詞句・**並び**）を日本語へ。無ければそのまま返す */
  word(en) {
    const hit = this.one(en)
    if (hit !== null) return hit
    // ★前置詞つきの句が入ることがある（`(with the sword)` ＝ パーサが道具を補った復唱）。
    //   日本語では**助詞が後ろに付く**ので、訳語の後ろに回すだけで通る
    const pp = String(en).match(/^(with|in|on|at|to|from|under|behind|over|through|around)\s+(.+)$/i)
    if (pp) {
      const obj = this.one(pp[2])
      const par = this.words.get(pp[1].toLowerCase())
      if (obj !== null && par) return obj + par
    }
    // ★スロットには**並び**が入ることがある（`a clove of garlic, and a lunch`）。
    //   全部引けたときだけ「と」で繋ぐ。1 つでも引けなければ諦める（半分英語より、全部英語）
    const parts = String(en).split(/,\s*and\s+|\s+and\s+|,\s*/).filter((x) => x.trim())
    if (parts.length > 1) {
      const ja = parts.map((x) => this.one(x))
      if (ja.every((x) => x !== null)) return ja.join('と')
    }
    // ★引けなかった。行としては引けているので miss には出ない —— ここで数える
    // ★どの骨格に当てた結果かも残す —— 屑がスロットに入るのは**当て違い**の徴候
    if (/[A-Za-z]/.test(String(en))) this.stats.rawWords.add(norm(en) + (this._curKey ? `　←　${this._curKey}` : ''))
    return en
  }

  /** 1 単位（1 文 or 数文のまとまり）を引く。引けなければ null。統計は数えない */
  lookup(key) {
    if (!key) return null
    this._curKey = key
    const hit = this.exact.get(key) ?? this.props.get(key)
    if (hit !== undefined) return hit
    for (const p of this.patterns) {
      const m = key.match(p.re)
      if (!m) continue
      let out = p.ja
      p.names.forEach((name, idx) => {
        // ★引用された語（`I don't know the word "X".`）は**打った語そのもの**なので訳さない
        const v = p.quotes[idx] ? m[idx + 1] : this.word(m[idx + 1])
        out = out.replace('{' + name + '}', v)
      })
      return out
    }
    return null
  }

  /**
   * ★行の中を前方から貪欲に食う。
   *
   * 1 行に複数の完成文が並ぶことがある（`It is pitch black. You are likely to be eaten by a grue.`）。
   * こちらは 2 文を別々に持っているので、行単位の完全一致では引けない。
   * 逆に、複数文が 1 件として登録されているもの（グルーの説明 3 文）もあるため、
   * **長いまとまりから順に試して、当たったら次へ進む**（逐次入力の配列エンジンと同じ最長一致）。
   */
  greedy(key) {
    const parts = key.match(/[^.!?]*[.!?]+["')\]]*\s*|[^.!?]+$/g)
    if (!parts || parts.length < 2) return null
    const out = []
    let i = 0, hit = false
    while (i < parts.length) {
      let ja = null, span = 0
      for (let j = parts.length; j > i; j--) {
        const cand = norm(parts.slice(i, j).join(''))
        const t = this.lookup(cand)
        if (t !== null) { ja = t; span = j - i; break }
      }
      if (ja === null) { out.push(parts[i].trim()); i++ }
      else { out.push(ja); hit = true; i += span }
    }
    return hit ? out.join('') : null
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
    // ★プロンプトは**行の後ろにも付く**。質問（`… (Y is affirmative): >`）は改行を伴わずに
    //   出るので、次のプロンプトが同じ行に貼りつく。剥がしてから照合する
    const q = raw.match(/^([\s\S]*?)(\s*>+\s*)$/)
    if (q && q[1].trim()) {
      const inner = this.line(q[1])
      return inner === null ? null : inner + q[2]
    }
    const key = norm(raw)
    if (!key || /^>+$/.test(key)) return raw   // プロンプトだけの行は素通し
    // ★行頭の字下げは**入れ子を見せる意味を持つ**（持ち物の中身は 2 字ずつ深くなる）。
    //   照合では空白を潰すので、訳文の前に戻してやらないと入れ子が消える
    //   ★字下げは**全角に置き換える**。原作は 1 段 2 字だが、半角のままだと
    //   日本語の字面の中で浅く見えて入れ子が読み取れない
    const sp = (raw.match(/^[ \t]*/) || [''])[0].length
    const indent = '　'.repeat(Math.round(sp / 2))
    const whole = this.lookup(key)
    if (whole !== null) { this.stats.hit++; return indent + whole }
    const g = this.greedy(key)
    if (g !== null) { this.stats.hit++; this.stats.greedy++; return indent + g }
    // ★版権バナーは実行時に連結されて出る（`Copyright (c) 1981, … Infocom, Inc. All rights reserved.`）
    //   ので、完全一致では拾えない。包含関係で判定する（相手は定型の版権表示だけなので安全）
    if (this.notrans.has(key) || [...this.notrans].some((n) => n.includes(key) || key.includes(n))) {
      this.stats.notrans++
      return null
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

{
  const api = { Translator, compile, norm }
  if (typeof module !== 'undefined' && module.exports) module.exports = api
  if (typeof window !== 'undefined') window.ZenmaiTranslate = api
}
