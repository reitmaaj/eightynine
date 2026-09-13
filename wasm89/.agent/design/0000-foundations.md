# Design: foundations

## Repository layout

```
src/            C89 runtime library (wasm89.h / wasm89.c) + CLI (main.c)
test/           unit test binaries, shell tests, fixtures
vendor/         reference implementations + testsuite-main (ignored by git)
.agent/         workflow docs (ignored except *.md/*.sql)
```

## Toolchain

* Compiler: `cc` (GCC/Clang), flags
  `-std=c89 -pedantic-errors -Wall -Wextra -Werror -ffp-contract=off`.
* `-ffp-contract=off` is mandatory: it disables FMA contraction so that
  each `f32`/`f64` operation rounds independently, which is required for
  bit-exact IEEE-754 semantics and the deterministic NaN profile.
* `u64` is `unsigned long` (LP64) guarded by a compile-time
  `sizeof` assertion; no computed goto (not C89), so dispatch is a
  switch-based loop.

## All actions run through `just`

Single root `Justfile`; recipes `lint`, `build`, `test`. No action other
than file editing and revision control is performed outside `just`.

## Test pipeline

`just test`:
1. compiles and runs C unit tests (pure library functions);
2. runs the CLI smoke test (exit code + output check);
3. runs a C89-rejection probe asserting that non-C89 source fails to
   compile under the strict flags.

## Milestone roadmap

1. Foundations (this file): repo, toolchain, test pipeline, smoke.
2. Values + numerics (spec 2.2, 4.3, 5.2): LEB128, bit patterns, all
   integer/float ops, conversions, DET NaN.
3. Binary decoder (spec ch5) with exhaustive malformed-case handling.
4. Validator (spec ch3 + A.4).
5. Interpreter core: control flow, calls, locals/globals, memory
   (multi-memory, memory64), tables, references, bulk memory, multi-value,
   extended-const, tail calls.
6. Exception handling.
7. SIMD + relaxed SIMD (deterministic profile).
8. Garbage collection.

Every milestone ends with the corresponding subset of the official
testsuite passing, then the full suite.

## Conformance pipeline (mechanics, verified)

The official testsuite lives at `vendor/testsuite-main`. Conversion:

```
cd <workdir> && wasm-tools json-from-wast <abs/path/file.wast> --output <workdir>/file.json
```

Important: `wasm-tools json-from-wast` writes the `.wasm` files to the
**current working directory**, not the `--output` location. The harness
recipe MUST therefore `cd` into a scratch work dir (e.g. `.agent/tmp/`)
and reference the `.wast` by absolute path. The JSON contains
`commands`: `module`, `assert_return`, `assert_trap`,
`assert_invalid`, `assert_malformed`, `assert_exhaustion`, `register`,
`invoke`, `get`.
