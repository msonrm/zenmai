#!/usr/bin/env python3
"""labo のゴールデン(japanese.json / english.json)→ golden_cases.h(C 呼び出し列)。

test_input.c のヘルパ関数呼び出しへ機械変換する。期待値の意味論は
labo scripts/run-gamepad-golden.mjs と同一。
使い方: python3 gen_golden_c.py <golden dir>
"""
import json
import sys
from pathlib import Path

GOLDEN = Path(sys.argv[1])


def u16(s):
    if not s:
        return 'NULL, 0'
    return '(const unsigned short[]){' + ','.join(f'0x{ord(c):04X}' for c in s) + '}, ' + str(len(s))


def cstr(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


def key_tap(name):
    """keyTap 相当: Space だけ key が " " になる。"""
    return ' ' if name == 'Space' else name


out = ['/* gen_golden_c.py が生成。手で編集しない */', 'static void run_cases(void) {']
total = 0
for fname in ['japanese.json', 'english.json']:
    data = json.loads((GOLDEN / fname).read_text())
    for case in data['cases']:
        total += 1
        eng = 1 if case.get('lang') == 'english' else 0
        out.append(f'  case_begin({cstr(case["name"])}, {eng});')

        def emit_expect(spec, where):
            if 'kana' in spec:
                out.append(f'  check_kana({u16(spec["kana"])}, {cstr(where)});')
            for field, fn in [('keys', 'check_keys'), ('shiftKeys', 'check_skeys'), ('ctrlKeys', 'check_ckeys')]:
                if field in spec:
                    ks = spec[field]
                    arr = '(const char *const[]){' + (','.join(cstr(k) for k in ks) or 'NULL') + '}'
                    out.append(f'  {fn}({arr}, {len(ks)}, {cstr(where)});')
            if 'ops' in spec:
                out.append(f'  check_ops({spec["ops"]}, {cstr(where)});')

        for i, step in enumerate(case['steps']):
            where = f'steps[{i}]'
            if 'frame' in step:
                f = {'now': 0, 'row': 0, 'vowel': -1, 'vowelNow': 0, 'ltNow': 0,
                     'rtNow': 0, 'consonantCount': 0}
                for k, v in step['frame'].items():
                    f[k] = (-1 if v is None else int(v))
                out.append(f'  do_frame((GpFrame){{{f["now"]},{f["row"]},{f["vowel"]},'
                           f'{f["vowelNow"]},{f["ltNow"]},{f["rtNow"]},{f["consonantCount"]}}});')
            elif 'action' in step:
                a = step['action']
                t = a['type']
                if t == 'kana':
                    out.append(f'  act_kana({u16(a["char"])}, {a.get("replace", 0)});')
                elif t == 'youon':
                    out.append('  act_youon();')
                elif t == 'toggleDakuten':
                    out.append('  act_dakuten();')
                elif t == 'space':
                    out.append('  act_kana((const unsigned short[]){0x20}, 1, 0);')
                elif t == 'deleteBack':
                    out.append('  act_key("Backspace", 0, 0);')
                elif t == 'cancel':
                    out.append('  act_key("Escape", 0, 0);')
                elif t == 'confirmOrNewline':
                    out.append('  act_key("Enter", 0, 0);')
                elif t == 'navKey':
                    out.append(f'  act_key({cstr(key_tap(a["key"]))}, {1 if a.get("shift") else 0}, 0);')
                elif t == 'undoCommit':
                    out.append('  act_key("Backspace", 0, 1);')
                elif t == 'redo':
                    out.append('  act_key("y", 0, 1);')
                else:
                    raise SystemExit(f'未対応 action: {t}')
            elif 'setTail' in step:
                out.append(f'  set_tail({u16(step["setTail"])});')
            elif 'assert' in step:
                emit_expect(step['assert'], where)
        emit_expect(case.get('expect', {}), 'expect')
out.append('}')
Path(__file__).parent.joinpath('golden_cases.h').write_text('\n'.join(out) + '\n')
print(f'golden_cases.h: {total} ケース')
