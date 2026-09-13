# librepl89 design

## Green source profile

Sources are strict C89 AND strict C23 clean under GCC and Clang, pass the
`green-*` semantic checks, and use canonical Allman formatting. Algorithms are
worker/controller: computation at a function's top level, thin nested blocks
that delegate. No function-like macros; no owned type name ends in `_t`.
`just check` gates this.

## Module map

| Module | File(s) | Responsibility |
|---|---|---|
| allocation | `src/repl89_mem.c` | fault-injectable malloc/realloc/free hooks |
| buffer | `src/repl89_buf.c` | growable byte buffer, atomic reserve/insert/delete |
| editor model | `src/repl89_edit.c` | exact insertion, content policy, grapheme editing, logical lines |
| history | `src/repl89_hist.c` | whole-submission history, eviction, traversal |
| keys | `src/repl89_key.c` | byte-stream decoder (escape sequences, paste) |
| render | `src/repl89_render.c` | cluster widths, tab stops, wrapping, cursor mapping |
| tty | `src/repl89_tty.c` | termios, `TIOCGWINSZ`, read/write, bracketed paste |
| public | `src/repl89.c` | object lifetime and the start/feed/submit/cancel state machine |

The editor model compiles without the tty layer so tests exercise it directly.

## Content policy

The buffer always contains valid UTF-8 whose only Cc scalars are LF and HT.
`repl89_insert` is exact and atomic: malformed UTF-8 is `REPL89_EUTF8`,
a forbidden control is `REPL89_EINVAL`, and on failure the buffer and cursor
are unchanged. Bracketed paste is the sanitizing boundary (CR/CRLF -> LF,
forbidden controls discarded).

## Width policy

Unicode facts come from libu89 (`u89_east_asian_width`, `u89_is_mark`,
`u89_is_control`, `u89_is_emoji`, `u89_is_emoji_presentation`,
`u89_default_ignorable`, `u89_grapheme_*`). librepl89 maps a cluster to cells
behind one internal function: a cluster containing a wide/fullwidth base is
two cells; an emoji-presentation cluster is two cells; marks and
default-ignorables add none; anything else is one cell. HT advances to the
next configured tab stop, which is column-dependent and therefore handled by
the layout function rather than the cluster-width function.

## Display updates

Every input operation reports a display effect: `NONE`, `CURSOR`, `CONTENT`,
or `RESIZE` (ordered by dominance). `feed` accumulates the strongest effect
across a drained batch and applies it once through `session_update`: cursor
motion emits cursor escapes only, content repaints by overwriting the old
region before erasing stale rows, and resize relayouts. A single layout
function (`repl89_layout`) computes the region without emitting, and the
renderer walk serves both full draws and repaints.

## Layout and the right edge

Layout columns are logical positions in `0..cols` inclusive. An atom is placed
on the current row when `col + width <= cols`; only an atom that would exceed
the margin starts a new row. A row filled exactly to the margin ends at
logical column `cols` with no phantom row, and a following atom begins the
next row. The cursor is a boundary: it is recorded after the following atom's
wrap decision, so a cursor immediately before a wrapping atom belongs to the
row that atom lands on, while a cursor before an LF stays at the end of the
preceding row. Emitted physical cursor motion maps logical `cols` to the last
terminal cell (`cols - 1`).

## State machine

```text
inactive --start--> active (visible or hidden)
active --submit--> inactive, buffer preserved
active --cancel--> inactive, buffer cleared
active --EOF----> inactive, buffer empty
```

`feed` performs at most one `read(2)` and never changes the blocking mode;
`EINTR`/`EAGAIN` report `NONE` so the application keeps control of signals and
polling. Bytes read after a semantic event stay queued for the next session.
`repl89_read` is a blocking convenience over the same machine and returns a
`repl89_result` so cancel and EOF are control-flow outcomes, not errors.

## Vertical movement

Up/Down move over visual rows (soft wraps included), targeting the grapheme
boundary closest to a preferred visual column; the preferred column is
captured when vertical movement starts and preserved across short rows until
any non-vertical operation. Leaving the first or last visual row falls back
to history (Up) or history/draft restore (Down). Ctrl-P/Ctrl-N are
history-only. Layout queries live in the renderer (`repl89_layout_vertical`)
and are allocation-free.

## Resize

Resize is a separate display transition. `repl89_resize` queries
`TIOCGWINSZ`, returns immediately when the dimensions are unchanged, and
otherwise repaints through `repl89_render_resize`. Because a terminal reflows
already displayed lines when its width changes, the old region's recorded
cursor row no longer describes the physical screen; `repl89_layout_reflow`
first reconstructs the old rendering under the new width (each old hard row
reflows independently and old hard rows never merge) and the ordinary
overwrite repaint then removes any rows left stale. This assumes an
xterm-like terminal that preserves explicit CRLF boundaries while reflowing
cells within a hard row; exact reconstruction on terminals without reflow is
out of scope. Signal policy stays with the application: a client installs its
own `SIGWINCH` handler and calls `repl89_resize` from its event loop. The
blocking `repl89_read` convenience therefore handles a resize only on the
next interaction; the event example demonstrates the event-driven pattern.

## v0 scope notes

`repl89_read` loops on `NONE`, so a blocking `read` defers resize handling to
the event API, as documented. Fault injection is available through the
allocation hooks (`repl89_mem_set_hooks`) and the tty syscall hooks
(`repl89_tty_set_hooks`). `just lint-split` is a narrow architectural
tripwire for R89-I10, not a correctness proof.

The renderer uses the terminal width only. A region taller than the terminal
scrolls the physical screen, after which "move up `cursor_row` rows to reach
the region top" no longer holds; a viewport is deliberately out of v0 scope,
and `repl89_tty` records the row count without using it.
