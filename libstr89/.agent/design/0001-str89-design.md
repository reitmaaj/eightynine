# 0001 — libstr89 design

## Types

```c
typedef struct str89_view { const unsigned char *data; size_t len; } str89_view;
typedef struct str89      { unsigned char *data;       size_t len; } str89;
typedef struct str89_buf  { unsigned char *data;       size_t len; size_t cap; } str89_buf;
```

`str89_view` is borrowed. `str89` and `str89_buf` own their `data` block,
allocated through the caller-supplied allocator; `str89_buf` additionally
tracks allocated capacity. No value stores its allocator.

Empty representation: `data == NULL, len == 0` (and `cap == 0` for a buffer);
constructing an empty value performs no allocation.

## Allocator

```c
typedef void *(*str89_malloc_fn)(void *ctx, size_t size);
typedef void *(*str89_realloc_fn)(void *ctx, void *ptr, size_t size);
typedef void (*str89_free_fn)(void *ctx, void *ptr);
typedef struct str89_alloc { void *ctx; str89_malloc_fn malloc_fn; str89_realloc_fn realloc_fn; str89_free_fn free_fn; } str89_alloc;
```

A NULL `str89_alloc *` selects the C library `malloc`/`realloc`/`free`. The
allocator is passed per call and never stored. The allocator used for
destruction MUST match the allocator used for allocation. Realloc failure
leaves the original block owned by the value; the value is unchanged.

## Status codes

`str89_status` is an enum with `STR89_OK == 0` and negative errors:

| Code | Meaning |
|---|---|
| `STR89_EINVAL` | invalid argument (NULL data with nonzero length, non-scalar code point, `take` into a non-empty destination) |
| `STR89_ENOMEM` | allocator returned NULL |
| `STR89_EUTF8` | input is not well-formed UTF-8 |
| `STR89_ERANGE` | byte range is out of bounds, or size arithmetic would overflow |
| `STR89_EBOUND` | an offset or range end does not lie on a scalar boundary |

`STR89_NPOS` is `(size_t)-1`, the "not found" offset.

## Module map

| File | Responsibility |
|---|---|
| `src/alloc.c` | allocator resolution, checked add/mul, growth policy |
| `src/view.c` | validation, boundaries, subviews, equality, ordering, search |
| `src/string.c` | owning finalization, copy, free |
| `src/buf.c` | builder lifecycle, reserve, set, append, insert, erase, replace, code-point insertion, take |

`src/str89_internal.h` declares the checked helpers shared by the modules
and by white-box tests.

## Invariants

1. `str89_view`, `str89`, and `str89_buf` contain well-formed scalar UTF-8.
2. `len` always counts bytes.
3. Embedded zero bytes carry no special meaning.
4. No terminator guarantee exists.
5. A successful mutation preserves UTF-8 validity.
6. A failed mutation leaves the destination bit-for-bit unchanged.
7. Editing can occur only at Unicode scalar boundaries.
8. No operation implicitly normalizes text.
9. Equality compares exact UTF-8 bytes.
10. `libstr89` contains no Unicode tables and no duplicate decoder.
11. All Unicode semantics come from `libu89`.
12. Owning strings use the allocator supplied by their owner.
13. `str89_take()` transfers ownership without copying or allocating.
14. A borrowed view remains valid only while the backing storage remains
    unchanged and alive.

## Validation boundary

`str89_view_init(out, data, len)`:

- `data == NULL && len == 0` -> empty view, `STR89_OK`;
- `data == NULL && len != 0` -> `STR89_EINVAL`, `*out` unchanged;
- `u89_utf8_valid(data, len) == 0` -> `STR89_EUTF8`, `*out` unchanged;
- otherwise `STR89_OK` with `{data, len}`.

A view fabricated directly by a caller is the caller's responsibility; the
library assumes every `str89_view` it receives is validated.

## Offsets and boundaries

`str89_view_is_boundary` returns 1 for offset 0 and offset `len`, and for
`0 < offset < len` delegates to `u89_utf8_prev` (which reports whether the
offset lies on a scalar boundary). Out-of-range offsets return 0 and read no
memory.

Range checks precede boundary checks. For a mutator, an out-of-bounds byte
range is `STR89_ERANGE`; a range that is in bounds but not on scalar
boundaries is `STR89_EBOUND`. A zero-length range at a boundary is legal; a
zero-length range inside a scalar is `STR89_EBOUND`.

## Mutation guarantees

Every fallible mutator validates arguments and computes the required size
before any destructive movement, and performs allocation before mutation.
Failure leaves the destination bit-for-bit semantically unchanged for
`ENOMEM`, `EUTF8`, `ERANGE`, `EBOUND`, and `EINVAL`.

## Aliasing

A source view may point into the destination buffer, including a whole-buffer
view, a prefix, an interior slice, or a suffix. `append` uses `memmove`
because the source range ends at or before the destination range.
`insert` and `replace` may move the bytes that back the source; when the
source overlaps the destination they are first copied to a temporary block
(allocated through the same allocator). If that temporary allocation fails,
the call returns `ENOMEM` and the destination is unchanged. When a
reallocation moves the destination block, an aliased source pointer is
recomputed from its offset before use.

## Growth

`str89_buf_reserve(s, alloc, capacity)` ensures `cap >= capacity`; it never
shrinks and never changes bytes or `len`. Growth starts at a small constant
and doubles until the request fits, with overflow checks; a request that
cannot be represented returns `STR89_ERANGE` without calling the allocator.
The exact growth sequence is private.

## Lifecycle contracts

- `str89_init` / `str89_buf_init`: `{NULL, 0}` / `{NULL, 0, 0}`.
- `str89_buf_clear`: `len = 0`, allocation retained.
- `str89_buf_free` / `str89_free`: release the block and reset to the empty
  representation; repeated calls are safe.
- `str89_take(out, src)`: `out` must be an initialized empty string; transfers
  `src`'s block and length to `out`, then resets `src` to `{NULL, 0, 0}`.
  Performs no allocation, no copy, and no allocator call.

## Non-behavior

No normalization, case folding, collation, grapheme segmentation, locale
dependence, or NUL-terminated-string assumption. `scripts/str89-audit.sh`
enforces the absence of a Unicode codec and of `strlen`-family APIs in `src/`.
