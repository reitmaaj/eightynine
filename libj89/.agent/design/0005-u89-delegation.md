# 0005 — Unicode delegation to libu89

## Responsibility boundary

`libu89` owns Unicode scalar/encoding validity; `libj89` owns JSON syntax and
representation. The practical parser no longer contains a UTF-8 codec.

| Concern | Owner |
|---|---|
| UTF-8 validity | `u89` |
| UTF-8 scalar decode/encode | `u89` |
| Unicode scalar validity | `u89` |
| UTF-16 surrogate classification / pair -> scalar | `u89` |
| JSON `\uXXXX` lexical syntax | `j89` |
| high surrogate must be followed immediately by a second `\uXXXX` | `j89` |
| JSON raw-control rule U+0000..U+001F | `j89` |
| JSON leading BOM policy | `j89` |
| JSON quoting/escaping | `j89` |
| strings stored as UTF-8 bytes | `j89` |
| normalization / case folding | not applied by `j89` |

`j89` does not call `u89_normalize_ex` or `u89_casefold`: JSON strings preserve
Unicode scalar sequences exactly. `j89` also does not use `u89_is_control`,
whose `Cc` set includes DEL and C1; the JSON lexical rule is specifically
U+0000..U+001F.

## Representation invariant

Every `J89_STRING` node and every object key in a `j89_arena` contains
well-formed UTF-8. This holds for both routes:

- parser: validates while decoding, then calls the private unvalidated
  `j89_add_string()`;
- builder: `j89_string_new()` and `j89_object_set()` call `u89_utf8_valid()`
  at the public boundary before allocating.

Validation happens before allocation, so malformed builder input does not
consume arena storage.

## Dependency

    libu89
      ^
      |
    libj89
      ^
      |
    libjrpc89 / llm89

`j89.h` does not include `u89.h`; no `u89` type enters the public J89 ABI.
This is an implementation/link dependency, not API coupling. `j89_alg` has no
`u89` dependency: `S` remains an uninterpreted byte-string carrier.

## J89 ABI compatibility

The migration changes no existing public symbol, struct layout, enum value,
calling convention, or documented semantic. Exactly one public symbol is
added:

```c
j89_len j89_object_key_length(j89_arena *a, j89_len node, j89_len i);
```

A j89 object member already stores its key length explicitly, and the builder
already permits embedded NUL bytes in keys. Without a length accessor those
keys could not be inspected faithfully, and `src/bridge.c` reconstructed key
length with `strlen()`, silently truncating at the first NUL. The accessor
makes the existing representation observable and lets the algebraic bridge
carry keys exactly.

- Source-compatible: existing callers compile unchanged.
- Link-compatible: existing symbols are unchanged; the new symbol is additive.
- Binary-compatible: no public struct/enum/typedef changes; `j89_arena` and
  node indices are untouched.
- Semantic tightening (documented invariant, not an ABI break): builder inputs
  that are not well-formed UTF-8 are now rejected before allocation. Callers
  that previously stored malformed bytes must handle `J89_BAD` and
  `j89_failed()`. Parsed documents are unaffected.

## Source-level deletion criterion

After the migration, `src/*.c` (excluding `src/alg`) contains no UTF-8
encoding/decoding algorithm: no lead-length classification, continuation
checks, overlong checks, scalar encoder, surrogate predicates, or surrogate
arithmetic. The only Unicode operations are calls to `u89_utf8_decode`,
`u89_utf8_encode`, `u89_utf8_valid`, `u89_utf16_is_high_surrogate`,
`u89_utf16_is_low_surrogate`, and `u89_utf16_decode_pair`. The
`just unicode-audit` tripwire enforces this.
