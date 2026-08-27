'use strict'
/**
 * ブラウザ側のホスト。自前 Glk シムに DOM を繋ぐだけ。
 * 翻訳は src/translate.js（node の検証と同じコード）を共有している。
 */
;(async () => {
  const $ = (id) => document.getElementById(id)
  // ★触る画面で盤を閉じているときは**入力欄に焦点を当てない** ——
  //   毎手 focus していたので、OS のキーボードが勝手に出てきた（実機の指摘）
  const touchDevice = () => typeof navigator !== 'undefined' && (navigator.maxTouchPoints || 0) > 0
  const refocus = () => {
    // ★手引き・案内を開いているあいだは焦点を奪わない
    //   （チェックを押した先から取り返してしまう／触る画面では OS のキーボードが出る）
    const p = $('panel')
    if (p && !p.hidden) return
    const i = $('intro')
    if (i && !i.hidden) return
    const h = $('pad-help')
    if (h && !h.hidden) return
    if (touchDevice() && !document.body.classList.contains('flick-open')) return
    $('input').focus()
  }
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
  // ★デバッグ表示（渡した英語コマンドの行）。物語ではなく機械の側。
  //   ★既定は**切**。遊ぶ人には要らないものなので、出すのは点検するときだけ
  if (localStorage.getItem('zenmai-debug') !== 'on') document.body.classList.add('no-debug')
  const cmdAsset = await load('../assets/zork1-cmd.json')
  const cm = createCommander(cmdAsset)

  // ================= 本文の言語 =================
  // ★英語版は「訳を足したもの」ではなく**訳すのをやめたもの** —— 原作がそのまま出る。
  //   だから *guess the verb* の問題もそのまま戻ってくる（そこも含めて原作の姿）。
  //   既定はブラウザの言語。入口の案内と設定から変えられる
  const browserJa = String((typeof navigator !== 'undefined' && navigator.language) || 'ja')
    .toLowerCase().startsWith('ja')
  let bodyLang = localStorage.getItem('zenmai-body-lang')
  if (bodyLang !== 'english' && bodyLang !== 'japanese') bodyLang = browserJa ? 'japanese' : 'english'
  // ★コントローラの言語は**本文と同じで始まり**、盤の札で切り替える。
  //   設定に置くのはやめた（盤で選べるものを 2 か所に置くと、どちらが正か分からなくなる）。
  //   覚えさせないので、開き直せばまた本文に揃う
  let padLang = bodyLang
  // ★英語には漢字が無いので、ふりがなは掛けない
  const ruby = (s) => (bodyLang === 'english' ? rb.esc(s) : rb.rubify(s))
  const applyBodyLang = () => {
    tr.setPassthrough(bodyLang === 'english')
    document.body.classList.toggle('body-en', bodyLang === 'english')
    // ★本文を選んだら、コントローラもそれで始める
    setPadLangEffective(bodyLang)
  }
  const setBodyLang = (lang) => {
    bodyLang = lang === 'english' ? 'english' : 'japanese'
    localStorage.setItem('zenmai-body-lang', bodyLang)
    applyBodyLang()
  }

  // ================= 設定と手引き（ヘッダの歯車）=================
  const panel = $('panel')
  const bindChk = (id, cls, store) => {
    const el = $(id)
    if (!el) return
    el.checked = !document.body.classList.contains(cls)
    el.addEventListener('change', () => {
      document.body.classList.toggle(cls, !el.checked)
      localStorage.setItem(store, el.checked ? 'on' : 'off')
    })
  }
  bindChk('ruby-chk', 'no-ruby', 'zenmai-ruby')
  bindChk('debug-chk', 'no-debug', 'zenmai-debug')

  // ★言い方は**アセットと command.js から引く** —— 画面に書き写すと、語彙を足し引きした
  //   ときに手引きだけ古くなり、★「打てない言葉を案内する」状態になる（実際に一度やった）
  const SYS = [
    ['得点を見る', 'SCORE'], ['保存する', 'SAVE'], ['続きから', 'RESTORE'],
    ['最初からやり直す', 'RESTART'], ['終わる', 'QUIT'], ['バージョン表示', 'VERSION'],
    ['体の具合を見る', 'DIAGNOSE'], ['描写を詳しく', 'VERBOSE'], ['描写を簡潔に', 'BRIEF'],
    ['描写をごく簡潔に', 'SUPER'], ['直前をもう一度', '@again'],
    // ★「はい / いいえ」は入れない —— 問いを出すときに画面が
    //   「（はい / いいえ）」と明記するので、手引きに書くと**同じことを 2 度言う**
  ]
  // ★出すのは**かなの形**。打鍵はかなだけなので、漢字の形を並べても打てない
  //   （カタカナしか無い語は kana() がひらがなに落とすので、そのまま出してよい）
  const pickKana = (list, n) => {
    const hira = list.filter((w) => /^[ぁ-んー]+$/.test(w))
    const kata = list.filter((w) => /^[ァ-ヶー]+$/.test(w))
    return (hira.length ? hira : kata).slice(0, n)
  }
  function renderSys() {
    const t = $('sys-list')
    if (!t || t._filled) return
    const { PARSER_WORDS } = window.ZenmaiCommand
    for (const [label, key] of SYS) {
      const words = key === '@again' ? pickKana(Object.keys(PARSER_WORDS), 2)
        : pickKana(((cmdAsset.verbs || {})[key] || {}).ja || [], 2)
      if (!words.length) continue
      const tr2 = document.createElement('tr')
      const k = document.createElement('td'); k.className = 'k'; k.textContent = label
      const v = document.createElement('td'); v.className = 'v'; v.textContent = words.join(' / ')
      tr2.appendChild(k); tr2.appendChild(v); t.appendChild(tr2)
    }
    t._filled = true
  }
  // ★ライセンスの全文は**実ファイルを読む** —— 画面に書き写すと、LICENSE を直したときに
  //   ここだけ古くなる（言い方の表と同じ理由）。
  //   ★MIT が求めているのは「複製物に著作権表示と許諾文を含めること」であって画面表示ではない。
  //   本体は配布物にファイルが在ること（build.mjs）で、ここは**そこへ辿り着く道**を作っている
  const LICENSES = [
    ['この Zenmai（自作部分）', '../LICENSE'],
    ['Z-machine 実装 — ifvms.js (ZVM)', '../vendor/LICENSE.ifvms'],
    ['作品のソース — historicalsource（2025 年公開）', '../vendor/zork1/LICENSE'],
  ]
  async function renderLicense() {
    const box = $('license')
    if (!box || box._filled) return
    box._filled = true
    for (const [label, url] of LICENSES) {
      const d = document.createElement('details')
      const s = document.createElement('summary'); s.textContent = label
      const pre = document.createElement('pre'); pre.textContent = '…'
      d.appendChild(s); d.appendChild(pre); box.appendChild(d)
      // ★Cloudflare Pages は**無いパスにも index.html を 200 で返す**ので `ok` では足りない。
      //   中身を見て弾く（怠ると HTML が全文の顔で並ぶ）
      let text = null
      try {
        const res = await fetch(url, { cache: 'no-store' })
        const body = res.ok ? await res.text() : ''
        if (body && !/^\s*<!doctype/i.test(body)) text = body
      } catch (e) { /* 下で断る */ }
      pre.textContent = text || `（${url} を読めなかった。リポジトリの同名ファイルが正）`
    }
  }
  const showPanel = (on) => {
    if (!panel) return
    if (on) { renderSys(); renderLicense() }
    panel.hidden = !on
    if (!on) refocus()
  }
  if ($('menu-btn')) $('menu-btn').addEventListener('click', () => showPanel(panel.hidden))
  if ($('panel-close')) $('panel-close').addEventListener('click', () => showPanel(false))
  if (panel) panel.addEventListener('click', (e) => { if (e.target === panel) showPanel(false) })
  if (document.addEventListener) {
    document.addEventListener('keydown', (e) => {
      if (e.key === 'Escape' && panel && !panel.hidden) showPanel(false)
    })
  }

  // story file を読む。★**中身を確かめてから使う** ——
  //   Cloudflare Pages は**存在しないパスにも index.html を 200 で返す**ので、
  //   `r.ok` は「そのファイルがあった」ことを意味しない（公開して初めて出た。
  //   `[エラー] This is not a Z-Code file` の正体が HTML を食わせていたこと）。
  //   Z-code は先頭バイトが版（1〜8）なので、そこで見分ける
  const looksLikeStory = (b) => b && b.length > 1024 && b[0] >= 1 && b[0] <= 8
  let story = null
  for (const url of ['vendor/zork1/zork1.z3', '../vendor/zork1/zork1.z3', 'zork1.z3']) {
    try {
      const r = await fetch(url, { cache: 'no-store' })
      if (!r.ok) continue
      const b = new Uint8Array(await r.arrayBuffer())
      if (looksLikeStory(b)) { story = b; break }
    } catch (e) { /* 次を試す */ }
  }
  if (!story) {
    show('story file が読めなかった（`vendor/zork1/zork1.z3`）。', 'raw')
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
      p.innerHTML = cls === 'raw' || cls === 'sent' ? rb.esc(p._raw) : ruby(p._raw)
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
      $('place').innerHTML = ruby(place)
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
        // ★版権表示と本文の境目に線を引く（最初の場所名の前だけ）
        if (!screen.querySelector('hr')) screen.insertBefore(document.createElement('hr'), p)
        const head = document.createElement('p')
        head.className = 'room'; head._raw = place; head.innerHTML = ruby(place)
        screen.insertBefore(head, p)
        p._raw = p._raw.slice(place.length).replace(/^\n+/, '')
        if (p._raw) p.innerHTML = ruby(p._raw)
        else p.remove()
      }
      // ★成績は手引きの中に置く（遊んでいる画面には出さない）
      $('stat').textContent = `引けた ${tr.stats.hit} 行 / 未訳 ${tr.stats.miss} 行`
        + ` / 訳さない ${tr.stats.notrans} 行`
      const note = $('stat-note')
      if (note) {
        note.textContent = tr.stats.miss
          ? '未訳の行は英語のまま、左に罫を付けて出る。ここに出たものは埋められる。'
          : 'ここまでに出た行は、版権表示をのぞいてすべて日本語になっている。'
      }
      $('input').disabled = Glk.waitingFor() !== 'line'
      $('input').placeholder = Glk.waitingFor() === 'char' ? '何かキーを（Enter で進む）' : hint()
      refocus()
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
    // ★轟音の部屋は**打った語を返す**部屋。原作へ渡すのは英語だが、返すべきは
    //   こちらのプレイヤーが打った呼び名なので、訳す側に教えておく
    tr.setEcho(r.echoWord || text)
    tr.setSaid(r.said || '')
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

  // ================= フリック（スマホ）=================
  // ★op の語彙はゲームパッドと同じ（kana / key）なので、配線は padInsert / padKey を使い回す。
  //   盤の中身は flickmap（JSON）で決まる —— 要らないキーは**データから消してある**
  //   （変換・戻す・英数・かな・句読点・括弧・疑問符）
  if (window.FlickEngine) {
    let kbd = null
    const flickOpen = () => document.body.classList.contains('flick-open')
    const showFlick = async (on) => {
      document.body.classList.toggle('flick-open', on)
      localStorage.setItem('zenmai-flick', on ? 'on' : 'off')
      // ★盤を出しているあいだは OS のキーボードを出さない（二重に出ると本文が潰れる）
      $('input').setAttribute('inputmode', on ? 'none' : 'text')
      if (!on) { if (kbd) { kbd.destroy(); kbd = null } return }
      if (kbd) return
      const map = window.FlickEngine.decodeFlickmap(await load('../assets/flick-ja.json'))
      kbd = window.FlickEngine.mount($('flick'), map, {
        getComposingTail: () => {
          const el = $('input')
          const at = el.selectionStart == null ? el.value.length : el.selectionStart
          return el.value.slice(0, at)
        },
        onOp(op) {
          if (op.type === 'kana') { padInsert(op.text, op.replace || 0); return }
          if (op.type === 'text') { padInsert(op.text, 0); return }
          if (op.type === 'key' && op.tap) padKey(op.tap.key)
        },
      })
      // ★字数の多いキー（「゛゜小」）に印を付ける。CSS には「字数で級数を変える」術が
      //   無いので、ここで見て回る。エンジンには手を入れない（図と挙動がずれるほうが害が大きい）
      const root = $('flick')
      if (root.querySelectorAll) {
        for (const el of root.querySelectorAll('.fe-key')) {
          if ((el.textContent || '').length > 1) el.classList.add('fe-multi')
        }
      }
    }
    $('flick-btn').addEventListener('click', () => { showFlick(!flickOpen()).then(refocus) })
    // ★触る画面なら最初から開いておく（スマホで来た人に「打てない」と思わせない）
    const touch = (navigator.maxTouchPoints || 0) > 0
    if (localStorage.getItem('zenmai-flick') === 'on' || (touch && localStorage.getItem('zenmai-flick') !== 'off')) {
      showFlick(true)
    }
  }

  // ================= ゲームパッド =================
  // ★iPad の Safari では、肩ボタンが**フォーカス移動**として解釈され、コマンド欄の焦点が
  //   付いたり外れたりする（キャレットと下線が点滅する）。Tab の抑止と `tabindex` 外しを
  //   試したが**どちらも効かなかった**ので撤去した —— 原因は Safari のブラウザ UI 側にあり、
  //   ページからは触れない。★**ホーム画面に追加して起動すれば起きない**（実機で確認）。
  //   直せないものに手当てを残すと、対価（キーボード操作の妨げ）だけが残る
  // ★labo の GamepadEngine（v1.7.0）をそのまま使い、**ホスト側で op を絞る**。
  //   ここは変換をしない（ひらがなをそのまま送る）ので、要らない操作を落とすだけで足りる。
  // ★入力欄の隣の札に出す「いま選んでいる行」は**エンジンの表から引く** ——
  //   日本語の行名を直書きしていたので、英語に切り替えても かな のままだった（実機の指摘）
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

  // ★操作図は**自前で描く**。labo の GamepadEngine のビジュアライザは
  //   色も造形も labo のもので、明朝＋紙の意匠に合わず、寸法も固定 px で狭い画面から溢れた。
  //   ★エンジンの**入力の側**（子音行・濁点・拗音の状態機械）は難しいので触らない ——
  //   `onState` が状態をくれるので、**描くところだけ**こちらの手に置く
  const GP = { A: 0, B: 1, X: 2, Y: 3, LB: 4, RB: 5, LT: 6, RT: 7, UP: 12, DOWN: 13, LEFT: 14, RIGHT: 15 }
  // ★入力表は**エンジンから引く**（v1.8.0 で公開された）。以前は写していたが、
  //   写すと表がずれても誰も気づけない —— 図に出ている字と実際に入る字が違う、が最悪の事故
  const table = (lang) => {
    const G = window.GamepadEngine || {}
    return (lang === 'english' ? G.ENGLISH_TABLE : G.KANA_TABLE) || []
  }
  // D-pad の並び = 中央・←・↑・→・↓ に、行 0〜4（LB を押すと 5〜9）が対応する
  const DPAD_ORDER = ['center', 'left', 'up', 'right', 'down']
  const rowOf = (lang, layer, pos) => table(lang)[(layer === 'lb' ? 5 : 0) + DPAD_ORDER.indexOf(pos)] || []
  // 十字の札。日本語は行の頭（あ/か/…）、英語は**プッシュホン式**（数字と英字の 2 段）
  const dpadLabel = (lang, layer, pos) => {
    const r = rowOf(lang, layer, pos)
    if (lang !== 'english') return { main: r[0] || '', sub: '' }
    return { main: r[0] || '', sub: r.slice(1).filter(Boolean).join('') }
  }
  // ★肩の札。日本語: 拗/小・は〜わ・ん/を　英語: シフト・6〜0（後半の面）・0
  //   ★英語の LT は**札の字面そのもので状態を示す**（GIME と同じ流儀）——
  //   `shift`（何もなし）→ `Shift`（次の 1 文字だけ大文字）→ `CAPS`（固定）。
  //   状態を色や記号で足すより、**その札を押すと何が起きるかが字で読める**
  const shoulderLabels = (lang, st) => {
    if (lang !== 'english') return { [GP.LT]: '拗/小', [GP.LB]: 'は〜わ', [GP.RT]: 'ん/を' }
    const lt = st && st.englishCapsLock ? 'CAPS' : st && st.englishShiftNext ? 'Shift' : 'shift'
    return { [GP.LT]: lt, [GP.LB]: '6〜0', [GP.RT]: '0' }
  }
  const padBtns = new Map()   // ボタン番号 → 要素
  let padCenter = null        // 十字の中央（どれも押していないことを示す）
  let padLangBtns = null      // { japanese, english } の切替ボタン
  let padCtl = null           // GamepadEngine.start() が返すコントローラ

  /** 選んでいる言語のボタンに印を付ける（枠線）。★押せるものと現在地を 1 か所で見せる */
  function markPadLang() {
    if (!padLangBtns) return
    for (const [lang2, b] of Object.entries(padLangBtns)) {
      if (b && b.classList) b.classList.toggle('on', lang2 === padLang)
    }
  }

  /**
   * 入力言語を実際に切り替える。★エンジン側の状態機械も一緒に切り替える
   * （表だけ変えても**入る字は変わらない**）。
   */
  function setPadLangEffective(lang) {
    padLang = lang === 'english' ? 'english' : 'japanese'
    if (padCtl && padCtl.setLang) padCtl.setLang(padLang)
    markPadLang()
    // ★札は毎フレーム書き替わるが、押していないあいだも切替が即見えるように 1 回描く
    updatePad({ pressed: new Set(), activeRow: 0, activeLayer: 'base' })
  }

  function buildPad(root) {
    padBtns.clear(); padCenter = null
    root.textContent = ''
    const el = (cls, parent) => {
      const d = document.createElement('div')
      d.className = cls
      if (parent && parent.appendChild) parent.appendChild(d)
      return d
    }
    const wrap = el('gp', root)
    // ★肩は**横並び**。外側が LT / RT、内側が LB / RB（中央を軸に左右対称）
    const buildSide = (side) => {
      const col = el('gp-side', wrap)
      const sh = el('gp-shoulders', col)
      const pair = side === 'l' ? [GP.LT, GP.LB] : [GP.RB, GP.RT]
      // ★肩の札は言語で変わる（RB は母音そのものなので updatePad が毎フレーム書き替える）
      const FIXED = shoulderLabels(padLang)
      // ★**文字を出すボタン**（RB=母音そのもの、RT=ん/を）は面のボタンと同じ濃さにする ——
      //   出るものが同じなら、色で差を付ける理由がない。LT / LB は機能キーなので薄いまま。
      //   ★大きさは 4 つとも揃える（役が違っても、並んでいるものの寸法が揃わないと乱れて見える）
      const CLS = { [GP.RB]: ' gp-char', [GP.RT]: ' gp-char' }
      for (const b of pair) {
        const t = el('gp-trigger' + (CLS[b] || ''), sh)
        t._btn = b
        if (FIXED[b]) t.textContent = FIXED[b]
      }
      const grid = el(side === 'l' ? 'gp-grid gp-dpad' : 'gp-grid gp-face', col)
      // 3×3 の菱形。十字は上下左右＋中央、フェイスは 4 隅を空けた 4 つ
      const map = side === 'l'
        ? [null, GP.UP, null, GP.LEFT, 'center', GP.RIGHT, null, GP.DOWN, null]
        : [null, GP.Y, null, GP.X, null, GP.B, null, GP.A, null]
      for (const slot of map) {
        const cell = el(slot === null ? 'gp-btn gp-blank' : slot === 'center' ? 'gp-btn gp-center' : 'gp-btn', grid)
        if (slot === 'center') padCenter = cell
        else if (slot !== null) cell._btn = slot
      }
      for (const c of sh.children || []) if (c._btn != null) padBtns.set(c._btn, c)
      for (const c of grid.children || []) if (c._btn != null) padBtns.set(c._btn, c)
    }
    buildSide('l')
    // ★十字とフェイスのあいだの空き地。**上端に言語切替**（面の上端に揃う）、**一番下に使い方**
    const mid = el('gp-mid', wrap)
    // ★言語は**2 つ並べて、選んでいるほうに枠線**を付ける。
    //   1 つのボタンで切り替える形だと「いま何が入るのか」と「押すとどうなるのか」が
    //   同じ札に同居して読みにくい。★字は `JA` / `EN` —— 「かな」だと全角 2 文字で溢れ、
    //   「あ」だと**十字の中央に出る「あ」**（行の選択）と紛れる
    const langs = el('gp-langs', mid)
    padLangBtns = {}
    for (const [code, lang2] of [['JA', 'japanese'], ['EN', 'english']]) {
      const b = document.createElement('button')
      b.className = 'gp-lang'
      b.type = 'button'
      b.textContent = code
      b.title = lang2 === 'english' ? '英字で打つ' : 'かなで打つ'
      b.addEventListener('click', () => setPadLangEffective(lang2))
      langs.appendChild(b)
      padLangBtns[lang2] = b
    }
    markPadLang()
    const help = document.createElement('button')
    help.className = 'gp-help'
    help.type = 'button'
    help.textContent = '?'
    help.title = 'コントローラの使い方'
    help.addEventListener('click', () => showPadHelp(true))
    mid.appendChild(help)
    buildSide('r')
  }

  // ★使い方のダイアログ。設定の手引きと同じ作りにする（閉じ方も揃える）
  const padHelp = $('pad-help')
  const showPadHelp = (on) => {
    if (!padHelp) return
    // ★開くたびに**いまのモードの説明**へ差し替える —— 盤に出ているものと食い違わせない
    const ja = $('help-ja')
    const en = $('help-en')
    if (ja) ja.hidden = padLang === 'english'
    if (en) en.hidden = padLang !== 'english'
    padHelp.hidden = !on
    if (!on) refocus()
  }
  if ($('pad-help-close')) $('pad-help-close').addEventListener('click', () => showPadHelp(false))
  if (padHelp) padHelp.addEventListener('click', (e) => { if (e.target === padHelp) showPadHelp(false) })
  if (document.addEventListener) {
    document.addEventListener('keydown', (e) => {
      if (e.key === 'Escape' && padHelp && !padHelp.hidden) showPadHelp(false)
    })
  }

  function updatePad(st) {
    const on = (elm, yes) => { if (elm && elm.classList) elm.classList.toggle('on', !!yes) }
    for (const [num, elm] of padBtns) on(elm, st.pressed && st.pressed.has(num))
    // ★十字は**押していないこと**を中央で示す（どの行を選んでいるかが一目で分かる）
    const anyDir = [GP.UP, GP.DOWN, GP.LEFT, GP.RIGHT].some((n) => st.pressed && st.pressed.has(n))
    on(padCenter, !anyDir)

    // ── 札を書き替える ──
    // ★2 段（プッシュホンの「2 abc」）を出せるようにする。日本語は 1 段だけ使う
    const put = (num, main, sub) => { const e = padBtns.get(num); if (e) setLabel(e, main, sub) }
    // 十字＝行（LB を押しているあいだは後半の面）
    const layer = st.activeLayer === 'lb' ? 'lb' : 'base'
    for (const [num, pos] of [[GP.LEFT, 'left'], [GP.UP, 'up'], [GP.RIGHT, 'right'], [GP.DOWN, 'down']]) {
      const l = dpadLabel(padLang, layer, pos)
      put(num, l.main, l.sub)
    }
    if (padCenter) {
      const l = dpadLabel(padLang, layer, 'center')
      setLabel(padCenter, l.main, l.sub)
    }
    // 肩（RB を除く）＝言語で変わる固定の札。★buildPad で 1 度書くだけだと、
    //   言語を切り替えたときに**日本語のまま残る**（実機の指摘）
    const sh = shoulderLabels(padLang, st)
    for (const k of [GP.LT, GP.LB, GP.RT]) put(k, sh[k])
    // 右手＝いま選んでいる行の 5 つ。★この企画で**落としている記号**（や行の「」・わ行の ？）は
    //   図でも空にする —— 出ているのに押しても入らない、が図といちばんずれる
    const rows = table(padLang)
    const row = rows[st.activeRow] || rows[0] || []
    // ★シフトが効いているあいだは**図の英字も大文字**にする ——
    //   押したら大文字が入るのに小文字が出ていたら、図が嘘をつくことになる
    const upper = padLang === 'english' && (st.englishCapsLock || st.englishShiftNext)
    const show = (ch) => {
      const c = DROP.has(ch) ? '' : (ch || '')
      return upper ? c.toUpperCase() : c
    }
    put(GP.RB, show(row[0]))
    put(GP.X, show(row[1])); put(GP.Y, show(row[2])); put(GP.B, show(row[3])); put(GP.A, show(row[4]))
  }

  /** 札を書く。`sub` があれば 2 段（プッシュホン式）にする */
  function setLabel(el, main, sub) {
    if (!sub) { el.textContent = main; return }
    el.textContent = ''
    for (const [cls, t] of [['gp-l1', main], ['gp-l2', sub]]) {
      const s = document.createElement('span')
      s.className = cls
      s.textContent = t
      el.appendChild(s)
    }
  }

  if (window.GamepadEngine && typeof navigator !== 'undefined' && navigator.getGamepads) {
    let vis = null
    const padOpen = () => document.body.classList.contains('pad-open')
    const showPad = (on) => {
      document.body.classList.toggle('pad-open', on)
      localStorage.setItem('zenmai-pad', on ? 'on' : 'off')
      if (on && !vis) { buildPad($('pad')); vis = true }
      if (!on && vis) { $('pad').textContent = ''; padBtns.clear(); padCenter = null; vis = null }
    }
    $('pad-btn').addEventListener('click', () => { showPad(!padOpen()); refocus() })

    padCtl = window.GamepadEngine.start({
      // ★前回選んだ言語で始める（切り替えたまま閉じた人に、また切り替えさせない）
      lang: padLang,
      // ★自動確定を止める。ここが入っていると R🕹↓ の 600ms 後に Enter が飛び、
      //   **打っていないのにコマンドが送られる**（変換をしないホストには要らない挙動）
      autoConfirm: false,
      // ★濁点（R🕹↑）と拗音（LT）は**末尾の文字を見て置換**を決める。
      //   ここを常に空で返していたので、どちらも何も起きなかった（実機で判明）
      getComposingTail: () => {
        const el = $('input')
        const at = el.selectionStart == null ? el.value.length : el.selectionStart
        return el.value.slice(0, at)
      },
      onOp(op) {
        if (op.type === 'kana') {
          // ★英語では ↓ は**空白**で、単語の区切りにどうしても要る（`take lamp`）ので入れる。
          //   ★日本語の ↓（、。）は**何もしない** —— 原作は句読点を受け取らないし、
          //   ここを送信に読み替えていたら誤送信が多かった（実機の指摘）。
          //   ★送信は**両言語とも LS 押し込み**に寄せた（`autoConfirm: false` で
          //   遅延確定を止めているので、↓ から Enter が飛ぶこともない）
          if (padLang === 'english' && op.text === ' ') { padInsert(' ', op.replace || 0); return }
          if (DROP.has(op.text)) return
          padInsert(op.text, op.replace || 0)
          return
        }
        if (op.type === 'key' && op.tap) padKey(op.tap.key)
      },
      onState(st) {
        document.body.classList.toggle('pad-on', !!st.connected)
        if (st.connected && localStorage.getItem('zenmai-pad') !== 'off' && !padOpen()) showPad(true)
        // ★英語は**数字だけでなく英字も併記**する（1 段だと何が打てるか分からない）。
        //   日本語は「あ」だけで伝わるので 1 段のまま —— ただし**高さは同じ**にして、
        //   言語を切り替えたときに欄が伸び縮みしないようにする（CSS の min-height）
        const rows = table(padLang)
        const r = rows[st.activeRow] || rows[0] || []
        const main = st.previewChar || r[0] || (padLang === 'english' ? '1' : 'あ')
        const sub = padLang === 'english' && !st.previewChar ? r.slice(1).filter(Boolean).join('') : ''
        setLabel($('pad-btn'), main, sub)
        if (vis) updatePad(st)
      },
    })
  }

  // ★入口の案内を閉じる道は 1 本にする（✕ / 背景 / Escape / 言語ボタン）。
  //   ★ここが**ゲームを始める合図**でもある —— 言語を選ぶ前に冒頭が印字されないように
  function closeIntro() {
    const i = $('intro')
    if (i) i.hidden = true
    startGame()
    refocus()
  }

  /**
   * 案内の見せ方を、**始める前 / 始めたあと**で変える。
   * ★始める前は言語ボタンだけ（それが「はじめる」を兼ねる）。
   * ★始めたあとは「とじる」だけ —— 途中で言語は変えられないので、選ばせると嘘になる。
   */
  function introMode() {
    const started = !!startGame._done
    const lang = $('intro-lang')
    if (lang) lang.hidden = started
    const ok = $('intro-ok')
    if (ok) ok.hidden = !started
  }

  // ★入口の案内。軽いので毎回出す
  const intro = $('intro')
  if (intro) {
    intro.hidden = false
    introMode()
    $('intro-ok').addEventListener('click', closeIntro)
    intro.addEventListener('click', (e) => { if (e.target === intro) closeIntro() })
    // ★ライセンスは案内の**上に重ねて**出す。案内を閉じない＝まだ始めない
    //   （閉じることが始める合図なので、読んだだけで冒頭が印字されてはいけない）
    if ($('intro-license')) $('intro-license').addEventListener('click', () => showPanel(true))
    // ★題を押すともう一度出す。★2 度目からは「はじめる」ではなく「とじる」——
    //   遊んでいる途中に押した人に「最初からやり直る」と読ませない
    if ($('title')) {
      $('title').addEventListener('click', () => {
        intro.hidden = false
        introMode()
      })
    }
    if (document.addEventListener) {
      document.addEventListener('keydown', (e) => { if (e.key === 'Escape' && !intro.hidden) closeIntro() })
    }
  }

  // ★狭い画面では例まで入らず**末尾が切れて読めない**。畳んで名詞だけ残す
  const hint = () => (window.innerWidth < 520
    ? 'ひらがなで打つ' : 'ひらがなで打つ（例: ゆうびんばこをあける）')
  window.addEventListener('resize', () => {
    if (Glk.waitingFor() === 'line') $('input').placeholder = hint()
  })

  // ★言語の設定を反映してから始める（訳の素通しは最初の 1 行目から効かせる必要がある）
  const bindLangSelect = (id, apply, value) => {
    const el = $(id)
    if (!el) return
    el.value = value
    el.addEventListener('change', () => apply(el.value))
  }
  for (const [id, lang] of [['intro-ja', 'japanese'], ['intro-en', 'english']]) {
    if ($(id)) $(id).addEventListener('click', () => { setBodyLang(lang); closeIntro() })
  }
  applyBodyLang()

  // ★ゲームは**案内を閉じてから**始める。先に走らせると、冒頭の文が
  //   言語を選ぶ前に印字されてしまい、選んでも手遅れになる（リロードするまで直らない）。
  //   ★訳したあとの文からは元の英語に戻せないので、後から刷り直す手は使えない
  if (!intro) startGame()

  function startGame() {
    if (startGame._done) return
    startGame._done = true
    applyBodyLang()          // 訳の素通しは 1 行目から効かせる
    const vm = new window.ZVM()
    vm.prepare(story, { vm, Glk, GlkOte: null, Dialog: null })
    Glk.init({ vm })
  }

  function show2() {}   // （予約）ゲームパッド入力はここに繋ぐ
})()
