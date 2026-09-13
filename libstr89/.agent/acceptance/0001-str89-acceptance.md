# 0001 — libstr89 acceptance

Release acceptance depends on every statement below holding. Each maps to
deterministic tests plus the recipes named in `Justfile`.

| ID | Statement | Evidence |
|---|---|---|
| A01 | Every public function has success and failure tests. | `test/unit/*`, `test/fault/*`; `scripts/api-coverage.sh` checks each header prototype name appears in `test/` |
| A02 | Every mutating failure proves destination unchanged. | `test/fault/test_fault_alloc.c`, `test/fault/test_overflow.c`, `test/model/test_model_invalid.c` |
| A03 | Every successful resulting string passes `libu89` validation. | `assert_valid_*` in `test/support` called after every operation |
| A04 | Every scalar boundary and non-boundary class gets exercised. | `test/unit/test_boundary.c`, `test/corpus/test_unicode_corpus.c` |
| A05 | All mutation primitives get self-alias tests. | `test/unit/test_alias.c` |
| A06 | Aliasing is tested with and without reallocation. | `test/unit/test_alias.c` (exact-capacity and spare-capacity variants) |
| A07 | Every allocating API gets exhaustive injected-allocation failure. | `test/fault/test_fault_alloc.c` |
| A08 | Every size arithmetic path gets overflow tests. | `test/fault/test_overflow.c`, `str89__add`/`str89__grow_cap` units |
| A09 | Embedded NUL passes through every relevant API. | `test/unit/test_nul.c` |
| A10 | No operation relies on NUL termination. | `scripts/str89-audit.sh`, `test/guard/test_guard_pages.c` |
| A11 | No implicit normalization or case folding occurs. | `test/unit/test_equal_compare.c`, `test/unit/test_nul.c`, audit |
| A12 | Grapheme boundaries impose no hidden restriction. | `test/unit/test_nul.c`, `test/unit/test_erase.c` |
| A13 | `take()` performs zero allocation and exact pointer transfer. | `test/unit/test_take.c` with fault-allocator counters |
| A14 | Random valid-operation sequences match a reference model. | `test/model/test_model.c` |
| A15 | Random invalid operations preserve state. | `test/model/test_model_invalid.c` |
| A16 | `libj89` preserves embedded-NUL values and keys. | `libj89` storage suite (integration phase) |
| A17 | `libj89` preserves canonically equivalent but byte-distinct keys. | `libj89` storage suite |
| A18 | JSON surrogate handling never admits invalid Unicode scalars. | `libj89` unicode suite |
| A19 | Parse/serialize round trips preserve exact Unicode strings. | `libj89` round-trip suite |
| A20 | C89, C23, and C++23 builds execute the same behavioral suite. | `just matrix`, `just cpp-check` |

## Unacceptable behavior (must reject)

- Accepting `NULL` data with a nonzero length.
- Accepting malformed UTF-8 through `str89_view_init`.
- Splitting a scalar through `sub`, `insert`, `erase`, `replace`, or `find`.
- Accepting a non-scalar code point through `append_cp`/`insert_cp`.
- Returning success after an allocation failure.
- Leaving a partially mutated destination after a failed mutation.
- Truncating at an embedded NUL or reading past the reported length.
- Normalizing, case folding, or collating text implicitly.
- Allowing `take` to copy, allocate, or overwrite a non-empty destination.
