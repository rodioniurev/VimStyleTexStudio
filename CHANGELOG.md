# Changelog

## 0.3.0-preview.2 - 2026-11-03

- Fix missing or distorted PDF text when printing embedded Type 1 fonts.
- Render complete pages through Splash before sending images to the printer,
  bypassing native printer font handling and QPicture font replay.
- Keep page ranges, copies, grayscale, margins and cancellation behavior.
- Add a LaTeX Type 1 font fixture and compare visible text against a reference,
  covering Cyrillic, mathematics, missing glyphs and excessive ink.
- Document raster output quality and memory limits.

## 0.3.0-preview.1 - 2026-11-03

- Add Print PDF to the viewer context menu, toolbar and windowed File menu.
- Use Qt's native print dialog with Poppler rendering instead of external commands.
- Support page ranges/current page, page order, copies, grayscale and driver properties.
- Snapshot the actual PDF before printing so recompilation does not invalidate the job.
- Preserve page proportions and printable margins, with vector output where available.
- Add isolated virtual-printer and viewer integration checks; no physical test jobs.
- Document upstream discussions #201 and #3710 and keep source printing distinct.

## 0.2.0-preview.2 — 2026-11-03

- Add a checkable Options > Vim Editing command, available without opening settings.
- Add a dedicated icon and expose the same command in toolbar customization.
- Keep menu/button state synchronized with the Editor checkbox and loaded profiles.
- Apply changes to all open and new documents and persist the toggle immediately.
- Add main-window verification of toolbar customization, pending input and restart state.

## 0.2.0-preview.1 — 2026-10-27

- Add Insert/Replace and command-line abbreviations, mode-specific removal,
  buffer-local definitions, literal input and explicit expansion.
- Add recursive/nonrecursive mappings by mode, key notation, timeout and
  bounded replay, with automatic profile storage for global definitions.
- Add manual `:source` for supported Ex commands and distinguish `:normal!`.
- Add Insert Ctrl+o/t/d/e/y/a, wrapped-line g motions, gi and change positions.
- Correct ge/gE word classification, counted o/O and the default :sort range.
- Add :copy/:move/:put, relative/mark/search addresses and semicolon ranges,
  command-line register insertion and substitution repeat.
- Add independent Vim reference fixtures and regressions for the new commands.
- Preserve the Preview 2 save-during-input fix.

## 0.1.0-preview.2 — 2026-10-27

- Fix automatic saving before Build & View while Vim Insert or Replace is active.
  Pending edit groups previously left the document marked as unmodified, so a
  successful compilation could use the old source from disk.
- Track unsaved edits inside open document change blocks immediately, including
  the Save action and close prompts. A save commits an undo boundary while
  preserving open edit groups, so typing can continue without pressing Escape.
- Keep empty edit groups from marking a saved document dirty.
- Add regressions for i/a/R/ciw, saving during input, continued typing, nested
  groups, clean-state notifications and undo/redo across a save boundary.
- Verify a real pdflatex build and PDF text change without leaving Insert mode.

The runtime dependency versions are unchanged from Preview 1.

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
