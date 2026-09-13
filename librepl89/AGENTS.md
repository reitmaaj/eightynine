# librepl89 — a strict C89 multiline terminal editor

A small interactive editor for one arbitrary multiline UTF-8 submission on a
modern POSIX terminal. Unicode semantics (UTF-8, extended grapheme clusters,
East Asian Width, emoji/control classification) come from the sibling
`libu89`; librepl89 owns the edit buffer, history, key decoding, terminal
width policy, rendering, and tty control. See `.agent/`.

## Build and test

- `just build` — build `libu89` then `build/librepl89.a`.
- `just test` — run the smoke + unit suite.
- `just check` — full green profile gate (C89 ∩ C23, GCC and Clang, green
  semantic checks, canonical Allman format) via the sibling `green` toolchain.

## Conventions

- Green source profile: ISO C89 AND C23 clean under GCC and Clang, Allman
  formatting, worker/controller structure, no function-like macros, no owned
  type names ending in `_t`.
- Public API namespaced `repl89_*`, declared in `include/repl89.h`.
- Terminal policy (cell widths, wrapping, history) lives here; Unicode facts
  live in libu89. No Unicode tables or segmentation algorithms may appear in
  this repository.
- Test-driven: scenario -> acceptance -> failing test -> minimal code -> refactor.

See the workspace root `AGENTS.md` for the shared agentic workflow.
