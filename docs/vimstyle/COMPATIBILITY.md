# Compatibility scope — 0.2 preview

This project implements commands inside TeXstudio's native editor. It does not
embed Vim or Neovim. The reference is their documented shared editing behavior;
Vim-specific and Neovim-specific defaults can differ (for example Y).

| Area | Status |
| --- | --- |
| Normal/Insert/Replace/Visual editing, core operators and text objects | Implemented subset; see VIM_MODE.md |
| Insert/Replace and command-line abbreviations | Implemented, including mode variants and buffer-local definitions |
| Global abbreviation and mapping persistence | Automatic profile JSON storage |
| Mode-specific recursive/nonrecursive mappings | Implemented subset with fixed one-second timeout and backslash Leader |
| Manual source imports | Supported Ex commands only; no automatic rc loading |
| Wrapped line movement | gj/gk/g0/g^/g$/gm; other viewport commands remain simplified |
| Extended Insert commands | Ctrl+o/t/d/e/y/a, register insertion and simple literal input |
| Ex line manipulation and addressing | copy/move/put; relative, mark and search ranges; limited destination syntax |
| Search/substitute | Qt/PCRE patterns; no complete Vim regex, incremental search, all-match highlighting or confirmation |
| Registers/macros/repeat | Shared in memory; special register subset; bounded key replay |
| Visual block operations | Text columns; tab-expanded virtual columns and block shifts remain unsupported |
| Undo | Linear native history; save boundaries; no Vim branches or persistent undo |
| Splits/folds/buffer management | Native TeXstudio behavior; full Vim command families absent |
| Formatting/filters/quickfix/tags/diff/terminal | No Vim command implementation |
| Options | Small documented subset; no complete local/global option model |
| Vimscript/Vim9script/Lua/plugins/autocmd/expressions | Unsupported |
| Neovim API/RPC/LSP/Tree-sitter | Unsupported; TeXstudio supplies its own language tools |

Default differences retained: Y yanks a line, Ctrl+w closes a tab, :e without a
path opens a dialog, and :q uses the application's unsaved-changes dialog. The
unnamed register uses the system clipboard. Logical go offsets count text
characters, not file bytes. Change positions are local and limited to 100.

The full public command reference and examples are in ../../VIM_MODE.md.
