'use strict'
/**
 * ふりがな。
 *
 * ★これは飾りではなく**操作系**。入力はかなだけなので、読めない漢字は打てない
 * ＝そのものを指せない。「格子」を素直に読める人がどれだけいるか、という話。
 *
 * 読みは入力語彙が持っているものを、送り仮名でアラインメントして漢字ごとに分けてある
 * （`血のついた斧` → 血(ち) + のついた + 斧(おの)）。作るのはビルド時（`zork1_asset.py`）。
 */
function createRubifier(ruby) {
  // 長い名前から当てる（`台所の窓` を `窓` に食われないように）
  const keys = Object.keys(ruby || {}).sort((a, b) => b.length - a.length)
  const esc = (s) => String(s).replace(/[&<>]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;' }[c]))

  /** 文字列 → ふりがな入りの HTML（**この関数だけが HTML を作る**） */
  function rubify(text) {
    const s = String(text == null ? '' : text)
    let out = ''
    let i = 0
    while (i < s.length) {
      const k = keys.find((x) => s.startsWith(x, i))
      if (!k) { out += esc(s[i]); i++; continue }
      for (const [seg, yomi] of ruby[k]) {
        out += yomi ? `<ruby>${esc(seg)}<rt>${esc(yomi)}</rt></ruby>` : esc(seg)
      }
      i += k.length
    }
    return out
  }

  return { rubify, esc, keys }
}

{
  const api = { createRubifier }
  if (typeof module !== 'undefined' && module.exports) module.exports = api
  if (typeof window !== 'undefined') window.ZenmaiRuby = api
}
