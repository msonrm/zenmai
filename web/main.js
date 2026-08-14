'use strict'
/**
 * ブラウザ側のホスト。自前 Glk シムに DOM を繋ぐだけ。
 * 翻訳は src/translate.js（node の検証と同じコード）を共有している。
 */
;(async () => {
  const $ = (id) => document.getElementById(id)
  const screen = $('screen')
  const { Translator } = window.ZenmaiTranslate
  const { createGlk } = window.GlkShim
  const { createCommander } = window.ZenmaiCommand
  const { createRubifier } = window.ZenmaiRuby

  // ★アセットは差し替わる。ブラウザのキャッシュが残ると、訳文と語彙の版がずれて
  //   「画面には新しい名前が出るのに、その名前で打てない」という混乱が起きる（実プレイで発生）
  const load = async (p) => (await fetch(p, { cache: 'no-store' })).json()
  const asset = await load('../assets/zork1-ja.json')
  const tr = new Translator(asset)
  const rb = createRubifier(asset.ruby)

  // ★ふりがなの入切。切っても組み直さないよう、CSS で隠すだけにする
  if (localStorage.getItem('zenmai-ruby') === 'off') document.body.classList.add('no-ruby')
  // ★デバッグ表示（渡した英語コマンドの行と、訳の成績）。物語ではなく機械の側。
  //   ★既定は**切**。遊ぶ人には要らないものなので、出すのは点検するときだけ
  if (localStorage.getItem('zenmai-debug') !== 'on') document.body.classList.add('no-debug')
  $('debug-btn').addEventListener('click', () => {
    const on = !document.body.classList.toggle('no-debug')
    localStorage.setItem('zenmai-debug', on ? 'on' : 'off')
    $('input').focus()
  })
  $('ruby-btn').addEventListener('click', () => {
    const off = document.body.classList.toggle('no-ruby')
    localStorage.setItem('zenmai-ruby', off ? 'off' : 'on')
    $('input').focus()
  })
  const cm = createCommander(await load('../assets/zork1-cmd.json'))

  // story file は同梱しない（MIT のソースから各自でビルドするか取得する）
  let story = null
  for (const url of ['zork1.z3', '../vendor/zork1/zork1.z3']) {
    try {
      const r = await fetch(url)
      if (r.ok) { story = new Uint8Array(await r.arrayBuffer()); break }
    } catch (e) { /* 次を試す */ }
  }
  if (!story) {
    show('story file が見つからない。`vendor/zork1/zork1.z3` を置くか、ZILF でビルドしてください。', 'raw')
    return
  }

  // 行頭・行末に単独で立つ `>` を落とす（ゲームのプロンプト）
  const strip = (t) => t.replace(/^[ \t]*>+[ \t]*$/gm, '').replace(/(^|\n)[ \t]*>+[ \t]*$/g, '$1')

  let para = null
  let place = ''          // ★いまの場所名。本文の中の「場所名だけの行」を見分けるのに使う
  const fresh = []        // 直前の入力要求から後に足した段落
  function show(text, cls) {
    // 空行で段落を切る。段落の中の改行は活かす（pre-wrap）
    for (const chunk of text.split(/\n{2,}/)) {
      const t = chunk.replace(/^\n+|\n+$/g, '')
      if (!t) { para = null; continue }
      if (!para || cls) {
        para = document.createElement('p')
        if (cls) para.className = cls
        para._raw = ''
        screen.appendChild(para)
        fresh.push(para)
      }
      const p = para
      // 追記のときは行の境目を落とさない。★末尾の改行は残さない（余った空行になる）
      if (p._raw && !p._raw.endsWith('\n')) p._raw += '\n'
      p._raw += t
      // ★英語のまま出す行（未訳・種明かし）にふりがなは要らない
      p.innerHTML = cls === 'raw' || cls === 'sent' ? rb.esc(p._raw) : rb.rubify(p._raw)
      if (cls) para = null
    }
    screen.scrollTop = screen.scrollHeight
  }

  // ★同じ日本語が別の英単語の物を指すことがある（箱 = mailbox / case / chest / trunk）。
  //   原作は 1 語しか受けないので、**順に試す**。
  //   `You can't see any X here!` は**手数を消費しない**ので、試しても盤面は動かない。
  let trial = null        // { alts: [...], buf: '' }
  let rawSince = ''       // 送ってからの英語（判定はここでする。訳文で判定しない）
  const NOT_HERE = /can't see any .* here!/i

  const sink = (t) => { if (trial) trial.buf += t; else show(t) }

  const Glk = createGlk({
    cols: 64,
    rows: 24,
    write(text) {
      rawSince += text
      const out = tr.feed(text)
      if (out) sink(strip(out))
    },
    status(line) {
      // v3 のステータス行は「部屋名 ……… 得点/手数」。部屋名だけ引く
      const m = line.match(/^(.*?)\s{2,}(.*)$/)
      place = m ? tr.word(m[1]) : tr.word(line)
      $('place').innerHTML = rb.rubify(place)
      // ★右側の `Score: 0  Turns: 2` は**原作ではなくインタプリタが書いている**
      //   （v3 の状態行は仕様上インタプリタが描く。Infocom の実機は `Moves` だった）。
      //   つまりここは訳してよい —— というより、ここだけ英語なのは筋が通らない
      $('score').textContent = jaStatus(m ? m[2] : '')
    },
    update() {
      // ★ゲーム自身が入力待ちの前に `>` を印字する。こちらは入力欄に自前の
      //   プロンプトを持っているので、二重に出さないよう落とす
      const rest = strip(tr.flush())
      if (rest.trim()) sink(rest)
      if (trial) {
        if (NOT_HERE.test(rawSince) && trial.alts.length) {
          const next = trial.alts.shift()
          // ★全部外れたときは**最初の返事**を見せる（打っていない名前を出さないため）
          if (trial.first === null) trial.first = trial.buf
          trial.buf = ''
          sent(next, '　……別の物として試す')
          return setTimeout(() => submit(next), 0)
        }
        // ★どれも無かった。**打った言葉**で断る（第一候補の名前を出さない）
        const t = NOT_HERE.test(rawSince) && trial.first !== null
          ? `${trial.disp}など、ここには見当たらない。\n` : trial.buf
        trial = null
        if (t.trim()) show(t)
      }
      // ★場所名の行を段落の頭として切り出す。**状態行は入力要求のときに来る**ので、
      //   書いた時点ではまだ場所が分からない。ここまで待ってから切る
      for (const p of fresh.splice(0)) {
        if (p.className || !place || !p._raw) continue
        if (p._raw !== place && !p._raw.startsWith(place + '\n')) continue
        const head = document.createElement('p')
        head.className = 'room'; head._raw = place; head.innerHTML = rb.rubify(place)
        screen.insertBefore(head, p)
        p._raw = p._raw.slice(place.length).replace(/^\n+/, '')
        if (p._raw) p.innerHTML = rb.rubify(p._raw)
        else p.remove()
      }
      $('stat').textContent = ` 引けた ${tr.stats.hit} 行 / 未訳 ${tr.stats.miss} 行`
      $('input').disabled = Glk.waitingFor() !== 'line'
      if (Glk.waitingFor() === 'char') $('input').placeholder = '何かキーを（Enter で進む）'
      else $('input').placeholder = 'ひらがなで打つ（例: ゆうびんばこをあける）'
      $('input').focus()
    },
  })

  $('input').addEventListener('keydown', (e) => {
    if (e.key !== 'Enter') return
    const text = $('input').value
    $('input').value = ''
    if (Glk.waitingFor() === 'char') { Glk.submitChar(32); return }
    // ★日本語で打たれたら英語コマンドへ翻訳する。英語ならそのまま通す
    const r = cm.toCommand(text)
    show(text || ' ', 'cmd')
    if (!r.command) {
      const w = r.unknown.filter((x) => x.length > 1).map((x) => '「' + x + '」').join('・')
      show(r.note ? `（${r.note}）`
        : r.trace === '否定は扱えない' ? '（打ち消しの言い方はまだ扱えない）'
        : w ? `（${w} は知らない言葉。別の言い方を試してほしい）`
        : '（読み取れなかった）', 'raw')
      return
    }
    if (r.trace !== '英語のまま') sent(r.command, r.unknown.length ? '　※残: ' + r.unknown.join(' ') : '')
    trial = r.alts && r.alts.length ? { alts: r.alts.slice(), buf: '', first: null, disp: r.objDisp } : null
    submit(r.command)
  })

  function sent(cmd, note) {
    const p = document.createElement('p')
    p.className = 'sent'; p.textContent = cmd + (note || '')
    screen.appendChild(p)
  }
  function submit(cmd) { rawSince = ''; Glk.submitLine(cmd) }

  function jaStatus(rhs) {
    const s = rhs.match(/Score:\s*(-?\d+)\s+Turns:\s*(\d+)/)
    if (s) return `得点 ${s[1]}　手数 ${s[2]}`
    const t = rhs.match(/Time:\s*(\d+):(\d+)\s*([AP]M)/)
    if (t) return `${t[3] === 'PM' ? '午後' : '午前'} ${t[1]}時${t[2]}分`
    return rhs
  }

  const vm = new window.ZVM()
  vm.prepare(story, { vm, Glk, GlkOte: null, Dialog: null })
  Glk.init({ vm })

  function show2() {}   // （予約）ゲームパッド入力はここに繋ぐ
})()
