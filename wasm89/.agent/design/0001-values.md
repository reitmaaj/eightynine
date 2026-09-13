# Design: values (spec 2.2, 4.3, 5.2)

## Scalar representation (C89)

* `w89_byte` = `unsigned char`
* `w89_u32`/`w89_i32` = `unsigned int`/`int` (compile-time sizeof assert)
* `w89_u64`/`w89_i64` = `unsigned long`/`long` (LP64; compile-time sizeof
  assert in `leb.h`)
* `w89_f32`/`w89_f64` = `float`/`double`

## LEB128 (spec 5.2.2)

Generic decoders `w89_leb_u`/`w89_leb_s` take an explicit `nbits`
parameter and thin wrappers fix N to 8/32/64 as needed.

Decode rules implemented:

* unsigned uN: at most `ceil(N/7)` bytes; value must be `< 2^N`; bits at
  or above position N in the terminal byte must be zero. Overlong
  encodings within the byte bound (trailing zeros) are allowed.
* signed sN: at most `ceil(N/7)` bytes; after sign extension to 64 bits,
  every bit at or above position N-1 must equal bit N-1. For the single
  case where the terminal byte's payload reaches past bit 63 (s64, 10
  bytes), the high payload bits must be sign-consistent with bit 63.
* On failure the decoder returns 0 and leaves the cursor untouched; on
  success it returns 1, writes the value and advances the cursor.

These rules are the ones exercised by the official `binary-leb128.wast`
and `binary_leb128_64.wast` malformed cases.

## Status

The values milestone (spec 2.2, 4.3, 5.2) scalar surface is complete and
committed on branch `numerics`:

* LEB128 decoders (spec 5.2.2) — `src/leb.c`
* Scalar integer operations (spec 4.3.2) — `src/numeric.c`
* Floating-point operations under the DET profile (spec 4.3.3)
  — `src/numeric.c`; NaN canonicalization to 0x7FC00000 / 0x7FF8000000000000
* Conversions incl. saturating truncation and extend8/16/32 (spec 4.3.4)
  — `src/numeric.c`

Float correctness relies on: `-ffp-contract=off`, SSE2 float semantics on
x86-64, `__builtin_sqrtf/ceilf/floorf` (no double rounding for f32),
memcpy-based bit reinterpretation, and trunc-boundary checks performed on
exact integer truncations in `double`.

## Next: binary decoder (spec ch5)

`src/module.h` defines the decoded module data model (growable arrays via a
small arena; `w89_module_free` for cleanup). `src/decoder.c` walks the
binary (spec 5.5.17) with `w89_byte *cursor` + `end`; every malformed
condition maps to a `w89_err` code. Type encodings follow A.9 / 5.3:
numtype 0x7C-0x7F, vectype 0x7B, heaptype 0x69-0x74 (exn=0x69 ...
noexn=0x74), reftype 0x63 (ref null) / 0x64 (ref), comptype 0x60 (func)
0x5F (struct) 0x5E (array), subtype 0x4F (final) 0x50, rectype 0x4E,
limits flags 0x00/0x01 (i32) and 0x04/0x05 (i64, memory64).
