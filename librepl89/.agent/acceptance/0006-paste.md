# librepl89 acceptance tests — bracketed paste

## 0011 — Unacceptable behavior (must reject / fail safely)

- PS-12. Malformed UTF-8 anywhere in a paste MUST reject the entire paste
  (`REPL89_EUTF8`); no partial content may reach the buffer.
- PS-10. A pasted escape sequence MUST NOT become terminal control output:
  raw ESC is discarded and the remaining bytes are inert text.
- PS-16. A pasted LF MUST NOT submit.
- PS-18. C0 controls, DEL, and C1 controls MUST be discarded, never inserted.
- PS-19. A truncated sequence at the end of a paste MUST be rejected.

## 0012 — Required behavior (must exhibit)

- PS-01. Plain text is preserved byte-for-byte.
- PS-02. Embedded LF is preserved and does not submit.
- PS-03. CRLF becomes LF.
- PS-04. Lone CR becomes LF.
- PS-05. Mixed CR/LF input normalizes every line break to LF.
- PS-06. HT is preserved.
- PS-07/PS-08/PS-09. C0, DEL, and C1 controls are discarded.
- PS-11. Valid UTF-8, including combining sequences and emoji, is preserved.
- PS-13/PS-14. Feeding the paste in any two chunks (including inside a
  multi-byte scalar or a CRLF pair) produces identical results.
- PS-15. Paste accumulation is separate from the edit buffer; the caller
  inserts the finished paste as one operation.
- PS-17. Very long paste content is preserved.
