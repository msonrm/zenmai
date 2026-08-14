// Cloudflare Pages 用の配置を作る。
// ★`web/` をそのまま root にできない —— `../assets` `../vendor` `../src` を参照しているから。
//   ここで 1 階層に畳んで、参照も畳んだ形に書き換える。
import { mkdir, rm, cp, readFile, writeFile } from 'node:fs/promises'
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

const fold = (s) => s.replace(/\.\.\/(assets|vendor|src)\//g, '$1/')
await writeFile(path.join(OUT, 'index.html'), fold(await readFile(path.join(ROOT, 'web', 'index.html'), 'utf8')))
await writeFile(path.join(OUT, 'main.js'), fold(await readFile(path.join(ROOT, 'web', 'main.js'), 'utf8')))

// ★アセットは差し替わる。訳文と語彙の版がずれると「画面に出る名前で打てない」が起きる
await writeFile(path.join(OUT, '_headers'), [
  '/assets/*', '  Cache-Control: no-store', '',
  '/vendor/*', '  Cache-Control: public, max-age=604800', '',
].join('\n'))

console.log('dist/ を作った')
