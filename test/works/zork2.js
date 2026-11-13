'use strict'
/**
 * Zork II の検査の材料（`test/work.js` の `spec()` が読む）。書き方は `zork1.js` と同じ。
 * ★`native/make_corpus.py` がこのファイルの字面から入力を吸い上げる（行頭の `['…',`）
 */
module.exports = {
  // --- 作品の事実 ---
  start: 'Inside the Barrow',
  save: { cmds: ['take lamp', 'save', 's', 's', 'restore', 'look'] },
  walkthrough: 'walkthrough-zork2.txt',
  walkthroughJa: 'walkthrough-zork2-ja.txt',
  // ★1 字のコマンド不適語は無い（run-swallow.js のカナリアは鳴らさない）
  nocmdOneChar: false,
  // ★呼びかけの中の命令（run-actor.js）。相手は語彙にある本物のロボット。物はこの作品の語彙から
  actorOrders: [
    ['ろぼっと、ランタンをとれ', 'robot, take lamp'],
    ['ろぼっと、とびらをあけろ', 'robot, open door'],
    ['ろぼっと、ぼたんをおせ', 'robot, push button'],
  ],

  cmd: {
    cases: [
      // --- 基本 ---
      ['けんをとる', 'take antique sword'],
      ['剣を取る', 'take antique sword'],
      ['らんたんをとる', 'take lamp'],
      // --- 「〜から出る／降りる／去る」（桶と気球・2026-10-05）。もとは run-leave.js にあった 5 件 ---
      //   「から」は起点 = 離れる物そのもの（原作の `exit bucket` / `disembark balloon` / `leave balloon`）
      ['ばけつからでる', 'exit bucket'],
      ['おけからでる', 'exit bucket'],
      ['かごからおりる', 'disembark balloon'],
      ['かごからさる', 'leave balloon'],
      ['でる', 'exit'],
      // --- 呪文・口に出す語は本物の表で 1 件ずつ（仕組みの検査は run-spell.js）---
      ['こおりつけ', 'say "freeze"'],
      ['「井戸」と答える', 'answer "well"'],
    ],
    // ★命令形の「いけ」は池（この作品の物）ではない（2026-11-14。復唱が「きたへ涙の池」になっていた）
    shown: [
      ['きたへいけ', 'north', 'きたへ行く'],
      ['きたへいく', 'north', 'きたへ行く'],
      ['いけをみる', 'look at pool', '涙の池を見る'],   // 物としての池はこれまでどおり
      ['いけ', 'pool', '涙の池'],                       // 聞き返しへ名詞だけで答える
    ],
    passthrough: { en: 'Inside the Barrow', ja: '塚の中' },
    lines: {
      '複数対象': [
        ['sword: Taken.', '剣：取った。'],
      ],
    },
    flows: [
      ['おりる', 'かご', 'disembark balloon', '何を降りる？'],
      ['おりる', 'ばけつ', 'disembark bucket', '何を降りる？'],
    ],
  },
}
