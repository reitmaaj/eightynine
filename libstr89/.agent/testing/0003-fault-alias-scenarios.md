# 0003 — fault, alias, and model scenarios

## Allocation failure

SCENARIO exhaustive injected allocation failure
  GIVEN a deterministic fault allocator that fails the Nth allocation
  WHEN each allocating API is called with N = 1, 2, 3, ... until the call
    succeeds with no injected failure
  THEN every failing call returns `STR89_ENOMEM`, leaves logical content,
    `len`, and `cap` unchanged, owns the original allocation exactly once,
    and leaks nothing

SCENARIO take performs no allocation
  GIVEN a buffer with content
  WHEN `str89_take` is called with the fault allocator armed to fail
  THEN the call succeeds and the allocator call counters do not change

## Aliasing

SCENARIO append self and subviews
  GIVEN a buffer
  WHEN a whole-buffer view, a prefix, an interior slice, or a suffix is
    appended, once with spare capacity and once forcing reallocation
  THEN the result matches the reference model and remains valid UTF-8

SCENARIO insert self and subviews
  GIVEN a buffer
  WHEN a whole-buffer view, a prefix, an interior slice, or a suffix is
    inserted at every boundary, once with spare capacity and once forcing
    reallocation
  THEN the result matches the reference model

SCENARIO replace self and subviews
  GIVEN a buffer
  WHEN the replacement aliases the region before, after, exactly at, or
    spanning the removed range, with and without reallocation
  THEN the result matches the reference model and no partial overlap is
    observable

SCENARIO set self and subviews
  GIVEN a buffer
  WHEN it is set from its own whole view, a prefix, or a suffix
  THEN the result matches the reference model

## Checked arithmetic

SCENARIO overflow paths
  GIVEN crafted lengths and offsets near `SIZE_MAX`
  WHEN length addition, offset plus length, capacity growth multiplication,
    and allocation size computation overflow
  THEN the result is `STR89_ERANGE`, no allocator call occurs, and no state
    changes

## Reference model

SCENARIO valid operation sequences
  GIVEN a reference model of a growable byte string and a deterministic seed
  WHEN random `set`, `append`, `append_cp`, `insert`, `insert_cp`, `erase`,
    `replace`, `clear`, `reserve`, and `take` operations are applied to both
    the model and a real buffer
  THEN after every operation the bytes and length match, the buffer validates
    under `libu89`, and `len <= cap`

SCENARIO invalid operation sequences
  GIVEN random operations that choose continuation-byte offsets, oversized
    lengths, `SIZE_MAX`-derived ranges, invalid scalars, and forced allocation
    failures
  WHEN the operation is applied
  THEN the expected error class is returned and the buffer is bit-for-bit
    unchanged

SCENARIO metamorphic identities
  GIVEN generated strings and boundaries
  WHEN the identities `append(a, empty) == a`, `insert(a, i, empty) == a`,
    `erase(a, i, 0) == a`, `replace(a, i, 0, x) == insert(a, i, x)`,
    `replace(a, i, n, empty) == erase(a, i, n)`, `set(a, view(a)) == a`,
    `sub(a, 0, len(a)) == a`, `find(a, a, 0) == 0`, and
    `find(a, empty, i) == i` are exercised
  THEN each identity holds

## Non-behavior

SCENARIO no normalization or case folding
  GIVEN U+00E9 vs U+0065 U+0301, A vs a, and ss vs U+00DF
  WHEN equality, ordering, search, copy, append, set, and take are exercised
  THEN no pair is silently transformed or made equal

SCENARIO grapheme boundaries are not imposed
  GIVEN e + COMBINING ACUTE, emoji + variation selector, a regional-indicator
    pair, and a ZWJ emoji sequence
  WHEN editing at a scalar boundary inside the cluster is requested
  THEN the operation succeeds, proving scalar safety without grapheme policy

## Embedded NUL

SCENARIO NUL corpus
  GIVEN `"\0"`, `"a\0b"`, `"\0a"`, `"a\0"`, `"\0\0"`, and `"€\0𐍈"`
  WHEN every public API is exercised
  THEN no operation truncates, drops, or reorders bytes, and no NUL-string
    assumption is observable
