// Cloudflare Pages 用の配置を作る。
// ★`web/` をそのまま root にできない —— `../assets` `../vendor` `../src` を参照しているから。
//   ここで 1 階層に畳んで、参照も畳んだ形に書き換える。
import { mkdir, rm, cp, readFile, writeFile } from 'node:fs/promises'
import { createHash } from 'node:crypto'
import path from 'node:path'

const ROOT = path.dirname(new URL(import.meta.url).pathname)
const OUT = path.join(ROOT, 'dist')

await rm(OUT, { recursive: true, force: true })
await mkdir(OUT, { recursive: true })

// 1 階層に畳む（配布に要るものだけ）
for (const d of ['src', 'assets']) await cp(path.join(ROOT, d), path.join(OUT, d), { recursive: true })
await mkdir(path.join(OUT, 'vendor', 'zork1'), { recursive: true })
for (const f of ['zvm.min.js', 'gamepad-engine.js', 'flick-engine.js', 'LICENSE.ifvms']) {
  await cp(path.join(ROOT, 'vendor', f), path.join(OUT, 'vendor', f))
}
for (const f of ['zork1.z3', 'LICENSE', 'README.md']) {
  await cp(path.join(ROOT, 'vendor', 'zork1', f), path.join(OUT, 'vendor', 'zork1', f))
}
// ★自作分の LICENSE も配る。MIT が求めているのは「**複製物に**著作権表示と許諾文を含める」
//   ことなので、リポジトリに置いてあるだけでは配布物に無い。
//   実際 `/LICENSE` は 404 で index.html が返っていた（Pages のフォールバック）——
//   MIT を名乗っているのに、受け取った人がそれを確かめる先が無い状態だった
await cp(path.join(ROOT, 'LICENSE'), path.join(OUT, 'LICENSE'))

// ★`../LICENSE` も畳む（末尾が `/` でないので括弧の中に入れてある）
const fold = (s) => s.replace(/\.\.\/(assets\/|vendor\/|src\/|LICENSE)/g, '$1')
// ★main.js を先に書く（下のハッシュ付けが中身を読むため）
await writeFile(path.join(OUT, 'main.js'), fold(await readFile(path.join(ROOT, 'web', 'main.js'), 'utf8')))

// ★`<script src>` に**中身のハッシュ**を付ける。
//   vendor は 1 週間キャッシュしているので、エンジンを差し替えても**ブラウザが古いまま**になる。
//   実際に v1.8.0 を出した直後、実機だけが v1.7.0 を掴んで
//   「札が全部空 / 言語を切り替えても効かない」になった（curl の md5 照合では見抜けない ——
//   curl はキャッシュを通らないので、確認したこちらは緑に見えていた）。
//   版を手で書くと忘れるので、**中身から機械的に**付ける
const bust = async (html) => {
  const out = []
  let last = 0
  for (const m of html.matchAll(/<script src="([^"]+)"><\/script>/g)) {
    const src = m[1]
    let tag = m[0]
    try {
      const h = createHash('md5').update(await readFile(path.join(OUT, src))).digest('hex').slice(0, 8)
      tag = `<script src="${src}?v=${h}"></script>`
    } catch (e) { /* 外部 URL などは触らない */ }
    out.push(html.slice(last, m.index), tag)
    last = m.index + m[0].length
  }
  out.push(html.slice(last))
  return out.join('')
}
await writeFile(path.join(OUT, 'index.html'), await bust(fold(await readFile(path.join(ROOT, 'web', 'index.html'), 'utf8'))))

// ★アセットは差し替わる。訳文と語彙の版がずれると「画面に出る名前で打てない」が起きる
await writeFile(path.join(OUT, '_headers'), [
  '/assets/*', '  Cache-Control: no-store', '',
  '/vendor/*', '  Cache-Control: public, max-age=604800', '',
].join('\n'))

console.log('dist/ を作った')
