# libu89 acceptance tests — positional UTF-8 interface

## 0002 — Unacceptable behavior (must reject / fail safely)

The following behaviors are UNACCEPTABLE and MUST be rejected or reported.
Each maps to an automated test in `test/test_utf8.c` and
`test/test_consistency.c`.

- U8-05. Overlong encodings MUST be rejected: `C0 80`, `C0 AF`, `E0 80 80`,
  `F0 80 80 80`.
- U8-06. A UTF-8-encoded surrogate (`ED A0 80`) MUST be rejected.
- U8-07. Encodings above U+10FFFF (`F4 90 80 80`, `F5 80 80 80`) MUST be
  rejected.
- U8-08. Isolated continuation bytes (`80`, `BF`) MUST be rejected.
- U8-09. Truncated sequences MUST be rejected (`C2`, `E2 82`, `F0 9F 8C`).
- U8-10. A valid lead followed by a non-continuation byte MUST be rejected
  (`C2 41`, `E2 82 41`, `F0 9F 8C 41`).
- U8-12. Decoding at `pos == n` or `pos > n` MUST return `U89_ERANGE`, never a
  scalar and never `U89_EUTF8`.
- U8-15. `u89_utf8_prev` at `pos == 0` MUST return `U89_ERANGE`.
- U8-17. `u89_utf8_prev` over malformed bytes, or at a position that is not a
  scalar boundary, MUST return `U89_EUTF8`.
- Invalid leading bytes (`C0`, `C1`, `FE`, `FF`) MUST NOT decode.

## 0003 — Required behavior (must exhibit)

- U8-01. ASCII boundaries: U+0000 and U+007F decode to one byte.
- U8-02. Two-byte boundaries: U+0080 and U+07FF decode to two bytes.
- U8-03. Three-byte boundaries: U+0800, U+D7FF, U+E000, U+FFFF decode to three
  bytes.
- U8-04. Four-byte boundaries: U+10000 and U+10FFFF decode to four bytes.
- U8-11. Forward/backward agreement: decoding forward from `p` yields `next`;
  `u89_utf8_prev(next)` returns the same scalar and `p`.
- U8-13. A mixed ASCII/CJK/emoji/combining string validates and decodes exactly.
- U8-14. Embedded U+0000 is accepted; explicit byte length governs input.
- U8-16. Decoding at an interior position returns the scalar beginning there.
- U8-18. Every Unicode scalar round-trips encode -> decode exhaustively.
- U8-19. `u89_utf8_seq_len` reports 1..4 for valid leading bytes, 0 for
  continuation bytes, 0 for the overlong leads C0/C1, and 0 for leads above
  F4, so streaming callers can tell a truncated sequence from a malformed
  one without owning continuation-byte rules.

## 0004 — Status codes

- `U89_OK` is 0; error codes are negative and preserve the pre-existing values.
- `U89_ERANGE` is `-6`.
