#!/usr/bin/env node
// PMD の MML を `.M` にする（MC.EXE を QuuBee の headless で走らせる）。
//   node mc98.js pc98-music/CANON.MML        → pc98-music/CANON.M
// ★MC.EXE は `sh build-mc.sh` で建てる（pc98-music/.mc/MC.EXE）。QuuBee のリポジトリ = 環境変数 QB_DIR（既定 ~/development/qb）。
// ★音色を MML の中に書く（`@` 行）には、音色ファイル（.FF）が**先にある**必要がある（無いと MC が `@` を受けない）。
//   空（0 で埋めた 256 音色）を置いて渡す。できた .FF は捨てる（音色は .M の中に添付される）
const fs = require('fs');
const path = require('path');
const os = require('os');
const QB = process.env.QB_DIR || path.join(os.homedir(), 'development/qb');
const { Machine } = require(path.join(QB, 'tools/lib/machine'));

(async () => {
    const mml = process.argv[2];
    if (!mml) { console.error('使い方: node mc98.js X.MML'); process.exit(2); }
    const mc = path.join(__dirname, 'pc98-music/.mc/MC.EXE');
    if (!fs.existsSync(mc)) { console.error('MC.EXE が無い（sh build-mc.sh）'); process.exit(1); }
    const base = path.basename(mml).replace(/\.mml$/i, '').toUpperCase();
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'mc98-'));
    fs.copyFileSync(mc, path.join(dir, 'MC.EXE'));
    fs.copyFileSync(mml, path.join(dir, base + '.MML'));
    fs.writeFileSync(path.join(dir, base + '.FF'), Buffer.alloc(6656));
    fs.writeFileSync(path.join(dir, 'RUN.BAT'), `MC /VW ${base}\r\n`);
    const m = await Machine.boot({ dir, bat: 'RUN.BAT', extmem: 2 });
    m.runUntil((mm) => mm.exited() || mm.batchDone(), 3000, 10);
    let data;
    try { data = m.M.FS.readFile('/run/' + base + '.M'); } catch (e) { data = null; }
    if (!data) {
        const shot = path.join(os.tmpdir(), 'mc98-error.png');
        m.screenshotPng(shot);
        console.error(`コンパイルできなかった（画面 = ${shot}）`);
        process.exit(1);
    }
    const out = path.join(path.dirname(mml), base + '.M');
    fs.writeFileSync(out, data);
    console.log(`${out}（${data.length} バイト）`);
    fs.rmSync(dir, { recursive: true, force: true });
    process.exit(0);
})().catch((e) => { console.error(e); process.exit(1); });
