# libu89 — validating Unicode for strict C89

A self-contained, dependency-free, strict-C89 Unicode library providing what
ICU cannot (strict validation) plus an orthogonal code/terminal feature set
(XID, display width, normalization). See `.agent/`.

## Build and test

- `just build` — compile `libu89.a`.
- `just test` — run the unit + acceptance test suite.
- `just build32` — best-effort ILP32 build of `build/32/libu89.a`; skips
  cleanly when 32-bit multilib is absent.
- `just test32` — run the unit + acceptance test suite under `-m32`.
- `just check` — full green profile gate (C89 ∩ C23, GCC and Clang, green
  semantic checks, canonical Allman format) via the sibling `green` toolchain.
- `just conform-norm` — run the complete Unicode `NormalizationTest.txt` suite.
- `just tables` — regenerate Unicode tables from the vendored UCD data.
- `just gendiff` — cross-check committed tables against a fresh parse of the
  vendored data.
- `just manifest` — regenerate `unicode-testdata/17.0.0/SOURCES.txt`.

## Conventions

- Green source profile: ISO C89 AND C23 clean under GCC and Clang, Allman
  formatting, worker/controller structure, no function-like macros, no owned
  type names ending in `_t`.
- Public API namespaced `u89_*`, alloc-free, length-based (explicit lengths).
- Test-driven: scenario -> acceptance -> failing test -> minimal code -> refactor.

See the workspace root `AGENTS.md` for the shared agentic workflow.
