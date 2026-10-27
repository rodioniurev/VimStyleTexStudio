# Vim editing in TeXstudio

Enable **Options → Configure TeXstudio → Editor → Enable Vim editing**.
The saved setting is `Editor/VimMode`; it applies to existing and new tabs.
The provided Windows package enables it in its own profile.

Normal mode interprets commands. `i` starts Insert; `Esc` or `Ctrl+[` returns to
Normal. Saving and Build & View work while Insert/Replace is active; Escape is
not required first. Saving creates an undo boundary and typing can continue.
Use the English keyboard layout for commands; text input can use any
layout. The editor status panel shows the mode. `:`, `/` and `?` open a command
line at the bottom of the editor, even if the status panel is hidden. Enter
executes, Esc cancels; arrows, Home/End, Backspace/Delete and Ctrl+U edit the
command; Up/Down recall up to 100 entries per prompt.

## Implemented Normal and Visual commands

| Keys | Action |
| --- | --- |
| `i a I A`, `o O` | Insert before/after, at first nonblank/end, open below/above |
| `R`, `r{char}`, `[count]r{char}` | Replace mode; replace characters on the current line |
| `h j k l`, arrows, Space, Backspace | Character/line movement |
| `w b e`, `W B E`, `ge gE` | Word/WORD movements; previous word end |
| `0 ^ $`, `N|`, `+ -`, Enter | Line boundaries, column N, adjacent line's first nonblank |
| `gg G`, `Ngg NG`, `N%` | First/last/specified line, percentage through document |
| `%` | Match the next bracket `() [] {}` on the current line |
| `{ }`, `( )` | Paragraph/sentence movement |
| `H M L` | Top/middle/bottom visible logical line |
| `Ctrl+d/u`, `Ctrl+f/b`, PageDown/Up | Half-page/page movement |
| `Ctrl+e/y`, `zt zz zb` | Scroll; position cursor line at top/middle/bottom |
| `f F t T` + character, `; ,` | Find/till a character on this line; repeat/reverse |
| `d c y` + motion | Delete/change/yank; `dd cc yy` operate on lines |
| `x X s S D C Y` | Delete forward/backward, substitute characters/lines, delete/change to end, yank lines |
| `> <` + motion, `>> <<` | Indent/unindent with the document tab width |
| `gu gU g~` + motion/object; `guu gUU g~~` | Lowercase, uppercase, toggle case |
| `~` | Toggle character case and advance |
| `gj gk`, `g0 g^ g$ gm`, `g_`, `Ngo` | Wrapped-line movement/boundaries, last nonblank, logical character offset |
| `gi`, `g;`, `g,` | Resume at the last insertion point; navigate recent change positions |
| `J`, `gJ` | Join with whitespace cleanup / join without whitespace cleanup |
| `p P` | Paste after/before; preserve character/line/block register kind |
| `v V Ctrl+v` | Character/line/rectangular Visual mode |
| Visual `o`, `gv` | Swap selection endpoints; restore previous selection |
| Visual `d c y x s`, `u U ~`, `> <`, `r{char}`, `p P` | Selection operations |
| Block Visual `d c y`, `I A` | Delete/change/yank columns; insert at left/right in all selected rows |
| `u`, `Ctrl+r` | Undo/redo; one Insert/change session is one undo group |
| `.`, `[count].` | Replay the last non-Ex change |
| `q{letter}` … `q`, `@{letter}`, `@@`, `N@a` | Record/play/repeat a macro; uppercase recording register appends |
| `m{letter}`, backtick + letter, apostrophe + letter | Local mark; exact/first-nonblank jump |
| `Ctrl+o`, `Ctrl+i` / Tab | Back/forward through local jump history |
| `Ctrl+a`, `Ctrl+x` | Add/subtract a count to decimal or positive `0x` integer |
| `gt gT`, `gf` | Next/previous document; open WORD under cursor as a path |
| `ZZ`, `ZQ` | Save if needed and close; discard changes and close |

Counts apply to motions, operators, paste, replace, undo/redo and macro playback:
`3w`, `2dd`, `d2w`, `2d3w`, `3p`. `i a I A R` support counted input, e.g.
`3iabc<Esc>`. `o/O` repeat the entered line for the requested count.
Counts are capped at 10,000. Macros have a depth limit of 10 and a shared budget
of 100,000 replayed key events per invocation.

## Text objects and registers

Use `i` (inner) or `a` (around) after an operator or in Visual mode:

- `iw aw`, `iW aW`: word / whitespace-separated WORD.
- `i( a(`, `i) a)`, `ib ab`: balanced parentheses.
- `i{ a{`, `i} a}`, `iB aB`: balanced braces, including LaTeX argument braces.
- `i[ a[`, `i] a]`, `i< a<`, `i> a>`: brackets/angle brackets.
- `i" a"`, `i' a'`, inner/around backticks: quoted text on this line.
- `ip ap`, `is as`: paragraph / sentence.
- `it at`: contents / complete HTML or XML tag block, including nested tags.

Word objects work from any character in a word. `aw` on whitespace includes
the following word; `iw` selects that whitespace. Counts for `iw` count both
words and whitespace runs (`d2iw` on `one two` removes `one `). Counts before
and after an operator multiply (`2d2aw`). The same objects work with `c`, `d`,
`y`, case operators and Visual selection. `ciw`/`caw` enter Insert; type the
replacement and press Esc. Logical offsets are independent of LF/CRLF endings.

Examples: `ciw`, `daw`, `ci{`, `yap`, `vi"`, `gUiw`, `d2i(`. Balanced bracket
objects support counts for outer nesting levels; paragraph counts are not expanded.

Prefix an operation with `"{register}`. Named registers `a-z`, append registers
`A-Z`, macros, the last inserted text register `.` and numbered registers are
shared across tabs. `0` holds the last yank; `1-9` hold line/multiline deletes;
`-` holds the last small delete. `_` discards without changing the clipboard.
The unnamed register and `+`/`*` use the Windows system clipboard; changes in
other applications replace that content. Named registers remain available.
Special registers `/`, `:`, `%` provide the last search, last Ex command and current filename.
Insert-mode `Ctrl+r{register}` inserts a register; `Ctrl+w` erases the preceding
word, `Ctrl+u` erases to the line start. `Ctrl+o` runs one Normal command and
returns to Insert. `Ctrl+t/d` add/remove indentation (`shiftwidth`, falling back
to `tabstop`); `Ctrl+e/y` copy a character from below/above, and `Ctrl+a` inserts
the last inserted text. `Ctrl+v` quotes the next typed character (numeric codes
and digraphs are not implemented). These are also recorded in macros.

## Search

`/pattern<Enter>` searches forward; `?pattern<Enter>` searches backward.
`n/N` repeat in the same/opposite direction; `g*`/`g#` search without word boundaries; `*/#` search the word under the
cursor forward/backward. Search wraps. An empty prompt repeats the last pattern.
Operators accept searches, for example `d/section<Enter>`.

Patterns use **Qt/PCRE regular expressions**, not Vim's complete regex language.
Word boundaries `\<` and `\>` are accepted as `\b`. Search moves the cursor;
it does not highlight all matches. `:set ignorecase smartcase` affects search.

## Ex commands

| Commands | Behavior |
| --- | --- |
| `:w`, `:write`, `:w filename` | Native TeXstudio save/save-as |
| `:wq`, `:x`, `:xit`, `ZZ` | Save and close; x/ZZ skip writing an unchanged buffer |
| `:q`, `:quit`, `:bd`, `:bdelete` | Close with TeXstudio's normal unsaved-changes dialog |
| `:q!`, `:quit!`, `ZQ` | Discard unsaved changes and close this tab |
| `:qa`, `:qall`, `:wa`, `:wall` | Native close-all / save-all |
| `:e filename`, `:edit filename`, `gf` | Open a file; :e without a path opens the native Open dialog |
| `:enew`, `:tabnew [filename]` | New buffer / open a file |
| `:bn`, `:bnext`, `:tabn`, `:tabnext`; `:bp`, `:bprevious`, `:tabp`, `:tabprevious` | Next/previous document |
| `:N`, `:$` | Jump to a line |
| `:d`, `:delete`, `:y`, `:yank`, `:>`, `:<`, `:join` | Line-range operations |
| `:s/pattern/replacement/flags` | Substitute on the current line or specified range |
| `:g/pattern/d`, `:v/pattern/d` | Delete matching/nonmatching lines |
| `:global/pattern/{action}`, `:vglobal/pattern/{action}` | Actions: delete, yank, print, substitute |
| `:sort`, `:sort!`, `:sort u` | Sort ascending/descending; optionally remove duplicates |
| `:normal keys`, `:normal! keys` | Execute commands on the current line/range; accepts `<Esc>`, `<CR>`, `<Tab>`, `<C-r>` notation |
| `:copy N`, `:t N`, `:move N` | Copy/move addressed lines after N (`0`, `.`, `$` also accepted) |
| `:put [register]`, `:put! [register]` | Insert register contents as lines after/before the address |
| `:source filename`, `:so filename` | Read a UTF-8 file of supported Ex commands; blank lines and comment lines are skipped |
| `:&`, `:s` | Repeat the last substitution |
| `:r filename`, `:read filename` | Read a UTF-8 file after the addressed line |
| `:u`, `:undo`, `:red`, `:redo` | Undo/redo |
| `:reg`, `:registers`, `:marks`, `:help` | Display registers, local marks, help location |

Addresses: numbers (1-based), `.`, `$`, `%`, local marks (`'a`), search
addresses (`/pattern/`, `?pattern?`), relative offsets (`.+2`, `$-1`),
`first,last` and `first;last` (second address relative to the first). Visual selections
provide the default range. Examples: `:2,5d`, `:%s/foo/bar/g`, `:'<,'>s/a/b/`,
`:%normal I%<Esc>`. Writes of a partial line range are explicitly rejected.

Substitute flags: `g` all matches per line, `i` ignore case, `I` force case,
`n` count without changing. Replacement supports `&` (whole match), `\1`…`\9`
(captures), `\r`/`\n` (new line); any nonletter delimiter can be used.
Unsupported flags, including confirmation flag `c`, report an error without editing.

`:set` / `:setlocal` support `ignorecase/ic`, `smartcase/scs`, `wrap`,
`autoindent/ai`, `expandtab/et` and their `no` variants; `tabstop=N` / `ts=N`
accepts 1–32. These options affect the current session; tabstop is shared by the
underlying QCodeEdit component. Use the regular settings dialog for persistence.

## Abbreviations and mappings

Examples (enter in Normal mode):

```vim
:iabbrev teh the
:iabbrev qq \qquad
:ab btw by the way
:inoreabbrev sig Best regards,<CR>Rodion
:nnoremap <Leader>w :w<CR>
:inoremap jj <Esc>
```

Type `teh` followed by a space to get `the `. `Esc` and `Ctrl+]` also expand an
abbreviation; `Ctrl+]` adds no separator, while `Ctrl+c` leaves Insert without
expansion. `Ctrl+v` quotes the next character to suppress expansion. Word
boundaries follow the default letters/digits/underscore keyword definition.
Full-id, end-id and non-id abbreviation forms are supported. Expansion is
nonrecursive; a mapping can affect the RHS unless a noreabbrev variant is used.

- `:ab[breviate]` defines/lists Insert, Replace and command-line abbreviations.
- `:iabbrev`, `:cabbrev` restrict the mode; `:noreabbrev`, `:inoreabbrev`,
  `:cnoreabbrev` suppress RHS mappings.
- `:unabbreviate`, `:iunabbrev`, `:cunabbrev` remove an entry.
- `:abclear`, `:iabclear`, `:cabclear` clear the selected modes.
- Omit the RHS to list definitions (an optional LHS filters the list).
- `:map`, `:nmap`, `:vmap`/`:xmap`, `:omap`, `:imap`, `:cmap`, `:map!` define
  mappings. Matching `noremap`, `unmap` and `mapclear` mode variants are supported.
- `<buffer>` makes an abbreviation or mapping local to this editor tab;
  `<silent>` is accepted. `<expr>` and `<script>` are explicitly unsupported.

RHS key notation: `<Esc>`, `<CR>`, `<Tab>`, `<BS>`, `<Del>`, arrows, Home/End,
`<C-x>`, `<F1>`…`<F35>`, `<Space>`, `<lt>`, `<Bar>`, `<Bslash>`, `<Nop>`.
`<Leader>` is a fixed backslash; setting `mapleader` is not implemented.
Incomplete mappings wait up to one second. Pending input is resolved before
saving, compilation, closing, focus changes or disabling Vim mode. Recursive mappings are limited to
20 levels and 100,000 dispatched events. `:normal!` bypasses mappings;
`:normal` applies them. Native shortcuts remain available unless a mapping
explicitly claims their key sequence.

Global definitions are shared across tabs and automatically saved to
`vim-definitions.json` beside the active TeXstudio profile's settings. This is
an additional convenience: stock Vim normally persists definitions through its
configuration scripts. Buffer-local definitions are temporary. The JSON file
contains text definitions only and never executes a configuration script on
startup. Back it up with the rest of the portable `config` directory.

`:source path/to/commands.vim` can import supported Ex commands manually.
It does not implement Vimscript expressions, variables, functions, conditionals,
autocommands, plugins or automatic `.vimrc`/`init.vim` loading.

## Limits

This remains a TeXstudio-native Vim emulation, not an embedded Vim/Neovim engine.
It does not execute Vimscript, load .vimrc/plugins, implement shell
commands/filters, Vim splits/window commands, folds, quickfix, tags, cross-file
marks, persistent undo/registers/macros, all Ex command abbreviations or every Vim option. Digraphs, Select/Virtual
Replace modes, complete Insert completion, expression registers, formatting
operators, substitution confirmation, full search highlighting and Vim regexes
remain unsupported. `:e`, `:q` and window/buffer actions retain TeXstudio behavior.
`:sort` defaults to the current line; use `:%sort` for the full document.
Change history is limited to 100 positions in this tab. Offset `go` counts
logical text characters rather than Vim file bytes for Unicode/CRLF files.
The `g` screen motions use wrapped lines; H/M/L and viewport scrolling still
use simplified logical-line calculations. Block columns are text columns rather than tab-expanded virtual columns;
block Visual paste/replace have simplified semantics. Sentence/paragraph parsing
uses punctuation and blank lines, not every Vim option. Counts on sentence and
paragraph objects and some advanced motion variants may differ in edge cases.

Application shortcuts with Ctrl/Alt/Meta remain native unless listed here;
notably Ctrl+w in Normal still closes a TeXstudio tab, rather than starting Vim
window commands. Unsupported plain commands are consumed instead of inserting text;
unsupported Ex commands/options show a message. Insert mode retains TeXstudio's
completion and snippets. Mouse clicks/loss of focus cancel pending/Visual/command
state. Read-only buffers allow navigation/copying but block editing commands.

## Verification

`src/viminputbinding.*` handles modes, key sequences and replay;
`src/vimcommands.cpp` handles objects, registers, search and Ex operations.
Application commands are queued into TeXstudio so closing a tab cannot delete
a binding mid-dispatch. The completer preserves the binding chain and Escape
closes completion before returning to Normal. The test suite sends key events
to real QEditor instances and restores the system clipboard after testing.

Build with Debug and TEXSTUDIO_ENABLE_TESTS=ON for the native test manager, or
use `scripts/vimstyle/test_windows.py` for the standalone Windows test runner.
Reference semantics: https://vimhelp.org/ (motion.txt, change.txt, pattern.txt).
