# Acceptance: full memory/table/bulk/ref surface (Phase C)

*Acceptance criteria for the Phase C milestone. Reference: WebAssembly
Core Spec 3.0 chapter 4, the reference interpreter's `eval.ml` Load/
Store/Table*/Memory*/Ref*/bulk cases, and the `oob` helper.*

## MUST

* Every load form `i32.load`, `i32.load8_s/u`, `i32.load16_s/u`,
  `i64.load`, `i64.load8_s/u`, `i64.load16_s/u`, `i64.load32_s/u`,
  `f32.load`, `f64.load` and store form `i32.store/8/16`, `i64.store/8/
  16/32`, `f32.store`, `f64.store` MUST execute with little-endian
  semantics, packed sign/zero extension, and effective address
  `index + offset` computed in u64.
* Every memory/table access MUST use the overflow-aware bounds check
  `oob(i, n, j) = (i+n < i) || (i+n > j)` on the full u64 sum, MUST trap
  with "out of bounds memory access"/"out of bounds table access" when it
  fails, and MUST trap even for `n = 0` when the base address is out of
  bounds. Stores MUST NOT write partial data before trapping.
* memory64/table64 MUST use i64 index types for addressing and
  size/grow results; `memory.grow`/`table.grow` MUST return the old size
  (as i64/i32 per the index type) or -1 on overflow/max/address-space
  failure, and MUST zero-fill / ref-fill only the new portion.
* `table.set` MUST write a slot; `table.size` MUST return the size;
  `table.grow` MUST fill new slots with the given ref and return -1 on
  failure; `table.fill` and `table.copy` MUST honor the `oob` check (copy
  with memmove semantics for overlapping ranges); `table.init` MUST copy
  from the element segment with both destination and segment bounds
  checked.
* `memory.copy` MUST honor the `oob` check on both memories (memmove
  semantics, distinct memories supported); `memory.fill` MUST honor the
  `oob` check and fill bytes.
* `memory.init`/`table.init` MUST pop operands in order `n, s, d` (n on
  top) and MUST resolve the data/elem segment from the first immediate
  (`idx2`) and the memory/table from the second (`idx`); `memory-multi`
  and multi-segment modules MUST address the correct instance.
* `data.drop`/`elem.drop` MUST empty the segment so later init reads trap;
  active segments dropped at instantiation MUST behave the same.
* `ref.is_null`, `ref.eq`, `ref.as_non_null` (trap "null reference" on
  null), `br_on_null`, `br_on_non_null` MUST execute per spec.
* Import matching for ref-typed globals and tables MUST NOT crash: the
  exporting instance's type environment MUST be used, and structurally
  equal declared types MUST match while mismatches MUST fail with
  "incompatible import type ...".
* The spectest instance MUST export `table64` (table64, 10..20 funcref).
* The REPL MUST accept `module <path> [<name>]` and register the loaded
  instance under the optional name so `invoke`/`get`/`assert_*` resolve it
  by name.
* The driver MUST resolve action `module` references by name (falling back
  to `last`), MUST skip `module_definition`/`module_instance`/
  `assert_uninstantiable` commands, and MUST skip (not fail) modules and
  registers that depend on those skipped instances.
* The Phase C conformance set (memory*, table*, bulk*, elem, load*,
  store*, data*, float_memory*, memory-multi, and the memory64/table64/
  bulk64 64-bit variants) MUST pass with zero failures; the full sweep
  MUST show no regression in Phase A/B files.

## MUST NOT

* The evaluator MUST NOT trap a zero-length bulk operation whose base
  address is in bounds, and MUST NOT proceed when the base is out of
  bounds (the `oob` check MUST precede the zero-length short-circuit).
* The evaluator MUST NOT alias the data/elem index with the memory/table
  index for `memory.init`/`table.init`/`memory.copy`/`table.copy`.
* The evaluator MUST NOT pop bulk operands in the wrong order (d before n).
* The interpreter MUST NOT crash (SIGSEGV) on ref-typed global/table
  imports; it MUST either match or report "incompatible import type".
* The driver MUST NOT attribute failures to commands that merely reference
  an out-of-scope proposal instance; those MUST be skipped.
* The REPL MUST NOT confuse module names with the `last` module: a named
  invocation MUST target the named instance.

## Full-suite sweep status (Phase C)

The full spec sweep is green: 257 files, 36903 passed, 0 failed, 28285
skipped. The Phase C conformance set (83 files covering address*,
align*, br_on_null/non_null, bulk*, call_ref, data*, data_drop0, elem,
endianness*, exports*, float_memory*, linking*, load*, memory*,
memory-multi, memory64*, ref*, return_call_ref, store*, table*, table64)
passes 17930 with 0 failures. Remaining skips are exclusively out-of-scope
features: SIMD (0xFD), GC (struct/array/i31/ref_eq/ref_cast/ref_test,
extern.convert), exceptions (throw/try_table/tag/throw_ref, Phase D),
threads, and the module-linking proposal (`module_definition`/
`module_instance`, instance.wast). Phase D reconciles exception message
matching and `assert_exception`.

Defects found and fixed during Phase C conformance work: bulk-op operand
pop order (n,s,d), bulk-op data/elem vs mem/table index swap, NULL type
environment for ref-typed global/table imports (SIGSEGV), declarative
element segments not dropped at instantiation, table.grow not updating
limits.min, and a decoder leak on malformed rec-type sections.

