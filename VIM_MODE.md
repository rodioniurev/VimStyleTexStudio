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
`3iabc<Esc>`. Opening a line with `o/O` currently opens one line regardless of count.
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
Insert-mode `Ctrl+r{register}` inserts a register; `Ctrl+w` erases the preceding
word, `Ctrl+u` erases to the line start. These are also recorded in macros.

## Search

`/pattern<Enter>` searches forward; `?pattern<Enter>` searches backward.
`n/N` repeat in the same/opposite direction; `*/#` search the word under the
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
| `:r filename`, `:read filename` | Read a UTF-8 file after the addressed line |
| `:u`, `:undo`, `:red`, `:redo` | Undo/redo |
| `:reg`, `:registers`, `:marks`, `:help` | Display registers, local marks, help location |

Addresses: numbers (1-based), `.`, `$`, `%`, `first,last`; Visual selections
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

## Limits

This remains a TeXstudio-native Vim emulation, not an embedded Vim/Neovim engine.
It does not execute Vimscript, load .vimrc/plugins, implement mappings, shell
commands/filters, Vim splits/window commands, folds, quickfix, tags, cross-file
marks, persistent undo/registers/macros, all Ex abbreviations or every Vim option.
Soft-wrapped screen-line movement differs from Vim: viewport commands use logical
lines. Block columns are text columns rather than tab-expanded virtual columns;
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
