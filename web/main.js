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
  let pendingVerb = null  // 原作が聞き返している最中の動詞
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
    // ★セーブの置き場。localStorage に base64 で 1 枠だけ持つ
    //   （枠を選ばせる画面を出すとコントローラだけでは操作できない）
    files: {
      read(name) {
        const b64 = localStorage.getItem(name)
        if (!b64) return null
        const bin = atob(b64)
        const out = new Uint8Array(bin.length)
        for (let i = 0; i < bin.length; i++) out[i] = bin.charCodeAt(i)
        return out
      },
      write(name, bytes) {
        let bin = ''
        for (const b of bytes) bin += String.fromCharCode(b)
        try { localStorage.setItem(name, btoa(bin)) } catch (e) { /* 容量超過は諦める */ }
      },
    },
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
    send($('input').value)
  })

  // ★1 行を送る道は 1 本にする（キーボードもゲームパッドもここを通る）
  function send(text) {
    if (Glk.waitingFor() === 'char') { $('input').value = ''; Glk.submitChar(32); return }
    // ★空は送らない。R🕹↓ を握っていると送信が連発し、空コマンドが並んでいた
    if (!String(text).trim()) { $('input').value = ''; return }
    $('input').value = ''
    // ★日本語で打たれたら英語コマンドへ翻訳する。英語ならそのまま通す
    // ★原作が「何を◯◯？」と聞き返している最中は、動詞がこちらの手元にある
    const r = cm.toCommand(text, { verb: pendingVerb })
    show(text || ' ', 'cmd')
    if (!r.command) {
      const w = r.unknown.filter((x) => x.length > 1).map((x) => '「' + x + '」').join('・')
      show(r.note ? `（${r.note}）`
        : r.trace === '否定は扱えない' ? '（打ち消しの言い方はまだ扱えない）'
        : w ? `（${w} は知らない言葉。別の言い方を試してほしい）`
        : '（読み取れなかった）', 'raw')
      return
    }
    // ★目的語が要る動詞なのに無いときは、原作に聞き返させずこちらで訊く
    if (r.needsObject) {
      pendingVerb = r.verbKey
      show(r.ask, 'raw')
      return
    }
    if (r.trace !== '英語のまま') sent(r.command, r.unknown.length ? '　※残: ' + r.unknown.join(' ') : '')
    trial = r.alts && r.alts.length ? { alts: r.alts.slice(), buf: '', first: null, disp: r.objDisp } : null
    // 送ったのが**動詞だけ**なら、次の入力は聞き返しへの答えとみなす
    pendingVerb = null
    submit(r.command)
  }

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

  // ================= ゲームパッド =================
  // ★labo の GamepadEngine（v1.7.0）をそのまま使い、**ホスト側で op を絞る**。
  //   ここは変換をしない（ひらがなをそのまま送る）ので、要らない操作を落とすだけで足りる。
  const ROW_KANA = ['あ', 'か', 'さ', 'た', 'な', 'は', 'ま', 'や', 'ら', 'わ']
  // ★この企画で要らない文字 —— 句読点・括弧・疑問符。原作のパーサは受け取らない
  const DROP = new Set(['、', '。', '「', '」', '？', ' ', '　'])

  function padInsert(text, replace) {
    const el = $('input')
    const at = el.selectionStart == null ? el.value.length : el.selectionStart
    const cut = Math.max(0, at - (replace || 0))
    el.value = el.value.slice(0, cut) + text + el.value.slice(at)
    const pos = cut + text.length
    el.setSelectionRange(pos, pos)
  }

  function padKey(key) {
    const el = $('input')
    const at = el.selectionStart == null ? el.value.length : el.selectionStart
    if (key === 'Enter') { send(el.value); return }
    if (key === 'Backspace') { padInsert('', 1); return }
    if (key === 'Escape') { el.value = ''; return }
    // ★左スティックの上下は**本文送り**。コマンド欄は 1 行なので上下が空いている
    if (key === 'ArrowUp' || key === 'ArrowDown' || key === ' ') {
      const d = key === 'ArrowUp' ? -1 : 1
      screen.scrollTop += d * Math.max(80, (screen.clientHeight || 0) * 0.35)
      return
    }
    // ★左右はキャレット移動だけ（変換が無いので候補送り・文節移動は要らない）
    if (key === 'ArrowLeft') { el.setSelectionRange(Math.max(0, at - 1), Math.max(0, at - 1)); return }
    if (key === 'ArrowRight') {
      const p = Math.min(el.value.length, at + 1)
      el.setSelectionRange(p, p)
    }
  }

  // ★操作図は labo のものをそのまま使い、**この企画で変えた所だけ札を書き換える**
  //   （エンジンには手を入れない。図と実際の挙動がずれるほうが害が大きい）
  function patchPad(root) {
    // ★使わない操作と、見なくても分かる札は出さない
    //   （押し込み＝取消/確定・機種名・「日本語」のモード札）
    const SWAP = { '、。␣': '送信', '「': '', '」': '', '？': '', '取消': '' }
    for (const el of root.querySelectorAll('*')) {
      if (el.children.length) continue
      const t = (el.textContent || '').trim()
      if (t in SWAP) el.textContent = SWAP[t]
      if (t === '日本語' && el.remove) el.remove()
    }
    const name = root.querySelector('.ge-name')
    if (name && name.remove) name.remove()
    const guide = root.querySelector('.ge-guide')
    if (!guide) return
    guide.textContent = ''
    for (const g of ['LT 拗音/っ', 'LT+RT っ', 'RT ん', 'R🕹↓ 送信', 'R🕹↑ ゛゜',
      'R🕹→ ー', 'R🕹← 1字消す', 'L🕹←→ カーソル', 'L🕹↑↓ 画面送り']) {
      const sp = document.createElement('span')
      sp.textContent = g
      guide.appendChild(sp)
    }
  }

  if (window.GamepadEngine && typeof navigator !== 'undefined' && navigator.getGamepads) {
    let vis = null
    const padOpen = () => document.body.classList.contains('pad-open')
    const showPad = (on) => {
      document.body.classList.toggle('pad-open', on)
      localStorage.setItem('zenmai-pad', on ? 'on' : 'off')
      if (on && !vis) { vis = window.GamepadEngine.mount($('pad')); patchPad($('pad')) }
      if (!on && vis) { vis.destroy(); vis = null }
    }
    $('pad-btn').addEventListener('click', () => { showPad(!padOpen()); $('input').focus() })

    window.GamepadEngine.start({
      // ★濁点（R🕹↑）と拗音（LT）は**末尾の文字を見て置換**を決める。
      //   ここを常に空で返していたので、どちらも何も起きなかった（実機で判明）
      getComposingTail: () => {
        const el = $('input')
        const at = el.selectionStart == null ? el.value.length : el.selectionStart
        return el.value.slice(0, at)
      },
      onOp(op) {
        if (op.type === 'kana') {
          // ★R🕹↓ は句読点だった。ここでは**コマンド送信**に使う（要らない記号は落とす）
          if (op.text === '、') { send($('input').value); return }
          if (DROP.has(op.text)) return
          padInsert(op.text, op.replace || 0)
          return
        }
        if (op.type === 'key' && op.tap) padKey(op.tap.key)
      },
      onState(st) {
        document.body.classList.toggle('pad-on', !!st.connected)
        if (st.connected && localStorage.getItem('zenmai-pad') !== 'off' && !padOpen()) showPad(true)
        const row = ROW_KANA[st.activeRow] || 'あ'
        $('pad-btn').textContent = st.previewChar || row
        if (vis) vis.update(st)
      },
    })
  }

  const vm = new window.ZVM()
  vm.prepare(story, { vm, Glk, GlkOte: null, Dialog: null })
  Glk.init({ vm })

  function show2() {}   // （予約）ゲームパッド入力はここに繋ぐ
})()
