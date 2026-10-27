# Release verification

## Preview 2 — 2026-10-27

Windows 11 x64 / Qt 6.12.0: **13,374 passed, 0 failed, 0 skipped** locally.
New cases cover saving inside Insert/Replace, continued input, nested change
blocks, empty groups, clean-state notifications and undo/redo across a save.
A separate integration check compiled a real LaTeX file with pdflatex before
and after entering text without Escape, and extracted the resulting PDF text
with pdftotext to confirm the change reached the PDF.

GitHub Actions runs the updated Windows/Linux suite on the release commit;
see the repository's VimStyle checks workflow for its current status.
The dependency versions are unchanged from Preview 1.

## Preview 1 — baseline

Release: 0.1.0-preview.1, 2026-10-24. Based on upstream commit
`fef67984b9fd5a0849c5e6b8625fe76df2d8b3c2`.

| Environment | Result |
| --- | --- |
| Windows 11 x64, Qt 6.12.0, GCC 16.2.0 UCRT | 13,369 passed; 0 failed; 0 skipped |
| Ubuntu 22.04, Qt 5.15.3, GCC 11 | 13,369 passed; 0 failed; 0 skipped |

The suite sends key events to real QEditor instances and restores the clipboard.
6,642 reference cases were obtained using Vim 8.2 without user configuration.
Each runs once with LF and once with CRLF. Checks cover text, entering Insert,
register contents for yanks and preserving the document's line-ending format.

The matrix includes words/WORD, whitespace, punctuation, tabs, Cyrillic, empty
buffers, quotes and escapes, nested and multiline brackets, aliases b/B,
paragraphs, sentences, tag blocks, counts before/after operators and Visual
selection. Separate tests cover cancellation, undo, dot replay, named registers,
read-only buffers, macros, search and selected Ex commands.

The standalone Windows package starts with a system-only PATH. Its binary and
Qt/plugin dependency imports were checked recursively, and ZIP members passed
CRC verification. It does not depend on a local development SDK at runtime.

These results describe automated testing. A complete manual UI acceptance pass
was not completed for this preview. Windows 10 and macOS were not tested here.
This coverage does not establish equivalence with every Vim version or command.

Regenerate fixtures with `python3 src/tests/generate_vim_object_oracle.py` when
Vim is on PATH. Changing the reference Vim version can change expected semantics;
review the fixture diff rather than accepting it automatically.
