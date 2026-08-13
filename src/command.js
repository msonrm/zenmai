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
// ★読みも要る。無いと「きたへいく」の「き」が木に食いつく（最長一致は
//   両方に読みがあって初めて守れる）
const DIRS = {
  北東: 'northeast', ほくとう: 'northeast', 北西: 'northwest', ほくせい: 'northwest',
  南東: 'southeast', なんとう: 'southeast', 南西: 'southwest', なんせい: 'southwest',
  北: 'north', きた: 'north', 南: 'south', みなみ: 'south',
  東: 'east', ひがし: 'east', 西: 'west', にし: 'west',
  上: 'up', うえ: 'up', 下: 'down', した: 'down', 中: 'in', なか: 'in', 外: 'out', そと: 'out',
}
// 助詞 → 役。長いものから当てる
const PARTICLES = [
  ['の中に', 'IN'], ['のなかに', 'IN'], ['の中へ', 'IN'], ['のなかへ', 'IN'],
  ['の下', 'UNDER'], ['のした', 'UNDER'], ['の後ろ', 'BEHIND'], ['のうしろ', 'BEHIND'],
  ['の裏', 'BEHIND'], ['のうら', 'BEHIND'],
  ['の上に', 'ON'], ['のうえに', 'ON'], ['の上', 'ON'], ['のうえ', 'ON'],
  ['を使って', 'WITH'], ['をつかって', 'WITH'], ['によって', 'WITH'],
  // ★「には」は入れない —— 「なかにはいる」の「はいる」の頭を食う
  ['から', 'FROM'], ['へ', 'TO'], ['に', 'TO'], ['で', 'WITH'],
  ['を', 'O'], ['は', 'O'], ['が', 'O'], ['と', 'AND'],
  // ★連体修飾の「の」（家の扉 / 家のドア）。**上の の〜 より必ず後ろに置く**
  //   —— 先に来ると「絨毯の下」の「のした」を食う
  ['の', 'MOD'],
]

// 空間の関係を表す役（捨てると意味が変わる）
const SPATIAL = new Set(['IN', 'ON', 'UNDER', 'BEHIND', 'FROM'])
const ROLE_JA = { IN: 'の中', ON: 'の上', UNDER: 'の下', BEHIND: 'の後ろ', FROM: 'から', WITH: 'で' }

const strip = (s) => s.replace(/[。、．，！？\s]+/g, '')

/**
 * ★照合はすべて「かな」に正規化してから行う。
 * コントローラ入力はかなしか出さないので、`じゅうたん` でも `絨毯` でも当たる必要がある。
 * 表示は語彙項目の代表形（漢字）を使うので、打鍵がかなでも画面は漢字になる。
 */
const kana = (s) => String(s)
  .replace(/[\u30a1-\u30f6]/g, (c) => String.fromCharCode(c.charCodeAt(0) - 0x60))  // カタカナ→ひらがな
  .replace(/[\uff01-\uff5e]/g, (c) => String.fromCharCode(c.charCodeAt(0) - 0xfee0)) // 全角英数→半角
  .replace(/[ー－—]/g, 'ー')
  .toLowerCase()

// 否定を捨てると逆の命令になる。★黙って間違うより止める
const NEGATIVE = /(ない|ないで|ぬ|ません|なかった)$/

function createCommander(asset) {
  // 語彙をひとつの表に畳んで、長いものから当てる（逐次入力の配列エンジンと同じ最長一致）
  const lex = []
  for (const [key, v] of Object.entries(asset.verbs || {})) {
    for (const ja of v.ja) lex.push({ ja, disp: v.ja[0], kana: kana(ja), kind: 'verb', key, shapes: v.shapes })
  }
  // ★同じ英単語を複数の物が共有するとき、どちらを指すかは**部屋によって決まる**。
  //   原作はそれを持っている（`open door` → 「木の扉と揚げ戸の、どちらのことか。」）ので、
  //   こちらが先回りして形容詞を足すと、その場に無い物を名指しして外れる
  //   （実プレイ: 家の西で「窓を覗く」→ `look in kitchen window` → 「台所の窓など、ここには…」）。
  //
  //   ★判定は**日本語の側**でする —— その言い方が日本語でも曖昧なら曖昧なまま渡し、
  //   日本語で一意なら（「揚げ戸」）形容詞を添えて聞き返しを省く。
  const owners = {}
  for (const [key, o] of Object.entries(asset.objects || {})) {
    for (const ja of o.ja) (owners[kana(ja)] = owners[kana(ja)] || []).push(key)
  }
  const objOf = asset.objects || {}
  // ★英語の名詞を複数の物が共有するときだけ形容詞を添える。
  //   共有していないなら裸で渡す（`move large rug` ではなく `move rug`）
  const nounUse = {}
  for (const o of Object.values(objOf)) {
    const n = (o.nouns[0] || '').toLowerCase()
    nounUse[n] = (nounUse[n] || 0) + 1
  }
  const seen = new Set()
  // ★曖昧な言い方でも画面には漢字を出したい（打鍵はかなだから）。
  //   同じ英単語に寄る言い方のうち、**漢字を含む最短の形**を代表にする（まど → 窓 / とびら → 扉）
  const sharedDisp = {}
  for (const [key, o] of Object.entries(objOf)) {
    const noun = (o.nouns[0] || '').toLowerCase()
    for (const ja of o.ja) {
      const own = owners[kana(ja)] || []
      if (own.length < 2 || !own.every((x) => (objOf[x].nouns[0] || '').toLowerCase() === noun)) continue
      if (!/[一-龥]/.test(ja)) continue
      if (!sharedDisp[noun] || ja.length < sharedDisp[noun].length) sharedDisp[noun] = ja
    }
  }
  for (const [key, o] of Object.entries(objOf)) {
    const noun = (o.nouns[0] || '').toLowerCase()
    const adj = (o.adjs[0] || '').toLowerCase()
    for (const ja of o.ja) {
      const k = kana(ja)
      const own = owners[k] || [key]
      // 複数の物が同じ言い方を名乗り、しかも英語の名詞が同じ → 原作に判断を返す
      const shared = own.length > 1 && own.every((x) => (objOf[x].nouns[0] || '').toLowerCase() === noun)
      if (shared && seen.has(k)) continue          // 同じ語を二重に積まない
      if (shared) seen.add(k)
      lex.push({
        ja,
        disp: shared ? (sharedDisp[noun] || ja) : o.ja[0],
        kana: k,
        kind: 'obj',
        key,
        word: shared || !(nounUse[noun] > 1 && adj) ? noun : adj + ' ' + noun,
        vehicle: !!o.vehicle,
      })
    }
  }
  for (const [ja, en] of Object.entries(DIRS)) lex.push({ ja, disp: ja, kana: kana(ja), kind: 'dir', word: en })
  lex.sort((a, b) => b.kana.length - a.kana.length)

  /** @returns {{command:string|null, trace:string, unknown:string[]}} */
  function toCommand(input) {
    const raw = strip(String(input || ''))
    if (!raw) return { command: null, trace: '空', unknown: [], echo: '' }
    if (/^[\x20-\x7e]+$/.test(input.trim())) {
      return { command: input.trim(), trace: '英語のまま', unknown: [], echo: input.trim() }
    }
    if (NEGATIVE.test(raw)) {
      // ★「扉を開けない」の「ない」を捨てると逆の命令になる。止める
      return { command: null, trace: '否定は扱えない', unknown: [], echo: raw }
    }
    const s = kana(raw)
    const found = []          // { kind, key, word, role }
    const unknown = []
    let hardStop = false      // ★未知語が目的語の位置に立っていたら送らない
    const echo = []           // ★打鍵がかなでも、画面には代表形（漢字）で見せる
    let i = 0
    while (i < s.length) {
      const hit = lex.find((e) => s.startsWith(e.kana, i))
      if (hit) {
        i += hit.kana.length
        echo.push(hit.disp)
        // 直後の助詞を読む（役を決める）
        let role = null
        for (const [p, r] of PARTICLES) {
          if (!s.startsWith(kana(p), i)) continue
          role = r; i += p.length; echo.push(p)
          // ★「の後ろに」「の中へ」—— 空間の助詞は方向の「に・へ」を伴うことがある
          if (SPATIAL.has(r) && (s[i] === 'に' || s[i] === 'へ')) { echo.push(s[i]); i++ }
          break
        }
        found.push({ ...hit, role })
        continue
      }
      // 語彙にない部分は読み飛ばす（何が落ちたかは残す）
      let j = i + 1
      while (j < s.length && !lex.some((e) => s.startsWith(e.kana, j))) j++
      let word = s.slice(i, j)
      // ★未知語が格助詞を背負っていたら、それは「物のつもり」だ。捨ててはいけない。
      //   動詞だけ送るとゲームが**別の物を勝手に補う**ことがあり、たまたま正しく
      //   見えてしまう（「どあをあける」が `open` だけになり、原作が (door) を補った）。
      //   助詞は語彙に無いので未知語の側に飲まれる。だから末尾で判定する。
      //   を/は/が に限る —— に・で は副詞の末尾（しずかに・いそいで）と紛れる
      //   ★助詞だけが余ることがある（「絨毯の下を見る」の「を」= 役はもう決まっている）。
      //   そのときは未知語ではないので、止めない
      const mk = word.match(/(を|は|が)$/)
      if (mk) word = word.slice(0, -1)
      if (word) { unknown.push(word); if (mk) hardStop = true }
      echo.push(s.slice(i, j))
      i = j
    }
    if (hardStop) return { command: null, trace: '知らない言葉', unknown, echo: echo.join('') }
    let verb = found.find((f) => f.kind === 'verb')
    const dirs = found.filter((f) => f.kind === 'dir')
    const objs = found.filter((f) => f.kind === 'obj')

    // 方角だけ（「北」「北へ行く」）→ 方角の語をそのまま渡す
    // ★「下へ降りる」「上へ登る」も方角の言い方。原作の CLIMB / DISEMBARK は
    //   **物に対する動詞**（梯子を降りる = `disembark ladder`）なので、
    //   物を伴わずに方角だけが立っているときに限って方角へ寄せる
    const MOVE_DIR = ['CLIMB', 'DISEMBARK']
    if (dirs.length && (!verb || verb.key === 'WALK' || (MOVE_DIR.includes(verb.key) && !objs.length))) {
      return { command: dirs[0].word, trace: '方角', unknown, echo: echo.join('') }
    }
    // 「中に入る」「外に出る」の中／外は方角ではなく動詞のほうに含まれている
    const bareDirs = dirs.filter((d) => !(verb && ['ENTER', 'EXIT', 'LEAVE'].includes(verb.key) && ['in', 'out'].includes(d.word)))
    if (!verb) {
      // ★知らない言葉が混じっているなら、それを言う。
      //   黙って名詞だけ送ると原作が「その文には動詞がない」と返し、
      //   **打った本人は動詞を打っているので誤解を招く**（今日の方針: 黙って外すより言う）
      if (unknown.length) return { command: null, trace: '知らない言葉', unknown, echo: echo.join('') }
      // 名詞だけ（「絨毯」）→ 原作の「何を？」に任せる
      if (objs.length) return { command: objs[0].word, trace: '名詞のみ', unknown, echo: echo.join('') }
      return { command: null, trace: '動詞が見つからない', unknown, echo: echo.join('') }
    }

    // ★「家の扉」の「家」は目的語ではなく**修飾語**。日本語は修飾語が前に来るので、
    //   これを外さないと先に見つかったほう（家）が目的語になる。
    //   ただし後ろに名詞が続かないなら（「家の」で終わる）ただの言い落としとして残す。
    //   ★「木のドア」のように**語彙に丸ごと載っている複合語は最長一致が先に取る**ので、
    //   ここへは来ない = 落ちるのは登録されていない組み合わせだけ
    const heads = objs.filter((o, k) => !(o.role === 'MOD' && objs[k + 1]))
    objs.length = 0
    objs.push(...heads)

    // ★役の割り当て: を/は/が → 直接目的語、で/を使って → 道具、に/へ/の中に… → 第 2 目的語
    //   ★助詞で明示されたほうを優先する（裸の名詞より「を」が強い）
    let prso = objs.find((o) => o.role === 'O') || objs.find((o) => o.role === null) || null
    const tool = objs.find((o) => o.role === 'WITH')
    const dest = objs.find((o) => ['TO', 'IN', 'ON', 'UNDER', 'BEHIND', 'FROM'].includes(o.role))
    if (!prso && dest && objs.length === 1) { prso = dest }

    // ★同じ日本語の動詞でも、**対象が英語の動詞を決める**。
    //   「降りる」は乗り物なら `disembark`、そうでなければ `climb down`
    //   （実プレイ: 木の上で「きをおりる」→ `disembark tree` → 「それには乗っていない。」）
    if (verb.key === 'DISEMBARK' && prso && !prso.vehicle) {
      verb = { ...verb, key: 'CLIMB', shapes: (asset.verbs.CLIMB || {}).shapes || [], fixed: 'DOWN' }
    }

    const out = [verb.key.toLowerCase()]
    const shapes = verb.shapes || []
    const has = (p) => shapes.some((sh) => sh.split(' ').includes(p))
    if (verb.fixed && has(verb.fixed)) out.push(verb.fixed.toLowerCase())
    // ★前置詞つきの目的語が 1 つだけでも、その前置詞を動詞が取らないなら**裸で渡す**。
    //   「木に登る」の「に」は原作の CLIMB には無い形（`climb tree` が正しい）
    // ★動詞がその関係を取れないなら、**黙って落とさず断る**。
    //   落とすと「家の後ろに行く」が `walk house` になり、別の意味の命令が飛ぶ。
    //   「に・へ」だけは例外 —— 日本語では目的語をただ指すことがある（木に登る = climb tree）
    for (const o of objs) {
      if (!SPATIAL.has(o.role) || has(o.role)) continue
      const hint = ['WALK', 'ENTER', 'EXIT'].includes(verb.key) ? ' —— 移動は方角で言う: 北・東・上・下' : ''
      return {
        command: null,
        trace: '原作にない言い方',
        note: `「${o.disp}${ROLE_JA[o.role]}」を「${verb.disp}」では受けられない${hint}`,
        unknown,
        echo: echo.join(''),
      }
    }
    if (tool && !has('WITH')) {
      return {
        command: null,
        trace: '原作にない言い方',
        note: `「${verb.disp}」に道具（${tool.disp}で）は付けられない`,
        unknown,
        echo: echo.join(''),
      }
    }

    if (prso && !(dest === prso && has(dest.role))) {
      // ★裸の目的語を取らない動詞（`look` など）には、構文表から前置詞を補う。
      //   `look case` は原作の構文に無く「その文は知らない形だ」で弾かれる
      // ★（動詞単独）は「裸の目的語が使える」ことを意味しない。混同していた
      const bareOk = shapes.some((sh) => sh === 'OBJ' || sh.startsWith('OBJ '))
      if (!bareOk) {
        const wants = /中|なか|のぞ/.test(verb.ja) ? ['IN', 'AT'] : ['AT', 'IN', 'ON', 'UNDER', 'TO']
        const p = wants.find((x) => has(x))
        if (p) out.push(p.toLowerCase())
      }
      out.push(prso.word)
    }
    if (tool) out.push('with', tool.word)
    if (dest && dest !== prso) {
      const p = has(dest.role) ? dest.role.toLowerCase() : (has('IN') ? 'in' : 'to')
      out.push(p, dest.word)
    } else if (dest && dest === prso && has(dest.role)) {
      // 「絨毯の下を見る」= 前置詞つきの目的語が 1 つだけ → `look under rug`
      out.length = 1
      out.push(dest.role.toLowerCase(), dest.word)
    }
    if (!prso && !tool && !dest && bareDirs.length) out.push(bareDirs[0].word)
    return { command: out.join(' '), trace: '動詞+役', unknown, echo: echo.join('') }
  }

  return { toCommand, lex }
}

{
  const api = { createCommander, DIRS, PARTICLES }
  if (typeof module !== 'undefined' && module.exports) module.exports = api
  if (typeof window !== 'undefined') window.ZenmaiCommand = api
}
