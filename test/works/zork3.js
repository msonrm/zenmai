'use strict'
/**
 * Zork III の検査の材料（`test/work.js` の `spec()` が読む）。書き方は `zork1.js` と同じ。
 * ★`native/make_corpus.py` がこのファイルの字面から入力を吸い上げる（行頭の `['…',`）
 */
module.exports = {
  // --- 作品の事実 ---
  start: 'Endless Stair',
  save: { cmds: ['take lamp', 'save', 's', 'w', 'restore', 'look'] },
  // ★通し（英語の手）= 237 手で最後まで（2026-11-15）。乱数は固定なので、**1 手でも足し引きすると後ろが崩れる**（卓の表示は 4 手ごと・人影と老人は乱数）
  walkthrough: 'walkthrough-zork3.txt',
  // ★通しの最後に着く場所 = 結末（ゾークの宝物庫）。手の列が途中で崩れたら赤になる（2026-11-15）
  walkthroughEnd: 'Treasury of Zork',
  walkthroughJaEnd: 'Treasury of Zork',
  // ★日本語の通し = 英語の通しと 1 手ずつ対応（送る英語が同じ意味になること。出力の行数も英語の通しと同じ 623 行・2026-11-15）
  walkthroughJa: 'walkthrough-zork3-ja.txt',
  // ★1 字のコマンド不適語は無い（run-swallow.js のカナリアは鳴らさない）
  nocmdOneChar: false,
  // ★呼びかけの相手はダンジョン・マスター（終盤に `master, follow` / `master, stay` / `master, push button` と命じる）
  actorOrders: [
    ['ますたー、ついてこい', 'master, follow'],
    ['マスター、とまれ', 'master, stay'],
    ['だんじょんますたー、ぼたんをおせ', 'master, push button'],
  ],

  cmd: {
    cases: [
      // --- 基本 ---
      ['らんたんをとる', 'take lamp'],
      ['らんたんをつける', 'light lamp'],
      ['けんをとる', 'take sword'],
      // --- 崖の箱とロープ ---
      ['はこをロープにむすぶ', 'tie chest to rope'],
      ['ロープをつかむ', 'take rope'],
      // --- 老人に旅の糧食を渡す ---
      ['ぱんをろうじんにわたす', 'give bread to old man'],
      // --- 影の人影の頭巾 ---
      ['ずきんをとる', 'take hood'],
      // --- 王家の難題の壁（2026-11-15。どの部屋にもある壁と同じ日本語を持たせると、方角が落ちて `push wall` になった）---
      ['みなみのかべをおす', 'push south wall'],
      ['南の壁を押す', 'push south wall'],
      ['ひがしのかべをおす', 'push east wall'],
      // --- 日本語の通しで拾ったもの（2026-11-15）---
      ['まつ', 'wait'],                                         // 「待つ」は STAY（マスターへの「ここにいろ」）ではない
      ['マホガニーのパネルをおす', 'push mahogany wall'],         // カタカナだけの名前を 2 度数えて形容詞を落としていた
      ['きのとびらをあける', 'open wood door'],                   // 2 つの木の扉が共有する言い方 = 共通の形容詞を添える（裸の door だと石・鉄の扉まで聞き返された）
      ['きんいろのきかいをみなみへおす', 'push temporizer south'], // 物を方角へ押す（PUSH OBJECT OBJECT）
      ['ゆびわをざせきのしたにかくす', 'put ring under seat'],
      ['かぎでせいどうのとびらのじょうをあける', 'unlock bronze door with key'],
      // --- 数字はそのまま原作へ渡す（2026-11-15。全角も受ける）---
      ['ひどけいを8にまわす', 'turn sundial to 8'],
      ['ひどけいを４にあわせる', 'turn sundial to 4'],
      // --- 呪文（2 語。秘密の扉の前で唱える）---
      ['フロッツ・オズモー', 'say "frotz ozmoo"'],
      ['ふろっつおずもーととなえる', 'say "frotz ozmoo"'],
    ],
    // ★「ダイヤル」は時間移動機と日時計の両方にある = 候補が 2 つ揃うこと（どちらが居るかは原作のスコープが決める）
    candidates: [
      { say: 'ダイヤルを776にあわせる', label: '時間移動機と日時計', want: ['turn console to 776', 'turn sundial to 776'] },
    ],
    passthrough: { en: 'Endless Stair', ja: '果てしない階段' },
  },
}
