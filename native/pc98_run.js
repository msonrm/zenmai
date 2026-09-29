#!/usr/bin/env node
// PC-98 版を QuuBee（headless）で台本つきで走らせ、記録（ZENMAI.LOG）と画面を取り出す。
//   node pc98_run.js 台本.txt 出力先/        → 出力先/ZENMAI.LOG・出力先/screen-1x.png
//   PC98_EXTMEM=2 node pc98_run.js …        → 拡張メモリ 2MB の機械で
// QuuBee のリポジトリ = 環境変数 QB_DIR（既定 ~/development/qb）。tools/lib/machine.js を使う。
// ★台本の終わりは、本体が入力欄に出す ASCII の [END] で見分ける（テキスト VRAM を読む）。
const fs = require('fs');
const path = require('path');
const os = require('os');
const QB = process.env.QB_DIR || path.join(os.homedir(), 'development/qb');
const { Machine } = require(path.join(QB, 'tools/lib/machine'));

(async () => {
    const [script, outDir] = process.argv.slice(2);
    if (!script || !outDir) { console.error('使い方: node pc98_run.js 台本.txt 出力先/'); process.exit(2); }
    const game = path.join(outDir, 'game');
    fs.mkdirSync(game, { recursive: true });
    for (const f of ['ZENMAI.EXE', 'ZORK1.ZMP', 'ZORK1.Z3', 'DOS4GW.EXE'])   // ★作品 = パック + story（pack.h）
        fs.copyFileSync(path.join(__dirname, 'pc98-out', f), path.join(game, f));
    fs.copyFileSync(script, path.join(game, 'SCRIPT.TXT'));
    fs.writeFileSync(path.join(game, 'RUN.BAT'), 'SET DOS16M=1\r\nZENMAI /S SCRIPT.TXT\r\n');

    const t0 = Date.now();
    // 拡張メモリ（MB）。PC98_EXTMEM が無ければ QuuBee の既定。★test-pc98.sh は 2 で流す（要件の下限）
    const extmem = process.env.PC98_EXTMEM ? +process.env.PC98_EXTMEM : null;
    const m = await Machine.boot({ dir: game, bat: 'RUN.BAT', extmem });
    const done = m.runUntil((mm) => mm.textVram(17)[15].includes('[END]'), 60000, 30);
    const info = m.info();
    console.log(`QuuBee wasm ${info.wasm.sha256} frame ${info.frame}（エミュ ${info.emuSeconds} 秒・実 ${((Date.now() - t0) / 1000).toFixed(1)} 秒）`);
    m.screenshotPng(path.join(outDir, 'screen-1x.png'));
    if (!done) { console.error('[END] が出なかった'); process.exit(1); }
    fs.writeFileSync(path.join(outDir, 'ZENMAI.LOG'), m.M.FS.readFile('/run/ZENMAI.LOG'));
    m.pressKey(0x1c);                  // RETURN で DOS へ返す
    const exited = m.runUntil((mm) => mm.exited() || mm.batchDone(), 600, 10);
    console.log(exited ? 'DOS へ返った' : '★DOS へ返らなかった');
    process.exit(0);
})().catch((e) => { console.error(e); process.exit(1); });
