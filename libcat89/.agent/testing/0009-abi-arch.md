# libcat89 testing scenarios (BDD) - W7 ABI/architecture audit

## 0009-abi-arch.md

W7 is a verification/audit workstream. The architecture gate (`just arch`,
wired into `just lint`) enforces the normative layering and ABI-surface rules
from `spec/0000-overview.md` so future changes cannot silently regress them.

- SCENARIO core isolation: GIVEN the checked-in public headers WHEN the
  architecture gate inspects `include/cat89/core.h` THEN its `<cat89/...>`
  includes are within `{core.h, alloc.h, status.h}` (no derived header), and a
  translation unit that includes only `<cat89/core.h>` compiles clean in strict
  C89.
- SCENARIO header DAG: GIVEN every public header WHEN the gate builds the
  directed include graph over the `<cat89/...>` includes THEN the graph has no
  cycle.
- SCENARIO exported ABI surface: GIVEN the built static library WHEN the gate
  enumerates its defined global symbols THEN every symbol carries the `cat89_`
  prefix and is declared in a public header (or `cat89_internal.h`), and NO
  test/fixture symbol leaks into the shipped library.
- SCENARIO struct exposure review: GIVEN the public headers WHEN the gate lists
  the concrete `struct cat89_*` definitions THEN the exposed layouts are exactly
  the intentional public ops/allocator/result structs, and core/backends expose
  only opaque handles.
