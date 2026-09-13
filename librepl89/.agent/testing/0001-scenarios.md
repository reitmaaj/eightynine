# librepl89 testing scenarios — editor model

## 0001 — Exact insertion
SCENARIO programmatic insertion is exact and atomic
GIVEN a buffer and cursor
WHEN text is inserted programmatically
THEN valid UTF-8 with only LF/HT controls is inserted byte-exactly, while
    CR, other C0/DEL/C1 controls, malformed UTF-8, and allocation failure are
    rejected with the buffer and cursor unchanged.

## 0002 — Grapheme-safe editing
SCENARIO one edit unit is one grapheme cluster
GIVEN a buffer containing combining sequences, CJK, emoji, ZWJ families, and
    flag pairs
WHEN Backspace, Delete, Left, or Right is applied
THEN exactly one extended grapheme cluster is removed or crossed, and the
    cursor remains on a grapheme boundary.

## 0003 — Logical lines
SCENARIO LF separates logical lines
GIVEN a buffer with leading, trailing, and consecutive LF
WHEN LF is inserted, deleted, or Home/End/Ctrl-U/Ctrl-K are applied
THEN the logical-line structure is preserved and only the intended logical
    line is affected.

## 0004 — Buffer invariants
SCENARIO every mutation preserves validity
GIVEN any sequence of accepted and rejected operations
WHEN the buffer is inspected
THEN it contains valid UTF-8 with only LF/HT as controls, and the cursor lies
    on a grapheme boundary.

## 0005 — History storage
SCENARIO history stores whole submissions
GIVEN a history with a configured limit
WHEN entries are added
THEN each successful add appends exactly one byte-identical entry (duplicates
    included), a zero limit retains nothing, and at the limit the oldest entry
    is evicted.

## 0006 — History failure atomicity
SCENARIO a failed add leaves history unchanged
GIVEN an allocation failure or an invalid entry
WHEN history_add is called
THEN it reports the error and the existing entries are unchanged.

## 0007 — History navigation
SCENARIO navigation moves through entries
GIVEN a history with entries and a reset navigation cursor
WHEN navigation moves backward and forward
THEN it returns the newest entry first, then older entries, then the draft
    position.

## 0008 — History independence
SCENARIO histories are per instance
GIVEN two editor instances
WHEN entries are added to one
THEN the other history remains empty and independent.

## 0009 — Cell width policy
SCENARIO grapheme clusters map to terminal cells
GIVEN ASCII, CJK, combining, emoji, and mark/default-ignorable clusters
WHEN the cluster width is computed
THEN wide/fullwidth or emoji-presentation clusters are two cells, marks and
    default-ignorables add none, and everything else is one cell.

## 0010 — Layout and redisplay
SCENARIO the submission is laid out into visual rows
GIVEN a submission, a primary and continuation prompt, a terminal width, and
    a tab width
WHEN the renderer draws
THEN explicit LFs start continuation-prompt rows, wraps start bare rows, tabs
    advance to tab stops, the cursor maps to the right row and column, and the
    buffer is unchanged.

## 0011 — Output safety
SCENARIO untrusted text never becomes a control sequence
GIVEN buffer contents (which the content policy limits to valid UTF-8 plus
    LF/HT)
WHEN the renderer emits output
THEN only printable cluster bytes, spaces, CRLF, and its own cursor movements
    reach the sink.

## 0012 — Key decoding
SCENARIO terminal bytes become semantic keys
GIVEN raw input bytes containing text, C0 controls, CSI/SS3 sequences, and
    bracketed-paste markers
WHEN the decoder runs
THEN each key is reported with the exact bytes consumed, unknown sequences
    are ignored whole, and incomplete sequences consume nothing.

## 0013 — Fragmented input
SCENARIO escape sequences survive arbitrary read boundaries
GIVEN any split of an accepted sequence into two reads
WHEN the first part is decoded
THEN it reports incomplete (or a complete earlier event) and the reassembled
    bytes decode to the same key.

SCENARIO over-long unknown sequences decode identically at any fragmentation
GIVEN an unknown sequence longer than the decoder's fixed cap window
WHEN it is decoded whole or one byte at a time
THEN both decode to the same event sequence: the capped prefix is consumed
    as one ignored unit and the remainder is decoded fresh, never split
    differently because of read boundaries.

## 0014 — Terminal session
SCENARIO raw mode and bracketed paste are managed
GIVEN a pseudoterminal
WHEN the session enters raw mode
THEN ICANON, ECHO, ISIG, and OPOST are disabled, VMIN/VTIME are set, and the
    bracketed-paste enable sequence is emitted.

SCENARIO the terminal is restored
GIVEN an active raw session
WHEN the session leaves
THEN the bracketed-paste disable sequence is emitted and the original termios
    state is restored exactly.

## 0015 — Terminal size
SCENARIO dimensions come from the kernel
GIVEN a pseudoterminal with a set window size
WHEN the size is queried
THEN TIOCGWINSZ rows and columns are reported, and failure is an error rather
    than a guessed default.

## 0016 — Bracketed paste
SCENARIO paste is sanitized out of band
GIVEN pasted bytes containing line breaks, tabs, controls, and Unicode
WHEN they are accumulated
THEN CRLF and CR become LF, HT is preserved, other Cc scalars are discarded,
    malformed UTF-8 rejects the whole paste, and no LF submits.

## 0017 — Fragmented paste
SCENARIO paste survives arbitrary chunk boundaries
GIVEN any two-chunk split of the pasted bytes
WHEN the paste is accumulated
THEN the result is identical to feeding the bytes whole.

## 0018 — Right-edge layout semantics
SCENARIO layout columns are logical positions in 0..cols
GIVEN a submission whose text exactly fills a visual row
WHEN the renderer lays it out
THEN a row filled exactly to the right margin occupies exactly one visual row,
    the next atom starts the following row, the cursor immediately before a
    wrapping atom is placed on the row that atom lands on, and the cursor
    immediately before an LF stays at the end of the preceding row.

SCENARIO the physical cursor stays inside the terminal
GIVEN a logical cursor or end column equal to the terminal width
WHEN the renderer emits cursor motion
THEN the physical target is column `cols - 1`, the last terminal cell.
