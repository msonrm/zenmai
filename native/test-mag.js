#!/usr/bin/env node
// 引数の .MAG を QuuBee のデコーダ（web/player/magimage.js）で復号し、test_mag（C）と同じ形で印字する。
// 使い方: node test-mag.js X.MAG … （QB_DIR = QuuBee のリポジトリ、既定 ~/development/qb）
const fs = require('fs'), path = require('path'), os = require('os');
const QB = process.env.QB_DIR || path.join(os.homedir(), 'development/qb');
require(path.join(QB, 'web/player/magimage.js'));
for (const f of process.argv.slice(2)) {
    const d = globalThis.QBMag.decode(new Uint8Array(fs.readFileSync(f)));
    let h = 2166136261;
    for (const v of d.rgba) h = Math.imul(h ^ v, 16777619) >>> 0;
    console.log(`${d.width}x${d.height} ${h.toString(16).padStart(8, '0')}`);
}
