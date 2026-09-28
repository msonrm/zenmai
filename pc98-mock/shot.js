#!/usr/bin/env node
// out/game/ の SCREEN.COM を QuuBee (headless) で起動し、キー待ちの画面を out/screen-1x.png に撮る。
// QuuBee のリポジトリ = 環境変数 QB_DIR (既定 ~/development/qb)。tools/lib/machine.js と web/ のビルドを使う。
const path = require('path');
const os = require('os');
const QB = process.env.QB_DIR || path.join(os.homedir(), 'development/qb');
const { Machine } = require(path.join(QB, 'tools/lib/machine'));

(async () => {
    const out = path.join(__dirname, 'out');
    const m = await Machine.boot({ dir: path.join(out, 'game'), bat: 'RUN.BAT' });
    m.runFrames(240);                        // 表示が落ち着くまで (約 4 秒)
    m.screenshotPng(path.join(out, 'screen-1x.png'));
    const info = m.info();
    console.log(`QuuBee wasm ${info.wasm.sha256} (${info.wasm.mtime}) frame ${info.frame}`);
    process.exit(0);
})().catch((e) => { console.error(e); process.exit(1); });
