# VimStyleTexStudio

Vim-style editing inside [TeXstudio](https://github.com/texstudio-org/texstudio),
with a portable Windows x64 build. Keep TeXstudio's LaTeX completion, PDF preview
and document tools while using modal navigation and editing.

This is an unofficial fork. The first release is a preview.

**[Download for Windows](https://github.com/rodioniurev/VimStyleTexStudio/releases)**
· [Command reference](VIM_MODE.md) · [Build instructions](docs/vimstyle/BUILDING.md)
· [Changelog](CHANGELOG.md)

The 0.2 preview adds persistent abbreviations (`:ab`, `:iabbrev`, `:cabbrev`),
mode-specific mappings, manual `:source`, additional Insert commands and wrapped
line motions. See [VIM_MODE.md](VIM_MODE.md) for examples and explicit limits.

## Start on Windows

1. Download the `windows-x64.zip` asset from Releases and extract the entire ZIP
   into a writable folder. Start `texstudio.exe` from that folder.
2. The portable package enables Vim editing in its separate `config` directory.
   Source builds start with Vim editing disabled.
3. Click inside the document and press **Esc** for Normal mode. Use the English
   keyboard layout for commands. Press **i** to enter text in any layout.
4. Enable or disable the feature at **Options → Configure TeXstudio → Editor →
   Enable Vim editing**. The editor status panel shows the current mode.

A TeX distribution such as MiKTeX or TeX Live is still needed to compile LaTeX.
Save documents and close an older instance before starting the new build.

## Editing examples

| Keys in Normal mode | Result |
| --- | --- |
| `h j k l`, `w b e`, `gg G` | Move by characters, words and document boundaries |
| `ciw`, replacement, Esc | Change the word under the cursor |
| `caw`, replacement, Esc | Change a word including adjacent whitespace |
| `diw`, `daw`, `d2aw` | Delete an inner word, a word with whitespace, or two words |
| `ci{`, `di(`, `vi"` | Change a LaTeX argument, delete inside parentheses, select quoted text |
| `dd`, `yy`, `p`, `u`, `Ctrl+r` | Delete/copy a line, paste, undo, redo |
| `v`, `V`, `Ctrl+v` | Character, line and rectangular selection |
| `.`, `qa` … `q`, `@a` | Repeat a change; record and play a macro |
| `/pattern`, Enter; `n`, `N` | Search and repeat search |
| `:w`, `:q`, `:wq` | Native TeXstudio save and close actions |

Also available: counts, named registers, local marks, character searches, case
operators, indentation, text objects, substitutions, selected Ex commands and
tab navigation. [VIM_MODE.md](VIM_MODE.md) documents the exact scope.

## Compatibility

This uses QEditor input bindings. It does not embed Vim/Neovim, execute Vimscript,
load `.vimrc`, plugins or mappings, or implement every Vim command. Regular
expressions use Qt/PCRE semantics. Window splits, shell filters, quickfix,
persistent registers and some advanced Visual/motion semantics are unavailable.
In Normal mode **Ctrl+w still closes a TeXstudio tab**.

The Windows package was tested on Windows 11 x64. Windows 10 is not a verified
target for this preview. The package is not code-signed.

## Validation

The release baseline passed **13,369 tests on Windows with Qt 6.12** and
**13,369 tests on Linux with Qt 5.15**, without failures or skipped tests.
This includes 6,642 reference cases from an unconfigured Vim 8.2, each tested
with LF and CRLF. Coverage includes text results, mode transitions, registers,
counts, Visual selections, undo, replay, read-only buffers and errors.
This validates that matrix, not complete Vim compatibility.

Preview 2 fixes saving before Build & View during Insert/Replace. It also adds
regressions for dirty-state tracking and undo boundaries when saving mid-input;
a real pdflatex/PDF-text check confirmed that the updated text reaches the PDF
without pressing Escape first. See the changelog and release validation report.

See [verification details](docs/vimstyle/VERIFICATION.md) and
[`scripts/vimstyle`](scripts/vimstyle).

## Upstream and contributions

Based on TeXstudio commit `fef67984b9fd5a0849c5e6b8625fe76df2d8b3c2`
(development version 4.9.10). Upstream history and notices are preserved.
[The original README](UPSTREAM_README.md) describes TeXstudio itself.

Earlier feature requests: [#53](https://github.com/texstudio-org/texstudio/issues/53)
and [#155](https://github.com/texstudio-org/texstudio/issues/155).
[Background and related work](docs/vimstyle/UPSTREAM_DISCUSSIONS.md) records
these discussions and another existing implementation. This fork does not
imply upstream acceptance. See [CONTRIBUTING.md](CONTRIBUTING.md).

## License and source

GNU GPLv3; see [COPYING](COPYING) and original per-file notices. TeXstudio and
dependencies retain their authors and licenses. Releases provide the matching
application source, build scripts, dependency source packages, license notices
and SHA256 checksums. New Vim-mode changes are dated in [CHANGELOG.md](CHANGELOG.md).
