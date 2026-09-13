# Design: full memory/table/bulk/ref surface (Phase C)

Completes the memory/table/bulk/ref instruction surface that Phase B only
partially executed (i32.load, i32.load8_u, i32.store, i32.store8,
memory.size/grow, table.get, memory.init/data.drop/table.init/elem.drop,
ref.null/ref.func, call_indirect/call_ref). Phase C adds every remaining
load/store form (including memory64 index types), table.set and the
table/memory bulk instructions, and the non-GC ref instructions.

## Bounds and overflow (`oob`)

Ports the reference interpreter's overflow-aware bounds check:

    oob(i, n, j) = I64.lt_u (i + n) i || I64.gt_u (i + n) j

All addresses are widened to u64 (zero-extension for i32 index types). The
check runs on the *sum* `i + n` before any zero-length short-circuit, so a
zero-length access whose base address is out of bounds still traps. Every
load/store/bulk/table access uses this single helper.

## Loads and stores

One table-driven step handles all `0x28..0x3E` forms. Each form carries a
result/operand value type (i32/i64/f32/f64), an access width (1/2/4/8), an
optional packed sign/zero extension, and whether the effective address is
u32- or u64-based (from the addressed memory's `limits.addr64`). Stores
trap before writing any byte (no partial stores), matching the reference
and the `memory_trap1` boundary tests.

## Table instructions

`table.get`/`table.set` share the index/addrtype logic with
`table.size`/`table.grow`/`table.fill`/`table.copy`/`table.init`:
indices are u64 for table64 tables, u32 otherwise; `table.size`/`table.grow`
push i64/i32 accordingly. `table.grow` returns the old size, or -1 on size
overflow / exceeding the declared max / exceeding the address space /
allocation failure, zero-filling nothing (new slots hold the given ref).

## Bulk instructions

`memory.copy`/`table.copy` use overlap-safe copying (memmove semantics,
matching the reference's direction-split expansion); they trap with the
`oob` check on the destination range against the destination, and the
source range against the source. `memory.fill`/`table.fill` trap on an
out-of-bounds destination range and otherwise fill. `memory.init`/
`table.init` copy from data/elem segments with `oob` checks on both the
destination range and the segment range, and `data.drop`/`elem.drop` empty
the segment. Operand pop order is `n, s, d` (n on top) and segment/table
index roles follow the decoder (`idx2` = data/elem segment, `idx` = mem/table).

## Cross-module import matching for ref-typed externs

`externtype_of_inst` previously left `env` unset for globals and tables,
so matching a ref-typed global/table import dereferenced a NULL type
environment and crashed. `w89_globalinst` and `w89_tableinst` now carry
the `const w89_typeenv *` of their defining module (set at allocation,
preserved through import/export so identity sharing keeps the owner's
types), and `externtype_of_inst` fills `xt->env` from it. Host-instance
globals/tables keep a NULL env, which is safe for the abstract/numeric
types spectest uses.

## Ref instructions

`ref.is_null` pushes 1/0; `ref.eq` compares refs by kind and identity
(null/null, func, extern, exn); `ref.as_non_null` traps with "null
reference" on null; `br_on_null`/`br_on_non_null` branch or keep the ref
on the operand stack per spec.

## Spectest

Adds the `table64` export (table64, 10-20 funcref) that the testsuite's
table64.wast imports. The reference spectest module defines it identically.

## REPL and driver

`module <path> [<name>]` registers a loaded instance under the optional
name (the JSON module `$name`) so actions can target it instead of only
`last`. `spec_driver.py` sends the module name when present, resolves
action `module` references against the registry, and treats the
module-linking proposal commands (`module_definition`, `module_instance`)
and `assert_uninstantiable` as out-of-scope skips, marking their instance
names so dependent imports/registers skip instead of failing.
