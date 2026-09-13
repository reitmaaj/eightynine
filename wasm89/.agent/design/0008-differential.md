# Design: cross-engine differential harness

*Drives `.agent/testing+acceptance/0015-differential.md` and stories
`0007`-`0010`. A seeded, bounded, strict differential tester that runs the
same scalar-numeric programs under wasm89 (subject) and reference engines
(wasmtime, wasm-interp) and flags any observable divergence. Test tooling
only; no change to the interpreter's semantics or its C89 build.*

## 1. Objective

wasm89's conformance suite checks behaviour against the expected values
embedded in the official `.wast` files. A differential harness adds an
independent cross-check: execute the *same* generated programs under
wasm89 and under engines written independently, and require agreement.
This catches bugs where wasm89 and the testsuite share a wrong reading of
the spec.

## 2. Value/verdict model

Each run is one `(module, entrypoint, typed-args)` case. Every oracle
normalizes to a verdict payload:

```
status ::= ok | trap | exhaustion | instantiate-fail | parse-error
results ::= [ value ]            # only meaningful when status == ok
value ::= int(bits) | float(bits)   # widths i32/i64/f32/f64
```

Cross-engine verdict:

| wasm89 | oracle | verdict |
|---|---|---|
| ok, results R89 | ok, results Ror | PASS iff `R89 ≈ Ror` |
| trap/exhaustion | same status | PASS (messages not compared) |
| ok | trap/exhaustion | FAIL |
| trap/exhaustion | ok | FAIL |
| ok, R89 | ok, Ror differ | FAIL |
| engine missing | — | ERROR |
| unparseable engine output | — | ERROR |

### Float equivalence `≈` (exact bits + NaN rule)

- `i32`/`i64`: raw 32/64-bit equality.
- `f32`/`f64`: if either side is **NaN**, results are equal iff the other
  is also NaN (payload and sign ignored). Otherwise raw bit equality is
  required. Rationale: engines legitimately differ in NaN payload
  policy; wasm89's own canonical-payload correctness stays the job of the
  conformance suite (see `.agent/testing/0003-float-ops.md`).
- Integer and float results are compared width-aware; a width or count
  mismatch is a FAIL.

### Why stock CLIs suffice (no engine shim)

- `wasm89 repl` `invoke` answers `@return i32:0x… / f64:0x…` in exact hex
  bits, and `@trap`/`@exhaustion`.
- `wasm-interp <f> -r <fn> -a <type>:<arg> …` prints `fn(...) => i32:42`
  (wabt reference interpreter). Integer results are exact; float results
  are printed only to a fixed ~6-decimal precision and NaN as `nan`, so
  wasm-interp is an **integer-exact and runtime-failure oracle only**.
- `wasmtime run --invoke <fn> <f> <args> …` prints decimal results; Rust's
  shortest-round-trip formatting is an **injective** encoding of finite
  floats, so each finite value is recoverable to its exact bits. The
  harness decodes wasmtime output back to bits and verifies the round
  trip; any ambiguity is a parse-error (never a silent pass). NaN prints
  as `NaN`/`-NaN` and matches the NaN rule.

## 3. Program generation (milestone 1)

The generator must produce only programs that are **valid in every engine
by construction**, so a mismatch is a real finding, never a generator bug.
Two complementary generators share one seed:

### 3a. Random numeric-expression generator
Type-correct random **expression modules**: each exported function is a
typed expression over scalar numerics, built bottom-up so validation
always succeeds. The opcode set is the conformance-green numeric core, so
mismatches are genuine, not known feature gaps.

- **In scope:** i32/i64/f32/f64 `const`, `local.get`, typed `select`, all
  integer arithmetic/bitwise/shift/rotate/count/`eqz`, all signed/unsigned
  comparisons, float arithmetic/`neg`/`abs`/`ceil`/`floor`/`trunc`/
  `nearest`/`sqrt`/`min`/`max`/compare, and conversions
  (int<->int, non-saturating int<->float truncation, `reinterpret`),
  wrapped occasionally in typed `block`/`if` bodies.
- **Excluded from the random generator:** `copysign`. When its sign
  operand is a NaN, wasm89's deterministic profile (positive-canonical
  NaN) and engines that preserve the NaN sign bit legitimately produce
  finite results of *opposite sign*, which the NaN-result rule cannot
  classify. `copysign` with non-NaN sign operands is still deterministic
  and is exercised via fixed templates where the sign source is never a
  generated NaN.
- **No `local.set/tee`, no mutable or imported globals, no memory, no
  tables, no calls, no `start` in this generator.** Expressions are
  recomputed pure functions of their parameters, so every run is
  deterministic and stateless.

### 3b. Fixed template modules (memory/control/calls/traps)
A small, hand-vetted library of structurally-valid template modules
covers the features the random generator must not risk: linear-memory
`load`/`store`/`size`/`grow`, `memory.copy`/`fill`/`init`, tables and
`call_indirect`, loops/`br_table`, bounded recursion and `return_call`,
and deliberate traps (divide-by-zero, out-of-bounds, `unreachable`,
`i32.trunc_*` of NaN). Operands and arguments are randomized; structure is
fixed so validity is guaranteed. The committed `fixtures/f.wasm` is one
such template.

### Feature flags and excluded set (milestone 1)
Every module is compiled with the same explicit feature-flag set across
engines (base core for M1). **Excluded (deferred, explicit):** SIMD/v128,
GC and i31, exceptions (`throw`/`try_table`/`throw_ref`), memory64,
multi-memory, mutable or imported globals, all host imports (incl.
spectest), WASI. A generated module never imports or starts code.

### Module shape constraints
Every generated module decodes, validates, and instantiates in all three
runtimes and exposes exported functions taking and returning only scalar
numerics with no imports. A case that fails to validate in any engine is a
harness/config bug (reported as ERROR), never a skip or pass.

### Input-argument strategy
For each entrypoint, enumerate a per-type **edge-case set** (`0`, `±1`,
`min`, `max`, `0x00000000`/`0x7fffffff`/`0x80000000`/`0xffffffff`,
canonical NaN, `±0.0`, subnormals, `±inf`) **plus** a large seeded
pseudo-random sample. Exotic NaN *payloads* are not fed as cross-engine
float *inputs* (wasmtime cannot receive a payload through its decimal arg
parser); only canonical NaN is fed, and NaN *results* are still compared
under the NaN rule. A single integer seed fixes the module graph, the
argument vectors, and therefore every verdict, so a reported mismatch is
reproducible verbatim.

## 4. Adapter contract

Each engine is a thin adapter exposing:

```
discover() -> path or raise EngineMissing
invoke(module, func, argtypes, argbits, feature_flags)
    -> (status, [bit tokens])   # normalized
```

- `oracle_wasm89`: one persistent `wasm89 repl`; issue `module <file>`
  then `invoke last <func> <nargs> <tok>…`; parse the `@`-prefixed reply.
- `oracle_wasmtime`: `wasmtime run --invoke <func> <file> <arg>…`; parse
  decimal results and decode to bits; `-W <flags>` for feature parity.
- `oracle_wasm_interp`: `wasm-interp <file> -r <func> -a <type>:<arg>…`
  with `--enable-*` flags; parse `=> ...` bits.
- Engines are **PATH-discovered**; a missing engine raises EngineMissing,
  which the sweep reports as ERROR and, when it is a required oracle,
  aborts nonzero.

## 5. Boundedness and determinism (mirrors A2)

Same discipline as `.agent/testing/0012-sweep-boundedness.md`: per-command
and per-case deadlines; per-module progress lines; aggregate accounting;
and a tear-down/restart of a wedged runtime process. The generator and
argument sampler are pure functions of the seed.

## 6. Repository layout and gating

All under the `wasm89` repo:

```
test/diff/compare.py        normalization + verdict (exact bits + NaN rule)
test/diff/oracle_wasm89.py  wasm89 repl adapter
test/diff/oracle_engines.py wasmtime + wasm-interp adapters + discovery
test/diff/gen_modules.py    seeded generator (whitelist subset)
test/diff/sweep.py          bounded sweep driver
test/diff/self_test.py      harness self-tests (agree + forced-mismatch FAIL)
test/diff/fixtures/         tiny hand modules
```

`just diff` runs a bounded smoke (few modules, required oracles only if
installed); `just diff-sweep` runs the full seeded sweep. The full sweep
is intentionally **not** added to `run_tests.sh`, because engines are
environmental and `main` must stay green without them. `lint` covers the
new Python via `py_compile`.

## 7. Out of scope / deferred
SIMD/GC/exceptions/memory64/multi-memory programs, host-import and
spectest differential, `wasmer` (not installed), engine-specific
non-determinism beyond the NaN rule, and auto-running the full sweep in
the default `just test`.
