# 0001-data-model

## Value tree

A parsed document is an owned tree. Nodes are referenced by index into a
single arena, or by pointer. Because the library is C89 and obligation-
clean, we use explicit indices into a growable arena.

```text
node_kind: NULL | FALSE | TRUE | INTEGER | FLOAT | STRING | ARRAY | OBJECT

node:
    kind
    union {
        j89_int       integer        (`double`, exact for |n| <= 2^53; integer
                                      tokens beyond that range are rejected so
                                      every accepted integer is exact. This is
                                      an RFC 8259 section-6 permitted
                                      implementation limit on numeric range.)
        double        float          (any number with a fraction or exponent;
                                      rejected if the magnitude exceeds
                                      DBL_MAX so no infinity/NaN is stored)
        string_ref    string bytes + length (NUL-terminated at the length)
        array         first child index + length
        object        first member index + length
    }

member:
    key (string_ref, owned)
    value (node index)
```

## Allocation

A single arena (`struct j89_arena`) owns all nodes, string bytes, array
slots, and object members. Parsing allocates from the arena; freeing the
arena releases everything. No per-node malloc/free.

## API (include/j89.h)

- `j89_parse(const char *buf, j89_len len, j89_arena *a)` -> node root index
  (or `J89_BAD` on error), with a message recorded in the arena.
- accessors: `j89_kind`, `j89_int_value`, `j89_double_value`,
  `j89_string_value/length`, `j89_array_len/elem`, `j89_object_len`,
  `j89_object_find`, iteration.
- `j89_render` -> pretty/compact printer used by `widc` metadata output and
  diagnostics.

## Rationale

The arena model keeps lifetimes trivial and avoids hidden calls inside
expressions (obligation-clean). The full RFC 8259 number grammar is accepted:
an integer token (no fraction/exponent) yields an INTEGER node; any token with
a fraction or exponent yields a FLOAT node. Both are stored as a `double`
(IEEE-754 binary64). `double` is ISO C89 and covers the grammar without
`long long`/`<stdint.h>` (neither is C89). Strings and object keys are
validated as UTF-8 on input and may carry a single optional leading UTF-8 BOM.
Duplicate object keys are rejected so that `j89_object_find` (which returns
the first match) stays well-defined.

All numbers share a single `double` representation (RFC 8259 section 6
explicitly permits implementations to limit numeric range/precision and names
`double` as the expected interoperability baseline). Integer tokens are exact
for |n| <= 2^53 and are rejected beyond that range, so no accepted integer is
approximated.
