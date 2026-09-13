# librepl89 acceptance tests — pure editor model

## 0001 — Unacceptable behavior (must reject / fail safely)

- IN-05. Programmatic insertion of CR MUST be rejected with `REPL89_EINVAL`;
  the buffer and cursor MUST be unchanged.
- IN-06. Programmatic insertion of any other C0 control MUST be rejected with
  `REPL89_EINVAL`; buffer and cursor unchanged.
- IN-07. Programmatic insertion of DEL (U+007F) MUST be rejected.
- IN-08. Programmatic insertion of a Unicode Cc scalar other than LF/HT
  (including C1, e.g. U+0085) MUST be rejected.
- IN-09. Malformed UTF-8 MUST be rejected with `REPL89_EUTF8` atomically.
- IN-10. An allocation failure MUST return `REPL89_ENOMEM` with buffer and
  cursor unchanged.
- ED-09/ED-10. Backspace/Left at offset 0 and Delete/Right at end of buffer
  MUST NOT mutate the buffer or move the cursor.
- ML-11. The buffer MUST never contain a lone UTF-8 continuation byte or a
  split grapheme cluster after any operation.

## 0002 — Exact programmatic insertion (must exhibit)

- IN-01. Printable ASCII is inserted byte-exactly.
- IN-02. Valid UTF-8 is inserted byte-exactly.
- IN-03. LF is inserted exactly.
- IN-04. HT is inserted exactly.
- IN-11. Insertion at a mid-buffer cursor inserts exactly at the cursor and
  leaves the cursor after the inserted text.

## 0003 — Grapheme editing (must exhibit)

Each Backspace/Delete/Left/Right operates on one extended grapheme cluster.

- ED-01. `abc|` Backspace -> `ab|`.
- ED-02. `a|bc` Delete -> `a|c`.
- ED-03. `e◌́|` Backspace -> empty.
- ED-04. `|e◌́` Delete -> empty.
- ED-05. `👨‍👩‍👧|x` Left -> cursor before the whole family cluster.
- ED-06. `|👨‍👩‍👧x` Right -> cursor after the whole family cluster.
- ED-07. `🇫🇮|` Backspace -> empty.
- ED-08. CJK + ASCII: Left/Right move by grapheme, not byte.
- ED-11. After every edit the cursor lies on a grapheme boundary.

## 0004 — Multiline editing (must exhibit)

- ML-01. Inserting LF between `a` and `b` yields `a\nb`.
- ML-02. Backspace immediately after LF joins the logical lines.
- ML-03. Delete immediately before LF joins the logical lines.
- ML-04. Home moves to just after the preceding LF (or 0).
- ML-05. End moves to the next LF (or `n`).
- ML-06. Consecutive LF preserve empty logical lines.
- ML-07. A leading LF is preserved and navigable.
- ML-08. A trailing LF is preserved and navigable.
- ML-12. Ctrl-U deletes to the logical-line beginning.
- ML-13. Ctrl-K deletes to the logical-line end.
