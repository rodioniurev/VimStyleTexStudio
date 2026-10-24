# Changelog

## 0.1.0-preview.1 — 2026-10-24

First unofficial preview, based on TeXstudio development version 4.9.10,
commit fef67984b9fd5a0849c5e6b8625fe76df2d8b3c2.

- Add optional Normal, Insert, Replace and Visual modes through QEditor.
- Add motions, counts, operators, text objects, registers, local marks, search,
  substitutions, selected Ex commands, macros and change replay.
- Preserve the LaTeX completer's binding chain and native document actions.
- Handle word objects on whitespace, inner-object counts, nested blocks,
  quoted strings, paragraph/sentence objects and HTML/XML tag objects.
- Calculate positions independently of the file's LF/CRLF format.
- Add 6,642 reference fixtures, each tested with both line-ending formats,
  plus regression tests for undo, replay, cancellation and read-only buffers.
- Provide portable Windows x64 binaries, matching sources, dependency sources,
  notices, build/test scripts and checksums.

Vim mode defaults to off in source builds and on in the portable package's
separate profile. See VIM_MODE.md for compatibility limits.

Modified and newly added Vim-mode functionality: 2026-10-24, rodioniurev.
Original TeXstudio files and third-party components retain their notices.
