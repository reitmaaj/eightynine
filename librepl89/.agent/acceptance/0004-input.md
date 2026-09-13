# librepl89 acceptance tests — key decoder

## 0007 — Unacceptable behavior (must reject / fail safely)

- IO-10. An unknown complete escape sequence MUST be consumed and ignored;
  it MUST NOT be partially interpreted as another valid command.
- IO-13. An incomplete escape sequence MUST report zero consumed bytes; the
  caller MUST be able to retain every byte for the next read.
- IO-14. The decoder MUST NOT read past the supplied byte range.
- IO-15. Malformed UTF-8 bytes MUST NOT be reported as text; they are
  consumed one byte at a time and ignored.
- IO-16. Decoder behavior MUST NOT depend on read fragmentation, including
  for over-long unknown sequences: the sequence-cap window is fixed, so a
  stream decodes to the same events whether it arrives whole or one byte at
  a time. An over-long sequence MUST NOT have a prefix consumed while its
  continuation is reinterpreted.

## 0008 — Required behavior (must exhibit)

- IO-01. Ordinary ASCII and UTF-8 scalars are reported as text with the
  exact byte length (1-4).
- IO-02. Arrow keys decode from both CSI (`ESC [ A/B/C/D`) and SS3
  (`ESC O A/B/C/D`).
- IO-03. Home decodes from `ESC [ H`, `ESC [ 1 ~`, `ESC O H`, and Ctrl-A.
- IO-04. End decodes from `ESC [ F`, `ESC [ 4 ~`, `ESC O F`, and Ctrl-E.
- IO-05. Delete decodes from `ESC [ 3 ~`; Backspace from DEL and Ctrl-H.
- IO-06. Enter (CR) is `SUBMIT`; Ctrl-J (LF) and `ESC CR` (Alt-Enter) are
  `LF`.
- IO-07. Ctrl-C is `CANCEL`; Ctrl-D, Ctrl-U, Ctrl-K, Ctrl-P, Ctrl-N decode
  to their keys; Tab is text.
- IO-08. Bracketed-paste markers `ESC [ 200 ~` and `ESC [ 201 ~` decode to
  `PASTE_BEGIN` and `PASTE_END`.
- IO-09. Multiple events in one read are decoded one at a time, consuming
  exactly the bytes of each event.
- IO-11. Every accepted escape sequence decodes identically when split at
  any byte boundary (the prefix reports incomplete).
- IO-12. A complete sequence in the prefix decodes before later bytes; no
  byte is lost or double-consumed.
