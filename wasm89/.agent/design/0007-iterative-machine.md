# Design: iterative control machine (Phase E perf)

*Drives `.agent/testing+acceptance/0014-on2-recursion.md`. Replaces the
O(N^2) C-recursive stepping model with an explicit heap control stack and
a unified value stack, making non-tail recursion O(N) time with bounded
C stack. No decoder/validator/type-system changes.*

## 1. Current machine model (and why it is O(N^2))

The evaluator (`src/eval.c`) steps a module-level config `w89_config`
that owns one `w89_code` deque (`front` LIFO + `back` instruction range)
and one value stack `vs`. Nesting is represented by embedding:

```
struct w89_code { value[] vs; ainstr[] front; ainstr[] back; src; nsrc; }
struct w89_ainstr { kind; in; ipos; ...; w89_code code; }  // A_FRAME/A_LABEL
                                                          // embed their inner code
struct w89_config { frame*; w89_code code; budget; crash; exhausted; }
```

Entering a function body or a block pushes an **admin instruction**
(`A_FRAME` for a call frame, `A_LABEL` for a block/if) that *embeds the
inner code as its own `w89_code` with its own value stack*. The steppers
then **C-recurse**: `step_frame`/`step_label` build a fresh local
`w89_config sub`, copy the embedded inner code into `sub.code`, call
`w89_step(&sub)` for one step, and copy it back.

### O(N^2) mechanism
After a non-tail `call` executes, `step_call` pushes an `A_INVOKE` and
returns `OK` all the way up to `w89_eval`'s surface loop; the next
surface iteration must **re-descend from the top through every currently
open frame/label** to reach the callee and step it. A depth-N non-tail
recursion therefore does O(depth) surface↔depth travel per instruction.

Measured: `down(N)` ≈ `17.6·N² + 46.7·N` `w89_step` calls; C-stack depth
reaches ≈ `1.7·N` frames (gdb: 1686 at N=1000). `down(5000)` > 60 s
(unoptimized). `down(2000)` ≈ 5.4 s (-O2). Profiling `down(3000)`:
108M `w89_step`, 54M `step_label` (≈63% self-time); code-deque ops are
all O(1), so the cost is the repeated descent, not the deque.

Tail `return_call` is already O(1) per iteration (RETINV replaces one
frame in place; no re-descent), which is why 1M tail loops are only ~5 s.

## 2. Target: explicit heap control stack + unified value stack

Make `w89_config` hold the full execution context on an explicit,
heap-backed machine stack instead of the C stack, and step the
**innermost** entry in place (O(1) amortized per instruction).

### 2.1 Machine stack entries
One entry type distinguishes a function frame from a structured block:

```
mach_entry {
  kind : FUNC | BLOCK
  // function frame:
  w89_funcinst *finst;
  w89_frame *frame;         // locals
  // block/label:
  const w89_instr *src; u32 nsrc;   // enclosing instr range
  u32 pc;                           // current position within body
  u32 end;                          // body end (arm end for if/else)
  u32 contpos, contn;               // break continuation
  u32 exit_arity;                   // results this entry must yield
  u32 base;                         // value-stack base index for this entry
  u32 catches; w89_catch *catches;  // try_table catch arms (nullable)
}
```

A single config-level **value stack** (`w89_value vs[]`) is used by all
entries; each entry records the `base` where its operand/result frame
begins (the standard "caller/block base" scheme). Function locals live in
the `frame` as today; only the *value* stack is shared.

### 2.2 Driving loop
`w89_eval` steps once per iteration against the **top machine entry**:

1. If the top entry is a `FUNC` whose pc has reached a `return`, pop it
   and splice its results onto the caller's value frame (O(1)); repeat
   while the new top is also at a return.
2. Otherwise read the current `src[pc]`:
   - plain instruction → dispatch and execute, advance `pc`;
   - `block`/`loop`/`if` → push a `BLOCK` entry (`if` first evaluates the
     condition; `else` picks the arm), record its `base` and `end`;
   - `call`/`call_indirect` → push a `FUNC` entry (host call runs inline);
   - `return_call`/`return_call_indirect` (tail) → pop current `FUNC`,
     push the callee `FUNC` with the same value frame (no growth);
   - `br`/`br_if`/`br_table` → pop entries down to the target label and
     resume at its continuation; `br` on a `FUNC` = function return;
   - `try_table` → push a `BLOCK`-like entry carrying catch arms;
   - `throw`/`throw_ref` → unwind entries to the nearest matching catch,
     or to the surface as an exception result.

Because entries are popped/advanced in place on a heap stack, no step
ever re-traverses the ancestors: total work is linear in the number of
admin transitions executed.

### 2.3 Mapping of current admin kinds to stack ops
| current (`A_*`) | new representation |
|---|---|
| `A_PLAIN`/`A_REFER` | dispatch on `src[pc]`, advance `pc` |
| `A_INVOKE` | push `FUNC` (wasm) or inline host call |
| `A_FRAME` | `FUNC` entry |
| `A_LABEL` | `BLOCK` entry |
| `A_BREAKING` | encoded as `br` target/continuation on `BLOCK` |
| `A_RETURNING`/`A_RETINV` | function return / tail-call pop |
| `A_TRAP`/`A_THROWING` | surface trap / exception result |

The old `step_frame`/`step_label` C-recursion, the embedded `w89_code` +
per-config value stacks, and the `front`/`back` deque threading between
levels are retired.

## 3. Value-stack unification
Currently each `w89_code.vs` is independent and results are spliced
(`vs_append`) at frame/label exit. Under the unified model:
- One config-level `w89_value vs[]` with a `vsn`.
- Each `BLOCK`/`FUNC` records `base = vsn` at entry; its body operates
  above that base; on exit the entry yields `exit_arity` results that are
  truncated/kept relative to `base` and the entry is popped.
- Locals/params are copied into the `w89_frame` at `FUNC` push as today,
  but the caller's live values above the call base are the callee's
  args (no copy in/out except across the host boundary and param/local
  setup).
- Multi-value results: on pop, copy the top `exit_arity` values just
  above `base`, adjust `vsn = base + exit_arity` in the caller.

## 4. Tail calls and exhaustion
- `return_call*`: pop the current `FUNC`, push the target `FUNC` reusing
  the same value frame/base → depth does not grow (matches today's
  RETINV behavior and keeps 1M tail loops working).
- **Exhaustion budget (5000):** now enforces a cap on the number of
  *simultaneously open non-tail `FUNC` entries* on the machine stack.
  When a non-tail `call` would exceed the budget, return `call stack
  exhausted`. Tail calls and blocks do not count. Terminating recursion
  below 5000 still succeeds; unbounded recursion traps at 5000 fast
  (depth-5000 descent is now ~O(5000), i.e. ~ms, not O(25M) steps).

## 5. C-stack safety
With no C-recursion per nesting level, the native stack stays flat even
at depth 5000, removing the native-stack-overflow risk (PERF-002). Deep
recursion uses only the heap machine stack (bounded by the budget).

## 6. Staged migration & test gates
One-shot replacement is too risky, so migrate in stages, keeping the
suite green after each:

0. Perf gate (done): `test/perf_on2.sh` fails on O(N^2) build.
1. Introduce the machine stack + a parallel "top-of-stack" stepper that
   handles the migrated kinds, falling back to the legacy C-recursive
   path for anything not yet migrated (per kind/opcode).
2. Migrate control flow: blocks/`br`/`if`/`loop` → `BLOCK` entries.
3. Migrate calls/returns/frames → `FUNC` entries (non-tail).
4. Migrate tail calls (`return_call*`).
5. Migrate `try_table`/`throw`/handlers.
 6. Delete the legacy path from the invocation/evaluation entry points
    (S2.6): the iterative driver is the sole path for wasm function evaluation
    (`w89_invoke`, start). The legacy C-recursive evaluator is removed
    entirely (S2.7): instantiation const/element expressions run on the driver
    as bounded flat single-value/ref expressions, host functions are invoked
    as leaves, and unit tests assert concrete expected results (no
    legacy-vs-driver reference oracle).

Gate after each stage: `just lint ob89 test` green + the affected
conformance files (`call`, `call_indirect`, `return_call*`, `fac`,
`skip-stack-guard-page`, exceptions). Final: `down(5000)` fast and the
full 257-file sweep unchanged.

## 7. Out of scope
- The `return_call_indirect.wast:303` stall (separate defect; tracked
  separately).
- The residual ~5 s for 1M tail loops (a smaller constant; revisit after
  the machine is iterative if desired).

---

## 8. Phase 2 execution plan (confirmed; handoff for a fresh session)

Confirmed decisions (do not re-litigate): **unify into a single value
stack** (this design), and **migrate side-by-side** with the legacy
recursive path kept as the default until a feature migrates, then
deleted.

### Root-cause isolation (verified)
The O(N^2) re-descent originates in only three orchestration functions,
each of which builds a stack-local `w89_config sub` and calls
`w89_step(&sub)` exactly once, then returns:
- `step_frame`: `src/eval.c:1728`
- `step_label`: `src/eval.c:2964`
- `step_handler`: `src/eval.c:1275`

`w89_eval` (`eval.c:4544`) calls `w89_step` once per top-level iteration,
so a depth-N nest is stepped by O(N) C-recursive calls per iteration →
O(N^2) overall. Everything else (`step_plain`, `step_numeric`,
loads/stores, `step_mem`, `step_bulk`, locals/globals, `step_select`)
operates on a single level's code+vs and needs no change; only
control-flow *orchestration* is rewritten.

### Staged work (each stage: `just lint ob89 test` green; delete legacy only at S2.6)
- **S2.1 Scaffold.** Add the level stack (`w89_lvl`) + single unified
  value stack + a new driver, built under a compile flag/env; legacy
  recursive path remains the untouched default. `just test` green.
- **S2.2 Blocks.** Migrate `block`/`loop`/`if`/`br_table`/`br` onto the
  level stack. Verify `br/block/if/loop/br_table` conformance.
- **S2.3 Calls/frames (non-tail).** Migrate `step_invoke`/`step_frame`.
  **Perf gate `test/perf_on2.sh` flips green here** (`down(5000)` fast;
  C-stack flat). Verify `call`, `call_indirect`, `fac`,
  `skip-stack-guard-page`.
- **S2.4 Tail calls.** Migrate `RETINV` frame-reuse on the level stack.
  Verify `return_call*` (1M loops must still succeed, uncharged).
- **S2.5 Exceptions.** Migrate `try_table`/`step_handler`/`A_THROWING`
  unwinding. Verify `throw`, `throw_ref`, `try_table`. **DONE**: `try_table`
  is a `BLOCK` level carrying its catch arms; a driver `throw`/`throw_ref`
  unwinds open levels innermost-out to the nearest matching catch (freeing
  driver-owned frames above the target and restoring the budget) or surfaces
  as `W89_EVAL_EXCEPTION`.
- **S2.6 Decommission.** Remove the legacy `w89_step(&sub)` recursion +
  old driver. Final full gate + 257-file conformance sweep unchanged.
  **DONE**: driver is the sole wasm-function path; `W89_ITER`, the eligibility
  scan and the `w89_eval_iter` fallback are removed. The driver additionally
  gained param-typed block/loop/if/try_table, table.get/set, the `0xFC`
  bulk/table/saturating family, `0xD0..0xD4` ref leaves,
  `br_on_null`/`br_on_non_null`, and `call_ref`/`return_call_ref`. Full
  257-file conformance 37171/0/0 STALL; `just ci` green.

### Semantics to preserve (correctness hotspots)
1. Budget/exhaustion: charge only non-tail `FUNC` pushes; exhaust at 0
   (depth 5000). Tail reuses the entry (uncharged). Terminating recursion
   <5000 succeeds; runaway traps at 5000 fast.
2. Branch markers: `A_BREAKING` depth-k → pop k+1 entries to the target
   label, splice its results, honor loop `contpos/contn`.
3. Returns pop the `FUNC` level and keep `exit_arity` above `base`.
4. Exceptions: `A_THROWING` unwinds to the nearest matching handler;
   unmatched reaches the surface as `W89_EVAL_EXCEPTION`.
5. Value/ref/exn payloads in captured `vs0`; multi-value; trap messages;
   effect order.

### Branch/state notes for the next session
- Work branch: `feat/a3-on2-iterative` (tag `on2-foundation`). `main`
  is untouched and green. `src/*.c` are pristine.
- The perf gate (`test/perf_on2.sh`, in `run_tests.sh`) is intentionally
  RED on this branch until S2.3; it is the regression gate. Do not merge
  to `main` while it is red.
- Start S2.1 by adding the `w89_lvl` level stack + unified value stack
  types in `src/eval.h` and a flagged driver in `src/eval.c`, keeping the
  legacy path default. Verify `just build` under
  `-std=c89 -pedantic-errors -Wall -Wextra -Werror` and `ob89` clean at
  every stage.
