# libu89 stories

## 0001 — The strict decoder author
AS a developer of a decoder (wasm names, JSON keys, protocol fields),
I WANT a C89 function that validates a UTF-8 byte range and rejects
overlong, surrogate, out-of-range, and truncated sequences
SO THAT I can stop hand-rolling a validator and guarantee structural
correctness with no external dependency.

## 0002 — The language front-end author
AS a developer of a language front-end,
I WANT `XID_Start`/`XID_Continue` predicates,
SO THAT Unicode identifiers (UAX #31) are accepted and non-identifiers rejected
consistently.

## 0003 — The terminal/UI author
AS a developer of a terminal or text UI,
I WANT per-scalar terminal display width and a word-wrap primitive that breaks
at Unicode whitespace,
SO THAT wide, zero-width, combining, and control scalars are laid out correctly
and lines never exceed an available width or split a grapheme cluster.

## 0004 — The editor author
AS a developer of a Unicode-aware editor,
I WANT NFC/NFD/NFKC/NFKD normalization and byte/codepoint/UTF-16 offset mapping,
SO THAT canonical-equivalence matching and cursor mapping behave predictably
without depending on ICU.

## 0005 — The byte-oriented consumer
AS a developer working with NUL-containing or explicitly-length-bounded text,
I WANT a length-based API (explicit lengths, not NUL-termination),
SO THAT embedded U+0000 and bounded buffers are handled safely.

## 0006 — The reviewer
AS a reviewer enforcing the green source profile,
I WANT the library to compile warning-clean as strict C89 AND strict C23 under
both GCC and Clang, pass the green semantic checks, and use one canonical
Allman format,
SO THAT every unit is auditable and gated by the sibling `green` toolchain
(`just check`), replacing the retired ob89 / c89-baseline gates.

## 0007 — The interactive editor author
AS the author of a multiline terminal editor (librepl89),
I WANT positional UTF-8 decode/prev, extended grapheme-cluster boundaries,
and the terminal-relevant scalar properties (East Asian Width, emoji and
emoji-presentation, marks, controls, default-ignorable),
SO THAT cursor motion, deletion, wrapping, and cell widths follow Unicode
instead of being reimplemented in the editor.
