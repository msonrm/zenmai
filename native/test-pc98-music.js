#!/usr/bin/env node
// 起動画面の曲（PMD が鳴らす）の検査 —— QuuBee の headless で起動画面まで進め、音を録って大きさを見る。
//   node test-pc98-music.js          （pc98-out/ が建ててあること・PMD86.COM / PMD.COM / CANON.M も pc98-out/）
// ★見るもの: PMD86 でも PMD.COM でも鳴る（音の大きさ）/ PMD が無くても起動画面まで出る（無音）/
//   RETURN で始めると数秒で消える（フェードアウト）
const fs = require('fs');
const path = require('path');
const os = require('os');
const QB = process.env.QB_DIR || path.join(os.homedir(), 'development/qb');
const { Machine } = require(path.join(QB, 'tools/lib/machine'));
const out = path.join(__dirname, 'pc98-out');

function rms(pcm, from, to) {
    let s = 0, n = 0;
    for (let i = from; i < to && i < pcm.length; i++) { s += pcm[i] * pcm[i]; n++; }
    return n ? Math.sqrt(s / n) : 0;
}

async function run(driver) {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'zm-music-'));
    for (const f of ['ZENMAI.EXE', 'DOS4GW.EXE', 'CANON.M', ...fs.readdirSync(out).filter((n) => /\.(ZMP|Z3)$/.test(n))])
        fs.copyFileSync(path.join(out, f), path.join(dir, f));
    if (driver) fs.copyFileSync(path.join(out, driver + '.COM'), path.join(dir, driver + '.COM'));
    fs.writeFileSync(path.join(dir, 'RUN.BAT'), `SET DOS16M=1\r\n${driver ? driver + ' /K\r\n' : ''}ZENMAI\r\n`);
    const m = await Machine.boot({ dir, bat: 'RUN.BAT', extmem: 2 });
    m.runFrames(300);                                   // 起動画面まで
    const title = m.textVram(17).join('') + '';
    const pcm = m.captureAudio(4);
    const level = rms(pcm, 0, pcm.length);
    m.pressKey(0x1c);                                   // RETURN
    const tail = m.captureAudio(14);
    const sr = 44100 * 2;
    const last = rms(tail, tail.length - sr * 2, tail.length);
    fs.rmSync(dir, { recursive: true, force: true });
    return { level, last, title };
}

// 部屋ごとの曲: 台本（pc98-test/music.txt = 外 → 家の中 → 地下室）を PMD86 が常駐した状態で流し、
// 終わりで（地下室 = DEEP.M が）鳴っていること・[END] まで着くこと（曲の読み込みで止まらない）を見る
async function rooms() {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'zm-rooms-'));
    for (const f of fs.readdirSync(out).filter((n) => /\.(ZMP|Z3|INI|M|COM)$/i.test(n)).concat(['ZENMAI.EXE', 'DOS4GW.EXE']))
        fs.copyFileSync(path.join(out, f), path.join(dir, f));
    fs.copyFileSync(path.join(__dirname, 'pc98-test/music.txt'), path.join(dir, 'SCRIPT.TXT'));
    fs.writeFileSync(path.join(dir, 'RUN.BAT'), 'SET DOS16M=1\r\nPMD86 /K\r\nZENMAI /S SCRIPT.TXT\r\n');
    const m = await Machine.boot({ dir, bat: 'RUN.BAT', extmem: 2 });
    const done = m.runUntil((mm) => mm.textVram(17)[15].includes('[END]'), 60000, 30);
    const pcm = done ? m.captureAudio(4) : new Int16Array(0);
    fs.rmSync(dir, { recursive: true, force: true });
    return { done, level: rms(pcm, 0, pcm.length) };
}

(async () => {
    let fail = 0;
    const ok = (c, msg) => { console.log((c ? 'OK  ' : 'NG  ') + msg); if (!c) fail = 1; };
    for (const drv of ['PMD86', 'PMD']) {
        const r = await run(drv);
        ok(r.level > 800, `${drv}: 起動画面で鳴る（rms ${r.level | 0}）`);
        ok(r.last < 50, `${drv}: RETURN のあと数秒で消える（rms ${r.last | 0}）`);
    }
    const rr = await rooms();
    ok(rr.done, '部屋ごとの曲: 台本が [END] まで着く（曲の読み込みで止まらない）');
    ok(rr.level > 100, `部屋ごとの曲: 地下室（DEEP.M）が鳴っている（rms ${rr.level | 0}）`);
    const r = await run(null);
    ok(r.level < 50, `PMD なし: 無音で起動画面まで出る（rms ${r.level | 0}）`);
    process.exit(fail);
})().catch((e) => { console.error(e); process.exit(1); });
