# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 rodioniurev
"""Generate extension fixtures using an unconfigured Vim (run under WSL)."""
import json
import sys
import subprocess
import tempfile
from pathlib import Path

cases = []
for lhs in ['teh', '#i', ';;', 'x']:
    for prefix in ['', ' ', 'a', '(', '>']:
        for trigger in [' ', '.', '\x1b', '\x1d', '\x03', '\r']:
            keys = 'i' + prefix + lhs + trigger
            if trigger not in ['\x1b', '\x03']:
                keys += '\x1b'
            cases.append(dict(name=f'ab-{lhs}-{ord(prefix or "0")}-{ord(trigger)}', text='', pos=0,
                              definitions=[f'iabbrev {lhs} EXPANDED'], keys=keys))
for text, pos, keys in [('ateh', 1, 'iteh \x1b'), ('teh', 0, 'A \x1b'), ('abc', 1, 'iteh\x1b')]:
    cases.append(dict(name=f'insert-boundary-{len(cases)}', text=text, pos=pos, definitions=['iabbrev teh EXPANDED'], keys=keys))
for text in ['foo.bar baz', 'one  two three', 'a,b.c!', '  one two  ']:
    for pos in range(len(text)):
        for keys in ['ge', 'gE', '2ge', 'g_']:
            cases.append(dict(name=f'motion-{len(cases)}', text=text, pos=pos, definitions=[], keys=keys))
with tempfile.TemporaryDirectory() as scratch:
    root = Path(scratch)
    (root / 'input.json').write_text(json.dumps(cases))
    script = r"""
set nocompatible encoding=utf-8 nomore noswapfile
" Disable typeahead batching: Qt sends individual key events too.
" A no-op InsertCharPre hook preserves character-by-character semantics.
autocmd InsertCharPre * let g:oracle_char = v:char
let cases = json_decode(join(readfile('input.json'), "\n"))
for c in cases
  enew!
  abclear
  mapclear
  mapclear!
  call setline(1, split(c.text, "\n", 1))
  call cursor(1, c.pos + 1)
  for definition in c.definitions
    execute definition
  endfor
  call feedkeys(c.keys, 'xt')
  let c.expected = join(getline(1, '$'), "\n")
  let c.column = col('.') - 1
  let c.line = line('.') - 1
endfor
call writefile([json_encode(cases)], 'result.json')
qa!
"""
    (root / 'oracle.vim').write_text(script)
    result = []
    # Use a fresh process: Ctrl-C/abbreviation typeahead state must not leak
    # between independent scenarios.
    for case in cases:
        (root / 'input.json').write_text(json.dumps([case]))
        subprocess.run([sys.argv[1] if len(sys.argv) > 1 else 'vim', '-Nu', 'NONE', '-n', '-es', '-S', 'oracle.vim'], cwd=root, check=True)
        result.extend(json.loads((root / 'result.json').read_text()))
    output = Path(__file__).with_name('nvim_extension_oracle.json' if len(sys.argv) > 1 and sys.argv[1] == 'nvim' else 'vim_extension_oracle.json')
    output.write_text(json.dumps(result, indent=2) + '\n')
    print(f'{len(result)} Vim fixtures written to {output}')
