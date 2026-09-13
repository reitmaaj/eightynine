# 0006 — owned str89 storage acceptance

| ID | Statement | Evidence |
|---|---|---|
| A16 | `libj89` preserves embedded-NUL values and keys. | `test/unit/test_str_storage.c` |
| A17 | `libj89` preserves canonically equivalent but byte-distinct keys. | `test/unit/test_str_storage.c` |
| A18 | JSON surrogate handling never admits invalid Unicode scalars. | `test/unit/test_unicode.c`, `test/unit/test_str_storage.c` |
| A19 | Parse/serialize round trips preserve exact Unicode strings. | `test/unit/test_str_storage.c`, `test/e2e/rfc8259.sh`, corpus |

Additional must-hold statements:

- Every string node and key still satisfies the `j89.h` NUL-terminator
  contract.
- `j89_object_key_length` returns the exact byte length for embedded-NUL keys.
- `j89_arena_destroy` releases every owned string exactly once (valgrind /
  sanitizer clean).
- `j89_alg` gains no Unicode or `str89` dependency (`just unicode-audit`).
- The existing public ABI remains source-compatible: only the arena size
  changes with the added `strhead` field.

Unacceptable behavior (must reject):

- Truncating a key or value at an embedded U+0000.
- Treating U+00E9 and U+0065 U+0301 as the same key.
- Admitting a lone surrogate or a non-scalar escape into stored text.
- Leaking or double-freeing a registered string on a failed parse.
