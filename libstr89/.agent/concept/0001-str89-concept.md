# 0001 — libstr89 concept

## Semantic model

A `str89` string represents:

> An exact sequence of Unicode scalar values encoded as well-formed UTF-8,
> preserving the original UTF-8 byte representation.

Valid UTF-8 gives each scalar exactly one canonical encoding, so preserving
the byte representation is the same as preserving the scalar sequence
exactly.

Consequences:

- explicit byte length;
- embedded U+0000 is an ordinary scalar;
- no `strlen()` semantics and no trailing-NUL requirement;
- no implicit normalization, case folding, locale, or collation;
- byte-exact equality; UTF-8 byte offsets;
- editing offsets must fall on scalar boundaries;
- grapheme boundaries remain a `libu89` concern.

These two strings remain different:

```text
U+00E9
U+0065 U+0301
```

even though normalization could make them canonically equivalent. That
matters for `libj89`: parsing JSON must not silently rewrite string contents.

## Three types

```c
str89_view     borrowed validated UTF-8
str89          owning finalized UTF-8 string
str89_buf      mutable UTF-8 builder
```

The split keeps finalized values compact (no capacity word per JSON string or
key) while the parser gets an efficient builder. `str89_view` is the single
interchange type for validated text.

## Dependency shape

```text
              libu89
             ^      ^
        libstr89     \
             ^        \
              \        \
               libj89 --+   (JSON-specific \uXXXX/surrogate processing)
```

`libstr89` handles validated UTF-8 storage; `libj89` still calls `libu89`
directly for JSON-specific `\uXXXX`/surrogate processing.

## Validation boundary

`str89_view_init` carries the architectural weight: it asks `libu89` to
validate the complete byte sequence. Rules:

```text
data == NULL && len == 0       valid empty string
data == NULL && len != 0       STR89_EINVAL
invalid UTF-8                  STR89_EUTF8
well-formed UTF-8              STR89_OK
```

Validation inherits `libu89`'s scalar rules: overlong encodings,
continuation errors, truncated sequences, UTF-16 surrogate scalar values, and
values above U+10FFFF are all rejected. After successful construction, a
`str89_view` means validated Unicode text; every other operation can assume
validity.

## Offsets

Offsets are always UTF-8 byte offsets. There are no code-point-indexed APIs:
their cost and meaning would be misleading. Operations that split existing
text require a scalar boundary; a scalar boundary does not imply a
user-perceived (grapheme) boundary. Callers wanting grapheme-aware editing ask
`libu89` for grapheme boundaries and then call `libstr89` with the resulting
byte offsets. The same principle applies to normalization, case folding, and
properties: no duplicate Unicode algorithms.

## Equality and ordering

Equality is `len` equality plus byte equality. Ordering is unsigned-byte
lexicographical comparison. Neither is locale ordering, alphabetical
ordering, Unicode collation, or canonical-equivalence ordering.

## Non-goals

`strlen`-style APIs, NUL-terminated-string APIs, locale APIs, ASCII
classification, Unicode properties, normalization, case folding, collation,
grapheme/word segmentation, display width, formatting, numeric conversion,
regex, tokenization, split/join, JSON escaping, UTF-16/UTF-32 storage, and
general byte buffers.
