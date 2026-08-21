#!/usr/bin/env python3
"""zork1-cmd.json → cmd_data.c/h(入力側の語彙)。

createCommander(src/command.js)の語彙構築(lex・owners・ambigDisp・sharedDisp・
others・hypernyms・DIRS・ソート)をビルド時に再現する。挙動の正典は JS。
C 側(cmd.c)は toCommand 本体だけを持つ。

使い方: python3 gen_cmd.py
"""
import json
import re
from pathlib import Path

HERE = Path(__file__).parent
asset = json.loads((HERE.parent / 'assets' / 'zork1-cmd.json').read_text())

# ---- command.js の定数 ----
DIRS = {
    '北東': 'northeast', 'ほくとう': 'northeast', '北西': 'northwest', 'ほくせい': 'northwest',
    '南東': 'southeast', 'なんとう': 'southeast', '南西': 'southwest', 'なんせい': 'southwest',
    '北': 'north', 'きた': 'north', '南': 'south', 'みなみ': 'south',
    '東': 'east', 'ひがし': 'east', '西': 'west', 'にし': 'west',
    '上': 'up', 'うえ': 'up', '下': 'down', 'した': 'down', '中': 'in', 'なか': 'in', '外': 'out', 'そと': 'out',
}
ALL_WORDS = ['ぜんぶ', 'すべて', 'のこらず', '全部', '全て', '残らず']
PARTICLES = [
    ['以外を', 'EXCEPT'], ['いがいを', 'EXCEPT'], ['以外', 'EXCEPT'], ['いがい', 'EXCEPT'],
    ['の中に', 'IN'], ['のなかに', 'IN'], ['の中へ', 'IN'], ['のなかへ', 'IN'],
    ['の下', 'UNDER'], ['のした', 'UNDER'], ['の後ろ', 'BEHIND'], ['のうしろ', 'BEHIND'],
    ['の裏', 'BEHIND'], ['のうら', 'BEHIND'],
    ['の上に', 'ON'], ['のうえに', 'ON'], ['の上', 'ON'], ['のうえ', 'ON'],
    ['を使って', 'WITH'], ['をつかって', 'WITH'], ['によって', 'WITH'],
    ['から', 'FROM'], ['へ', 'TO'], ['に', 'TO'], ['で', 'WITH'],
    ['を', 'O'], ['は', 'O'], ['が', 'O'], ['と', 'AND'],
    ['の', 'MOD'],
]
PARSER_WORDS = {
    'もういちど': 'again', 'もう一度': 'again', 'くりかえす': 'again', '繰り返す': 'again',
    'またやる': 'again', 'リピート': 'again', 'りぴーと': 'again',
}
YESNO = {'はい': 'y', 'いいえ': 'n'}
ROLE_JA = {'IN': 'の中', 'ON': 'の上', 'UNDER': 'の下', 'BEHIND': 'の後ろ', 'FROM': 'から', 'WITH': 'で'}

# UI 断片(cmd.c が合成に使う。gen_data がグリフ収集のため import する)
UI_FRAGS = ['「', '」を「', '」では受けられない', ' —— 移動は方角で言う: 北・東・上・下',
            '」に道具（', 'で）は付けられない', '何の', '何を', 'を', '？',
            '（打ち消しの言い方はまだ扱えない）', '（読み取れなかった）',
            ' は知らない言葉。別の言い方を試してほしい）', '（', '）', '・',
            '　……別の物として試す', 'など、ここには見当たらない。', '　※残: ']


def kana(s):
    out = []
    for c in str(s):
        o = ord(c)
        if 0x30A1 <= o <= 0x30F6:
            c = chr(o - 0x60)
        elif 0xFF01 <= o <= 0xFF5E:
            c = chr(o - 0xFEE0)
        if c in 'ー－—':
            c = 'ー'
        out.append(c.lower())
    return ''.join(out)


HAS_KANJI = re.compile(r'[一-龥]')

if __name__ == '__main__':
    lex = []
    verbs = asset.get('verbs', {})
    vkeys = list(verbs.keys())
    for key, v in verbs.items():
        for ja in v['ja']:
            lex.append({'ja': ja, 'disp': v['ja'][0], 'kana': kana(ja), 'kind': 'verb',
                        'key': key, 'rank': 0})

    objOf = asset.get('objects', {})
    owners = {}
    for key, o in objOf.items():
        for ja in o['ja']:
            owners.setdefault(kana(ja), []).append(key)
    nounUse = {}
    for o in objOf.values():
        n = (o['nouns'][0] if o['nouns'] else '').lower()
        nounUse[n] = nounUse.get(n, 0) + 1
    seen = set()
    ambigDisp = {}
    for key, o in objOf.items():
        noun0 = (o['nouns'][0] if o['nouns'] else '').lower()
        for ja in o['ja']:
            own = owners.get(kana(ja), [key])
            if len(own) < 2 or all((objOf[x]['nouns'][0] if objOf[x]['nouns'] else '').lower() == noun0 for x in own):
                continue
            sig = '|'.join(sorted(own))
            if not HAS_KANJI.search(ja):
                continue
            if sig not in ambigDisp or len(ja) < len(ambigDisp[sig]):
                ambigDisp[sig] = ja
    sharedDisp = {}
    for key, o in objOf.items():
        noun = (o['nouns'][0] if o['nouns'] else '').lower()
        for ja in o['ja']:
            own = owners.get(kana(ja), [])
            if len(own) < 2 or not all((objOf[x]['nouns'][0] if objOf[x]['nouns'] else '').lower() == noun for x in own):
                continue
            if not HAS_KANJI.search(ja):
                continue
            if noun not in sharedDisp or len(ja) < len(sharedDisp[noun]):
                sharedDisp[noun] = ja

    def word_of(t):
        o = objOf[t]
        n = (o['nouns'][0] if o['nouns'] else '').lower()
        a = (o['adjs'][0] if o.get('adjs') else '').lower()
        return a + ' ' + n if nounUse.get(n, 0) > 1 and a else n

    for key, o in objOf.items():
        noun = (o['nouns'][0] if o['nouns'] else '').lower()
        adj = (o['adjs'][0] if o.get('adjs') else '').lower()
        for rank, ja in enumerate(o['ja']):
            k = kana(ja)
            own = owners.get(k, [key])
            shared = len(own) > 1 and all((objOf[x]['nouns'][0] if objOf[x]['nouns'] else '').lower() == noun for x in own)
            if shared and k in seen:
                continue
            if shared:
                seen.add(k)
            if shared:
                disp = sharedDisp.get(noun, ja)
            elif len(own) > 1:
                disp = ambigDisp.get('|'.join(sorted(own)), ja)
            else:
                disp = o['ja'][0]
            word = noun if (shared or not (nounUse.get(noun, 0) > 1 and adj)) else adj + ' ' + noun
            vehicle = any(objOf[x].get('vehicle') for x in own) if shared else bool(o.get('vehicle'))
            others = [word_of(x) for x in own
                      if x != key and (objOf[x]['nouns'][0] if objOf[x]['nouns'] else '').lower() != noun]
            lex.append({'ja': ja, 'disp': disp, 'kana': k, 'kind': 'obj', 'key': key,
                        'rank': rank, 'word': word, 'vehicle': vehicle, 'others': others})

    for ja in ALL_WORDS:
        lex.append({'ja': ja, 'disp': ALL_WORDS[0], 'kana': kana(ja), 'kind': 'obj', 'key': '*ALL*',
                    'word': 'all', 'rank': 0, 'others': [], 'vehicle': False})

    for h in asset.get('hypernyms', []):
        targets = [t for t in h.get('targets', []) if t in objOf]
        if not targets:
            continue
        vehicle = any(objOf[t].get('vehicle') for t in targets)
        word = h['noun'].lower() if h.get('noun') else word_of(targets[0])
        others = [] if h.get('noun') else [word_of(t) for t in targets[1:]]
        for ja in [h['form']] + h.get('yomi', []):
            lex.append({'ja': ja, 'disp': h['form'], 'kana': kana(ja), 'kind': 'obj', 'key': '*H*',
                        'rank': 0, 'word': word, 'others': others, 'vehicle': vehicle})

    for ja, en in DIRS.items():
        lex.append({'ja': ja, 'disp': ja, 'kana': kana(ja), 'kind': 'dir', 'word': en, 'rank': 0})

    lex.sort(key=lambda e: (-len(e['kana']), e.get('rank', 0)))

    # ---- C 出力 ----
    jpool = []      # u16
    apool = bytearray()

    def jput(s):
        off = len(jpool)
        jpool.extend(ord(c) for c in s)
        return off, len(s)

    def aput(s):
        b = s.encode('ascii')
        off = len(apool)
        apool.extend(b)
        return off, len(b)

    # 動詞表: key 名 + shapes ビット + 形フラグ
    TOKENS = ['IN', 'ON', 'AT', 'TO', 'UNDER', 'BEHIND', 'FROM', 'WITH', 'DOWN', 'OBJ']
    vrows = []
    for key in vkeys:
        shapes = verbs[key].get('shapes', [])
        mask = 0
        for sh in shapes:
            for tok in sh.split(' '):
                if tok in TOKENS:
                    mask |= 1 << TOKENS.index(tok)
        bare_ok = any(sh == 'OBJ' or sh.startswith('OBJ ') for sh in shapes)
        bare_ok2 = any(sh == '' for sh in shapes)
        ko, kl = aput(key.lower())
        d = verbs[key]['ja'][0]
        do_, dl = jput(d)
        vrows.append((ko, kl, do_, dl, mask, 1 if bare_ok else 0, 1 if bare_ok2 else 0))

    KIND = {'verb': 0, 'obj': 1, 'dir': 2}
    lrows = []
    others_pool = []    # (off,len) 列。lex は others の先頭 index + 個数
    for e in lex:
        kn, knl = jput(e['kana'])
        dp, dpl = jput(e['disp'])
        jo, jl = jput(e['ja'])
        if e['kind'] == 'verb':
            wo, wl = 0, 0
            vidx = vkeys.index(e['key'])
        else:
            wo, wl = aput(e['word'])
            vidx = -1
        ooff = len(others_pool)
        for w in e.get('others', []):
            others_pool.append(aput(w))
        lrows.append((kn, knl, dp, dpl, jo, jl, wo, wl, KIND[e['kind']], vidx,
                      1 if e.get('vehicle') else 0, ooff, len(e.get('others', [])),
                      1 if e.get('key') == '*ALL*' else 0))

    prows = []
    ROLES = ['O', 'WITH', 'TO', 'IN', 'ON', 'UNDER', 'BEHIND', 'FROM', 'AND', 'EXCEPT', 'MOD']
    for p, r in PARTICLES:
        po, pl = jput(kana(p))
        do_, dl = jput(p)
        prows.append((po, pl, do_, dl, ROLES.index(r)))

    def simple_map(d, out):
        rows = []
        for k, v in d.items():
            ko, kl = jput(kana(k))
            vo, vl = aput(v)
            rows.append((ko, kl, vo, vl))
        return rows

    pw_rows = simple_map(PARSER_WORDS, None)
    yn_rows = simple_map(YESNO, None)

    guide = asset.get('guide', {})
    grows = []
    for k, v in guide.items():
        ko, kl = jput(kana(k))
        vo, vl = jput(v)
        grows.append((ko, kl, vo, vl))

    frows = [jput(s) for s in UI_FRAGS]
    role_ja_rows = [jput(ROLE_JA.get(r, '')) for r in ROLES]

    with open(HERE / 'cmd_data.h', 'w') as f:
        f.write('/* gen_cmd.py が生成。手で編集しない */\n#ifndef CMD_DATA_H\n#define CMD_DATA_H\n')
        f.write('typedef struct { unsigned int kn; unsigned short knl; unsigned int dp; unsigned short dpl;\n'
                '  unsigned int jo; unsigned short jl; unsigned int wo; unsigned short wl;\n'
                '  unsigned char kind, vehicle, is_all; unsigned short ooff, on_; short vidx; } CmLex;\n')
        f.write('typedef struct { unsigned int ko; unsigned short kl; unsigned int jo; unsigned short jl;\n'
                '  unsigned short mask; unsigned char bare_ok, bare_ok2; } CmVerb;\n')
        f.write('typedef struct { unsigned int po; unsigned short pl; unsigned int dp; unsigned short dpl;'
                ' unsigned char role; } CmPart;\n')
        f.write('typedef struct { unsigned int ko; unsigned short kl; unsigned int vo; unsigned short vl; } CmMap;\n')
        f.write('typedef struct { unsigned int off; unsigned short len; } CmStr;\n')
        f.write('enum { CMK_VERB, CMK_OBJ, CMK_DIR };\n')
        f.write('enum { CMR_O, CMR_WITH, CMR_TO, CMR_IN, CMR_ON, CMR_UNDER, CMR_BEHIND, CMR_FROM,'
                ' CMR_AND, CMR_EXCEPT, CMR_MOD, CMR_NONE };\n')
        f.write('enum { CMT_IN = 1, CMT_ON = 2, CMT_AT = 4, CMT_TO = 8, CMT_UNDER = 16,'
                ' CMT_BEHIND = 32, CMT_FROM = 64, CMT_WITH = 128, CMT_DOWN = 256, CMT_OBJ = 512 };\n')
        all_idx = next(i for i, e in enumerate(lex) if e.get('key') == '*ALL*' and e['ja'] == ALL_WORDS[0])
        f.write(f'enum {{ CM_ALL_LEX = {all_idx} }};\n')
        f.write(f'enum {{ CM_LEX_N = {len(lrows)}, CM_VERB_N = {len(vrows)}, CM_PART_N = {len(prows)},'
                f' CM_PW_N = {len(pw_rows)}, CM_YN_N = {len(yn_rows)}, CM_GUIDE_N = {len(grows)},'
                f' CM_FRAG_N = {len(frows)}, CM_ROLE_N = {len(ROLES)} }};\n')
        for k in ['VK_WALK', 'VK_CLIMB', 'VK_DISEMBARK', 'VK_ENTER', 'VK_EXIT', 'VK_LEAVE']:
            name = k[3:]
            f.write(f'enum {{ {k} = {vkeys.index(name) if name in vkeys else -1} }};\n')
        f.write('extern const unsigned short cm_jpool[];\nextern const char cm_apool[];\n')
        f.write('extern const CmLex cm_lex[CM_LEX_N];\nextern const CmVerb cm_verbs[CM_VERB_N];\n')
        f.write('extern const CmPart cm_parts[CM_PART_N];\n')
        f.write('extern const CmMap cm_pwords[CM_PW_N];\nextern const CmMap cm_yesno[CM_YN_N];\n')
        f.write('extern const CmMap cm_guide[CM_GUIDE_N];\n')
        f.write('extern const CmStr cm_frags[CM_FRAG_N];\nextern const CmStr cm_others[];\n')
        f.write('extern const CmStr cm_role_ja[CM_ROLE_N];\n')
        f.write('#endif\n')

    with open(HERE / 'cmd_data.c', 'w') as f:
        f.write('/* gen_cmd.py が生成。手で編集しない */\n#include "cmd_data.h"\n')
        f.write('const unsigned short cm_jpool[] = {\n  ' + ','.join(f'0x{c:04X}' for c in jpool) + '\n};\n')
        f.write('const char cm_apool[] = {\n  ' + ','.join(str(b) for b in apool) + '\n};\n')
        f.write('const CmLex cm_lex[] = {\n' + ''.join(
            f'  {{{a},{b},{c},{d},{e},{g},{h},{i},{j},{k},{m},{n},{o},{p}}},\n'
            for a, b, c, d, e, g, h, i, j, k, m, n, o, p in
            [(r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], r[8], r[10], r[13], r[11], r[12], r[9])
             for r in lrows]) + '};\n')
        # ↑ 並び: kn,knl,dp,dpl,jo,jl,wo,wl,kind,vehicle,is_all,ooff,on_,vidx
        f.write('const CmVerb cm_verbs[] = {\n' + ''.join(
            f'  {{{a},{b},{c},{d},{e},{g},{h}}},\n' for a, b, c, d, e, g, h in vrows) + '};\n')
        f.write('const CmPart cm_parts[] = {\n' + ''.join(
            f'  {{{a},{b},{c},{d},{e}}},\n' for a, b, c, d, e in prows) + '};\n')
        for name, rows in [('cm_pwords', pw_rows), ('cm_yesno', yn_rows), ('cm_guide', grows)]:
            f.write(f'const CmMap {name}[] = {{\n' + ''.join(
                f'  {{{a},{b},{c},{d}}},\n' for a, b, c, d in rows) + '};\n')
        f.write('const CmStr cm_frags[] = {\n' + ''.join(
            f'  {{{a},{b}}},\n' for a, b in frows) + '};\n')
        f.write('const CmStr cm_others[] = {\n' + (''.join(
            f'  {{{a},{b}}},\n' for a, b in others_pool) or '  {0,0},\n') + '};\n')
        f.write('const CmStr cm_role_ja[] = {\n' + ''.join(
            f'  {{{a},{b}}},\n' for a, b in role_ja_rows) + '};\n')

    print(f'cmd_data: lex {len(lrows)} / verbs {len(vrows)} / parts {len(prows)} '
          f'/ guide {len(grows)} / jpool {len(jpool) * 2}B / apool {len(apool)}B')
