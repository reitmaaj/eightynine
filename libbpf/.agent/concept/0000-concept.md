# 0000 - BPF ISA interpreter in pedantic C89

## Software

`libbpf` is a BPF Instruction Set Architecture (ISA) interpreter and VM
implemented in strict, portable ISO C89, from RFC 9669 (`tmp/rfc9669.txt`).

## Primary goals, in order

1. **Conformance to RFC 9669.** The decoder, validator, and evaluator follow
   the specification's instruction encoding, semantics, and conformance
   groups. Conformance is measured with unit tests for every pure function
   and branch, plus end-to-end tests that execute real bytecode.
2. **Strict C89 conformance.** `src/*` compiles cleanly under
   `-std=c89 -pedantic-errors -Wall -Wextra -Werror -Wconversion
   -Wsign-conversion -Wstrict-prototypes -Wmissing-prototypes
   -Wold-style-definition -Wundef -Wshadow -Wformat=2`.
3. **Clarity over speed.** A switch-based interpreter over a decoded
   instruction vector with a fixed register set; correctness and auditability
   outrank performance.

## Scope

* Instruction encoding: the basic (64-bit) and wide (128-bit) encodings.
* Conformance groups supported: `base32` (mandatory), plus `base64`,
  `divmul32`, `divmul64`, `atomic32`, `atomic64`.
* Registers r0-r10 (u64 each); r10 is the read-only frame pointer, r0 the
  return value. Registers r1-r5 are the standard argument positions.
* Execution: ALU/ALU64 arithmetic (incl. signed divide/modulo and byte-swap),
  JMP/JMP32 control flow with CALL/EXIT, bounds-checked memory access
  (load/store/sign-extension), and 32/64-bit atomic operations.
* A step/instruction budget guarantees termination; exceeding it is reported
  as an error.

## Excluded by design (rejected cleanly, never offered)

* The deprecated `packet` conformance group (legacy ABS/IND packet access).
* Host-specific 64-bit-immediate LD subtypes (`map_fd`, `map_val`,
  `var_addr`, `code_addr`, `map_idx`, `map_val_idx`). A host hook is reserved
  in `src/bpf.h` for a later, platform-specific milestone.
* BPF Type Format (BTF) and helper-function registration (out of RFC scope).
* The verifier: RFC 9669 delegates safety analysis to external verifiers; a
  basic structural validator (register ranges, reserved bits, deprecated
  instructions) is in scope, full type/reachability verification is not.

## Verifiability

Every milestone is gated on (a) a smoke test, (b) unit tests for pure
functions, (c) end-to-end execution tests, then (d) the full suite. The build
must stay warning-free at every commit. `just check` enforces c89-baseline,
ob89, and bb-lifter; `just lint` enforces shellcheck and clang-format.
