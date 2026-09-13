# Acceptance criteria

## Must exhibit

1. The four one-shot functions return the published numeric values for the
   frozen inputs, including the official NVMe 4096-byte vectors.
2. Production equals the bit-at-a-time reference model for every algorithm
   over the corpora and length boundaries in `.agent/testing/`.
3. Every partition of a byte string produces the same value as the one-shot
   computation; exhaustive for lengths up to 12.
4. INET16 produces the same value whether updates end on odd or even byte
   boundaries, and the RFC 1071 verification property holds.
5. `final` does not mutate; repeated `final` calls agree; contexts copy by
   assignment and branch independently.
6. `update(ctx, NULL, 0)` and zero-length inserts are neutral.
7. Every lookup-table entry equals an independently generated reference
   remainder.
8. Results are independent of buffer alignment (+0..+7) and of host
   byte order.
9. The public header compiles as C89 and as C++23 and keeps its C ABI.
10. `CHAR_BIT == 8` is the only machine-model requirement, checked at
    compile time.

## Must reject / must not happen

1. A translation unit that overrides `CHAR_BIT` to a value other than 8
   MUST fail to compile (`test/compile/char_bit_guard.c`, `just guard`).
2. The library MUST NOT reference allocation or I/O symbols
   (`scripts/audit.sh`).
3. The library MUST NOT modify caller input bytes or the input pointer.
4. `final` MUST NOT change any context byte.
5. Values MUST NOT contain significant bits outside their declared domains.
6. Passing `ctx == NULL` or `data == NULL` with `len != 0` is a documented
   precondition violation; it is undefined behavior and is not a status the
   library reports.
7. No error enum, no allocation, no `destroy`, no runtime algorithm
   selector, and no serialization helper may appear in the public header.
