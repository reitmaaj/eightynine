# 0001-json89-concept

libj89 is a minimal JSON parser with strict engineering constraints.

## Purpose

Provide the `wid` sibling project with a tiny, auditable JSON parser that
satisfies the hard gate that larger JSON libraries (libjv2, jj's json)
cannot: the strict `green` C89∩C23 profile, semantic clang-tidy suite, and
canonical format.

## Supported JSON subset

- object, array
- string with escapes (`\"`, `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t`,
  `\uXXXX`), preserving valid UTF-8 content (invalid UTF-8 is rejected)
- integer numbers (stored as `double`, exact for |n| <= 2^53; larger integer
  tokens are rejected; an RFC 8259 section-6 implementation limit)
- fractional and exponent numbers (FLOAT, backed by a `double`)
- `true`, `false`, `null`
- an optional leading UTF-8 BOM is ignored
- duplicate object keys are rejected

## Explicitly unsupported

- a number whose magnitude exceeds `DBL_MAX` (no infinity/NaN)
- duplicate-key tolerance and ordering guarantees are not asserted by the
  parser (the consumer decides)

## Model

Parsing yields an owned value tree (arena) of typed nodes. The parser is
a hand-written recursive-descent scanner; the tree provides accessors used
by `wid` to walk a document semantically.
