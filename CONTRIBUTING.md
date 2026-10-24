# Contributing

Keep Vim editing optional and preserve ordinary editing when disabled.
Keep changes local to input bindings where possible and preserve completion
and native document actions.

Bug reports should include release, OS, keyboard layout, minimal starting
text, cursor location, exact keys and expected/actual result. For compatibility
issues include a reproduction with `vim -Nu NONE -n`.

Add regressions in `src/tests/viminputbinding_t.cpp`. Generated reference cases
are in `src/tests/vim_object_oracle.json`; review changes when using another
Vim version. Build/test instructions are in `docs/vimstyle/BUILDING.md`.

Contributions must be GPL-compatible and preserve copyright and dependency
notices. Do not commit personal profiles, sessions, SDKs or credentials.

Upstream discussions already exist in texstudio-org/texstudio issues #53 and
#155. A focused upstream patch can reference those instead of duplicating the
feature request. This fork is not an official texstudio-org release channel.
