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

  const asset = await (await fetch('../assets/zork1-ja.json')).json()
  const tr = new Translator(asset)

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

  let para = null
  function show(text, cls) {
    // 空行で段落を切る。改行はそのまま活かす（pre-wrap）
    for (const chunk of text.split(/\n{2,}/)) {
      if (!chunk) { para = null; continue }
      if (!para || cls) {
        para = document.createElement('p')
        if (cls) para.className = cls
        screen.appendChild(para)
        if (cls) para = null
      }
      const p = para || screen.lastElementChild
      p.textContent += chunk
    }
    screen.scrollTop = screen.scrollHeight
  }

  const Glk = createGlk({
    cols: 64,
    rows: 24,
    write(text) {
      // ★訳せた行と訳せなかった行を分けて描く（未訳が目で見える）
      const out = tr.feed(text)
      if (out) show(out)
    },
    status(line) {
      // v3 のステータス行は「部屋名 ……… 得点/手数」。部屋名だけ引く
      const m = line.match(/^(.*?)\s{2,}(.*)$/)
      $('place').textContent = m ? tr.word(m[1]) : tr.word(line)
      $('score').textContent = m ? m[2] : ''
    },
    update() {
      const rest = tr.flush()
      if (rest) show(rest)
      $('stat').textContent = ` 引けた ${tr.stats.hit} 行 / 未訳 ${tr.stats.miss} 行`
      $('input').disabled = Glk.waitingFor() !== 'line'
      if (Glk.waitingFor() === 'char') $('input').placeholder = '何かキーを（Enter で進む）'
      else $('input').placeholder = '英語のコマンドを打つ（例: open mailbox）'
      $('input').focus()
    },
  })

  $('input').addEventListener('keydown', (e) => {
    if (e.key !== 'Enter') return
    const text = $('input').value
    $('input').value = ''
    if (Glk.waitingFor() === 'char') { Glk.submitChar(32); return }
    show(text || ' ', 'cmd')
    Glk.submitLine(text)
  })

  const vm = new window.ZVM()
  vm.prepare(story, { vm, Glk, GlkOte: null, Dialog: null })
  Glk.init({ vm })

  function show2() {}   // （予約）ゲームパッド入力はここに繋ぐ
})()
