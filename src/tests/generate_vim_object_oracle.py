# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 rodioniurev
"""Generate regression fixtures with an unconfigured Vim (run under WSL).

No implementation code is used to compute the expected results.
"""
import json
import tempfile
import subprocess
from pathlib import Path

source = Path(__file__).resolve().parent
scratch = tempfile.TemporaryDirectory()
root = Path(scratch.name)
cases = []
samples = [
    ('words', 'one two three', 'wW', [0, 1, 2, 3, 4, 7, 12]),
    ('spaces', '  one   two  ', 'wW', [0, 1, 2, 4, 5, 7, 8, 11, 12]),
    ('punctuation', 'aa,bb... cc_dd! zz', 'wW', [0, 2, 3, 5, 8, 9, 14, 16]),
    ('tabs', '\tone\t\ttwo\t', 'wW', [0, 1, 4, 5, 6, 9]),
    ('unicode', 'один два,три', 'wW', [0, 2, 4, 5, 8, 9]),
    ('multiline', 'one two\nthree four\n\nfive', 'wW', [0, 4, 6, 8, 13, 18, 19, 20]),
    ('empty', '', 'wW', [0]),
    ('only-spaces', '   ', 'wW', [0, 1, 2]),
]
for label, text, objects, positions in samples:
    for obj in objects:
        for pos in positions:
            for around in 'ia':
                for count in ['', '2', '3']:
                    for op in ['d', 'c', 'y', 'gu', 'gU', 'g~']:
                        command = op + count + around + obj
                        cases.append(dict(name=f'{label}-{pos}-{command}', text=text, pos=pos, command=command))
for left, right, aliases in [('(', ')', '()b'), ('{', '}', '{}B'), ('[', ']', '[]'), ('<', '>', '<>')]:
    for label, text in [('simple', f'pre {left}one two{right} post'), ('nested', f'{left}a{left}b{right}c{right}'), ('empty', left+right), ('multiline', left+'\n  one\n'+right), ('missing', 'one two')]:
        for pos in sorted(set([0, len(text)//2, len(text)-1])):
            for obj in aliases:
                for around in 'ia':
                    for count in ['', '2']:
                        for op in ['d', 'c', 'y']:
                            cmd = op+count+around+obj
                            cases.append(dict(name=f'block-{ord(left)}-{label}-{pos}-{cmd}',text=text,pos=pos,command=cmd))
for quote in ['"', "'", '`']:
    for label,text in [('simple', f'pre {quote}one two{quote} post'),('empty',quote+quote),('escaped',f'{quote}one\\{quote}two{quote}'),('pairs',f'{quote}one{quote} and {quote}two{quote}')]:
        for pos in sorted(set([0,len(text)//2,len(text)-1])):
            for around in 'ia':
                for count in ['', '2']:
                    for op in ['d','c','y']:
                        cmd=op+count+around+quote
                        cases.append(dict(name=f'quote-{ord(quote)}-{label}-{pos}-{cmd}',text=text,pos=pos,command=cmd))
for obj,text in [('p','one\ntwo\n\n\nthree\nfour\n\nfive'),('s','One thing.  Two things! Third?\nLast sentence.')]:
    for pos in [0,4,8,10,14,len(text)-1]:
        for around in 'ia':
            for count in ['', '2']:
                for op in ['d','c','y']:
                    cmd=op+count+around+obj
                    cases.append(dict(name=f'{obj}-{pos}-{cmd}',text=text,pos=pos,command=cmd))
for label,text,positions in [('word','one two three four five',[0,1,3,4,8]), ('block','(a(b)c)',[0,3,6]), ('tag','<p>one <b>two</b> three</p>',[0,5,10,22])]:
    objects = 'wW' if label == 'word' else '()b' if label == 'block' else 't'
    for pos in positions:
        for obj in objects:
            for prefix in 'ia':
                for op in ['d','y','gU']:
                    for cmd in [op+prefix+obj, '2'+op+prefix+obj, op+'2'+prefix+obj, '2'+op+'2'+prefix+obj,
                                'v'+prefix+obj+op, 'V'+prefix+obj+op, 'v'+prefix+obj+prefix+obj+op]:
                        cases.append(dict(name=f'extra-{label}-{pos}-{cmd}',text=text,pos=pos,command=cmd))
for case in cases:
    prefix=case['text'][:case['pos']]
    case['line']=prefix.count('\n')
    case['column']=len(prefix.rsplit('\n',1)[-1])
    case['bytecolumn']=len(prefix.rsplit('\n',1)[-1].encode())+1
(root/'oracle-input.json').write_text(json.dumps(cases,ensure_ascii=False))
script = r'''
set nocompatible encoding=utf-8 nomore noautoindent nosmartindent expandtab tabstop=4 shiftwidth=4
let cases = json_decode(join(readfile('oracle-input.json'), "\n"))
let output = []
for c in cases
  enew!
  call setline(1, split(c.text, "\n", 1))
  call cursor(c.line + 1, c.bytecolumn)
  call setreg('"', '')
  let cmd = c.command
  if cmd[0] == 'c'
    let cmd .= "\<Cmd>let g:inserted = mode() ==# 'i'\<CR>X\<Esc>"
  endif
  execute 'silent! normal! ' . cmd
  let c.insert = c.command[0] == 'c' && g:inserted
  if c.command[0] == 'c' && !g:inserted
    enew!
    call setline(1, split(c.text, "\n", 1))
    call cursor(c.line + 1, c.bytecolumn)
    call setreg('"', '')
    execute 'silent! normal! ' . c.command . "\<Esc>"
  endif
  let c.expected = join(getline(1, '$'), "\n")
  let c.register = getreg('"')
  call add(output, c)
endfor
call writefile([json_encode(output)], 'oracle-output.json')
qa!
'''
(root/'oracle.vim').write_text(script)
subprocess.run(['vim','-Nu','NONE','-n','-es','-S','oracle.vim'],cwd=root,check=True)
data=json.loads((root/'oracle-output.json').read_text())
dest=source/'vim_object_oracle.json'
dest.write_text(json.dumps(data,ensure_ascii=False,indent=1)+'\n')
print(f'{len(data)} reference cases -> {dest}')
