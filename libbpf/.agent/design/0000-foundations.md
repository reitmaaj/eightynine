# 0000 - Design: foundations

## Repository layout

```
src/            C89 library (bpf.h/bpf.c) + decoder/validate/eval + CLI (main.c)
test/           unit test binaries, shell tests
.agent/         workflow docs (ignored except *.md)
tmp/            reference material (RFC 9669)
```

## Toolchain

* Compiler: `cc` (GCC/Clang), strict flags:
  `-std=c89 -pedantic-errors -Wall -Wextra -Werror -Wconversion
  -Wsign-conversion -Wstrict-prototypes -Wmissing-prototypes
  -Wold-style-definition -Wundef -Wshadow -Wformat=2`.
* No external C dependencies. No floats, so no `-ffp-contract=off` needed.
* `bpf_u64` is `unsigned long` (LP64) guarded by a compile-time `sizeof`
  assertion. Dispatch is a switch-based loop (no computed goto in C89).

## Scalar types (C89)

* `bpf_byte` = `unsigned char`
* `bpf_u32`/`bpf_i32` = `unsigned int`/`int` (compile-time sizeof assert)
* `bpf_u64`/`bpf_i64` = `unsigned long`/`long` (LP64; compile-time sizeof
  assert)
* `bpf_off` = `signed short` (the 16-bit signed `offset` field)
* Instruction count and lengths are `bpf_u32`.

## All actions run through `just`

Single root `Justfile`; recipes `build`, `test`, `check`, `lint`, `memcheck`,
`format`, `clean`, `doctor`. No action other than file editing and revision
control is performed outside `just`.

## Test pipeline

`just test`:
1. compiles and runs C unit tests (pure library functions);
2. runs end-to-end shell tests against `build/bpf`;
3. runs a C89-rejection probe asserting that non-C89 source fails to compile
   under the strict flags.

## Milestone roadmap

0. Foundations (this file): repo, toolchain, test pipeline, smoke, C89 probe.
1. Instruction model + decoder (basic and wide encodings).
2. Validator + conformance-group tagging.
3. ALU/ALU64 arithmetic + byte swap.
4. JMP/JMP32 control flow + CALL/EXIT.
5. Memory operations (MEM, MEMSX).
6. Atomic operations (32/64).
7. CLI: run, disassemble, conformance-group reporting.

Every milestone ends with the relevant unit + e2e tests passing, then the
full suite.

## Status

All milestones (0-7) are implemented and committed. `just test`, `just check`
(c89-baseline + ob89 + bb-lifter), `just lint` (shellcheck + clang-format),
and `just memcheck` (valgrind) all pass. The `bpf` CLI decodes, validates,
disassembles, reports conformance groups, and executes bytecode. Helper
CALL (src 0/2) is reported as `HOSTCALL` for a host integration that is out
of scope; the host-specific 64-bit-immediate subtypes (1-6) and the deprecated
`packet` group are rejected.

