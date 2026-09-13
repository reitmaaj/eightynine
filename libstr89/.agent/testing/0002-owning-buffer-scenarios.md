# 0002 — owning string and buffer scenarios

## Owning strings

SCENARIO initialized string
  GIVEN a `str89`
  WHEN `str89_init` is called
  THEN `data == NULL` and `len == 0`

SCENARIO finalize from a view
  GIVEN empty, ASCII, multibyte, embedded-NUL, and large views
  WHEN `str89_from_view` is called
  THEN the bytes are copied into a fresh allocation that does not alias the
    source, and mutating the source leaves the destination unchanged

SCENARIO copy
  GIVEN an empty and a nonempty source
  WHEN `str89_copy` is called
  THEN the destination equals the source and owns distinct storage

SCENARIO free is idempotent
  GIVEN an empty and a nonempty string
  WHEN `str89_free` is called once and then again
  THEN both calls succeed, `data == NULL`, and `len == 0`

## Buffer lifecycle

SCENARIO init/clear/free
  GIVEN a buffer
  WHEN `str89_buf_init`, `str89_buf_clear`, and `str89_buf_free` are called
  THEN init yields `{NULL, 0, 0}`; clear keeps the allocation and sets
    `len = 0`; free releases the allocation and resets all three fields;
    freeing twice is safe

SCENARIO append after clear
  GIVEN a cleared buffer
  WHEN bytes are appended
  THEN the bytes follow the cleared prefix and remain valid UTF-8

## Reserve

SCENARIO reserve never mutates content
  GIVEN any buffer state
  WHEN `str89_buf_reserve` is called with 0, a value below `len`, a value equal
    to `cap`, a value below `cap`, and a value above `cap`
  THEN bytes and `len` never change, success implies `cap >= capacity`, and
    failure leaves `data`, `len`, and `cap` unchanged

SCENARIO unrepresentable capacity
  GIVEN a capacity whose growth overflows `size_t`
  WHEN `str89_buf_reserve` is called
  THEN the result is `STR89_ERANGE` and the allocator is not called

## Set, append, insert

SCENARIO set replaces content
  GIVEN empty, shorter, equal, and longer sources, including embedded NUL
  WHEN `str89_buf_set` is called
  THEN the buffer contains exactly the source bytes and remains valid

SCENARIO append cross product
  GIVEN destinations that are empty, nonempty, at exact capacity, and with
    spare capacity
  AND sources that are empty, ASCII, 2/3/4-byte, mixed, and embedded-NUL
  WHEN `str89_buf_append` is called
  THEN the buffer is the concatenation of destination and source

SCENARIO insert at every boundary
  GIVEN a buffer of mixed-width scalars
  WHEN a source is inserted at 0, every scalar boundary, and `len`
  THEN the buffer is the reference insertion; insertion inside a scalar is
    `STR89_EBOUND`

## Erase, replace

SCENARIO erase shapes
  GIVEN zero-byte, first-scalar, last-scalar, middle, multiple, whole,
    prefix, suffix, and interior ranges
  WHEN `str89_buf_erase` is called
  THEN the result matches the reference model; out-of-range is `STR89_ERANGE`
    and a split scalar is `STR89_EBOUND`

SCENARIO replace cross product
  GIVEN removed lengths of 0, one scalar, many scalars, and the whole string
  AND replacement sources that are empty, shorter, equal-length, longer,
    ASCII, multibyte, and embedded-NUL
  WHEN `str89_buf_replace` is called
  THEN the result matches erase-then-insert of the same range

## Code points

SCENARIO append/insert scalar values
  GIVEN U+0000, U+0001, U+007F, U+0080, U+07FF, U+0800, U+D7FF, U+E000,
    U+FFFF, U+10000, and U+10FFFF
  WHEN `str89_buf_append_cp` or `str89_buf_insert_cp` is called
  THEN the appended bytes equal `u89_utf8_encode` output; U+D800, U+DFFF, and
    values above U+10FFFF return `STR89_EINVAL`

## Take

SCENARIO ownership transfer
  GIVEN empty and nonempty buffers, including embedded NUL
  WHEN `str89_take` is called
  THEN `out.data == old_buf.data`, `out.len == old_buf.len`,
    `src == {NULL, 0, 0}`, and the allocator call counters do not change

SCENARIO non-empty destination
  GIVEN a destination that already owns bytes
  WHEN `str89_take` is called
  THEN the result is `STR89_EINVAL` and both values are unchanged
