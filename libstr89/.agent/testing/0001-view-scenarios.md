# 0001 — view scenarios

## Validation boundary

SCENARIO empty input
  GIVEN `data == NULL, len == 0`
  WHEN `str89_view_init` is called
  THEN the result is `STR89_OK`, the view is empty, and no allocation occurs

SCENARIO NULL with nonzero length
  GIVEN `data == NULL, len == 1`
  WHEN `str89_view_init` is called
  THEN the result is `STR89_EINVAL` and `*out` is unchanged

SCENARIO well-formed UTF-8
  GIVEN a valid sequence covering 1-, 2-, 3-, and 4-byte scalars, U+0000,
    U+007F, U+0080, U+07FF, U+0800, U+D7FF, U+E000, U+FFFF, U+10000, U+10FFFF
  WHEN `str89_view_init` is called
  THEN the result is `STR89_OK` and the view borrows the same bytes

SCENARIO malformed UTF-8
  GIVEN lone continuation bytes, overlong encodings, truncated sequences,
    surrogate encodings, values above U+10FFFF, and valid prefixes followed by
    invalid suffixes
  WHEN `str89_view_init` is called
  THEN the result is `STR89_EUTF8` and `*out` is unchanged

## Boundaries

SCENARIO boundary table
  GIVEN a string mixing ASCII, 2-byte, 3-byte, 4-byte, and U+0000 scalars
  WHEN `str89_view_is_boundary` is called for every offset `0..len`
  THEN the start, every scalar start, and the end report 1, and every
    continuation-byte offset reports 0

SCENARIO out-of-range boundary
  GIVEN any view
  WHEN `str89_view_is_boundary` is called with `len + 1` or `SIZE_MAX`
  THEN the result is 0 and no memory beyond the view is read

## Subviews

SCENARIO valid subview
  GIVEN a view and start/end offsets on scalar boundaries
  WHEN `str89_view_sub` is called
  THEN the result is `STR89_OK`, `*out` points into the original storage, and
    no allocation occurs

SCENARIO invalid subview
  GIVEN an offset past `len`, a length past the remainder, or a `SIZE_MAX`
    arithmetic case
  WHEN `str89_view_sub` is called
  THEN the result is `STR89_ERANGE` and `*out` is unchanged

SCENARIO split inside a scalar
  GIVEN an offset inside a multibyte scalar
  WHEN `str89_view_sub` is called
  THEN the result is `STR89_EBOUND` and `*out` is unchanged

SCENARIO empty subview at a boundary
  GIVEN a zero length at a scalar boundary
  WHEN `str89_view_sub` is called
  THEN the result is `STR89_OK`

## Equality and ordering

SCENARIO byte-exact equality
  GIVEN equal bytes in different storage, different lengths, first/middle/last
    byte differences, and embedded-NUL cases
  WHEN `str89_view_equal` is called
  THEN only exact byte equality returns 1

SCENARIO no normalization
  GIVEN U+00E9 and U+0065 U+0301
  WHEN `str89_view_equal` is called
  THEN the result is 0

SCENARIO unsigned-byte ordering
  GIVEN pairs with prefix, length, embedded-zero, and multibyte differences
  WHEN `str89_view_compare` is called
  THEN the sign matches an independent `memcmp`-style reference, is
    antisymmetric, and is transitive over generated triples

## Search

SCENARIO find positions
  GIVEN needle occurrences at the beginning, middle, end, repeated, absent,
    empty haystack, empty needle, and embedded NUL
  WHEN `str89_view_find` is called with various `from` offsets
  THEN the reported offset is the first match at or after `from`, or
    `STR89_NPOS` when absent, and an empty needle returns `from`

SCENARIO no match inside a scalar
  GIVEN a haystack whose continuation bytes resemble a needle byte pattern
  WHEN `str89_view_find` is called
  THEN no match begins inside a scalar

SCENARIO invalid search start
  GIVEN `from > len` or `from` inside a scalar
  WHEN `str89_view_find` is called
  THEN the result is `STR89_ERANGE` or `STR89_EBOUND` respectively
