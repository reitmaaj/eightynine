# librepl89 acceptance tests — renderer and cell policy

## 0005 — Unacceptable behavior (must reject / fail safely)

- VT-15. Rendering MUST NOT modify the submission buffer or cursor (R89-I9).
- VT-16. Buffer bytes MUST NOT be emitted as terminal control sequences: only
  printable cluster bytes, spaces for HT, CRLF for line breaks, and the
  editor's own cursor movements may reach the output sink.
- VT-17. A cluster MUST NOT be split by wrapping; wide clusters either fit on
  a row or move whole to the next row.
- VT-18. The renderer MUST NOT read past the submission length.

## 0006 — Required behavior (must exhibit)

- VT-01. Single line: `> abc` with the cursor after `abc`; region rows = 1.
- VT-02. Cursor mid-line is mapped to the correct column.
- VT-03. Empty submission still shows the prompt; cursor after the prompt.
- VT-04. Soft wrapping: `abcdefgh` in five columns renders `> abc` / `defgh`.
- VT-05. An explicit LF starts a new row with the continuation prompt.
- VT-06. The cursor can return to an earlier row (CUU) and column.
- VT-07. Multiline and wrapping compose; continuation prompt only for
  explicit LF, never for a wrap.
- VT-08. CJK base occupies two cells.
- VT-09. Combining sequence occupies one cell.
- VT-10. Emoji presentation occupies two cells.
- VT-11. HT advances to the next configured tab stop.
- VT-12. A trailing LF leaves an empty continuation-prompt row.
- VT-13. Cursor on a wrapped first row maps through the wrap.
- VT-14. A wide prompt shifts text and cursor by two cells.
- VT-19. Region row count and cursor position are reported to the caller.
- VT-20. Cursor-only motion between two regions emits only CUU/CUD/CR/CUF
  escapes; it emits no printable bytes and no erase sequences.
- VT-21. Cursor-only motion between identical positions emits nothing.
- VT-22. Content repaint MUST write replacement bytes before erasing the
  obsolete suffix of a row; whole stale rows MUST be erased only after the
  new content has been written. The old clear-then-draw order is forbidden.
- VT-23. After a repaint, every cell and the cursor position MUST equal a
  fresh full draw of the new submission.
- VT-24. A row filled exactly to the right margin reports `end_col == cols`
  and occupies exactly one visual row; the next atom starts the next row.
- VT-25. The cursor immediately before an atom that wraps MUST be reported on
  the row the atom lands on; the cursor immediately before an LF MUST stay at
  the end of the preceding row.
- VT-26. Emitted physical cursor motion for logical column `cols` MUST target
  column `cols - 1`, and MUST never leave the terminal.

## 0007 — Vertical layout queries (must exhibit)

- VM-01. With soft wrapping, the row above/below the cursor is selected and
  the byte offset closest to the preferred column is returned.
- VM-02. Logical newlines are row boundaries for vertical movement.
- VM-03. A short intervening row clamps the target column while the preferred
  column is preserved for later rows.
- VM-04. Wide clusters are measured in cells, so the chosen offset lands on
  the requested cell column.
- VM-05. Tab stops participate in column computation.
- VM-06. Movement past the first or last row reports `in_range == 0`; the
  caller then falls back to history.
- VM-07. Empty logical lines and a trailing LF (final prompt row) are valid
  vertical targets.
- VM-08. Returned offsets always lie on grapheme boundaries.
