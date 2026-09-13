# libu89 acceptance tests — UTF-16 surrogate primitives

## 0005 — Unacceptable behavior (must reject / fail safely)

- U16-06. A high surrogate paired with a BMP unit (`D800 0041`) MUST fail.
- U16-07. A BMP unit paired with a low surrogate (`0041 DC00`) MUST fail.
- U16-08. A high surrogate paired with another high surrogate (`D800 D800`)
  MUST fail.
- U16-09. A low surrogate paired with another low surrogate (`DC00 DC00`)
  MUST fail.
- U16-10. An invalid pair MUST leave `*cp` unchanged.
- U16-11. `u89_utf16_is_high_surrogate` MUST NOT report true for 0xD7FF,
  0xDC00..0xDFFF, or any value above 0xFFFF.
- U16-12. `u89_utf16_is_low_surrogate` MUST NOT report true for 0xD7FF,
  0xD800..0xDBFF, or any value above 0xFFFF.

## 0006 — Required behavior (must exhibit)

- U16-01. `u89_utf16_is_high_surrogate` is true exactly for 0xD800..0xDBFF.
- U16-02. `u89_utf16_is_low_surrogate` is true exactly for 0xDC00..0xDFFF.
- U16-03. `D800 DC00` decodes to U+10000.
- U16-04. `D800 DFFF` decodes to U+103FF.
- U16-05. `DBFF DC00` decodes to U+10FC00.
- U16-13. `DBFF DFFF` decodes to U+10FFFF.
- U16-14. Passing `cp == NULL` performs only the validity test and succeeds
  for a valid pair.
- U16-15. For representative valid pairs, the decoded scalar encodes to four
  UTF-8 bytes that decode back to the same scalar, and
  `u89_utf16_units(cp) == 2`.
