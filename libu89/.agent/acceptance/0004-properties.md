# libu89 acceptance tests — terminal-relevant scalar properties

## 0005 — Unacceptable behavior (must reject / fail safely)

- P-07. A non-scalar argument (surrogate, or above U+10FFFF) MUST be
  rejected by every predicate (`0` / `U89_EAW_N`), never classified as a
  printable or wide scalar.
- P-08. `u89_is_control` MUST NOT report `Cf`, `Zs`, or `Cs` scalars as
  controls; it is General_Category `Cc` only.
- P-09. `u89_is_mark` MUST NOT report non-mark scalars (letters, symbols,
  emoji modifiers) as marks.
- P-10. East Asian Width MUST NOT classify a narrow/neutral scalar as wide,
  or an unassigned scalar as anything but `N`.

## 0006 — Required behavior (must exhibit)

- P-01. `u89_is_control` is 1 for LF, HT, ESC, DEL, and C1 scalars
  (e.g. U+0085, U+009B), and 0 for printable scalars.
- P-02. `u89_is_mark` is 1 for `Mn` (U+0301), `Mc` (U+0903), and `Me`
  (U+20DD), and 0 otherwise.
- P-03. `u89_is_emoji` is 1 for U+1F600 and U+00A9, 0 for U+0041.
- P-04. `u89_is_emoji_presentation` is 1 for U+1F600, 0 for U+00A9 and
  U+2708 (Emoji without Emoji_Presentation).
- P-05. `u89_east_asian_width` returns `NA`, `A`, `H`, `W`, `F`, and the
  default `N` for representative scalars of each class.
- P-06. `u89_default_ignorable` keeps its established behavior (soft hyphen,
  ZWSP, WJ, CGJ in; letters and space out).
- P-11. Runtime property results agree with the vendored UCD ranges for
  every represented range boundary and representative values
  (`tools/gen_check.py`, `just gendiff`).
