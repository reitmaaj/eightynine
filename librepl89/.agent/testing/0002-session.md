# librepl89 testing scenarios — session state machine

## 0001 — Session lifecycle
SCENARIO start, submit, cancel, EOF transitions
GIVEN an inactive editor over a pseudoterminal
WHEN start, feed, submit, cancel, and Ctrl-D are applied
THEN the documented transition occurs, the buffer is preserved or cleared as
    specified, and the terminal is restored on every exit path.

## 0002 — Event semantics
SCENARIO events are requests, not hidden transitions
GIVEN Enter, Ctrl-C, and Ctrl-D input
WHEN feed reports SUBMIT, CANCEL, or EOF
THEN SUBMIT leaves the session active until the caller submits, CANCEL is
    applied by the caller (or by read), and EOF has already ended the session.

## 0003 — Queued input
SCENARIO bytes after an event are preserved
GIVEN one read containing a submission followed by more bytes
WHEN the event is reported
THEN the extra bytes remain queued and feed the next session without another
    read.

SCENARIO bytes after a paste end marker are processed normally
GIVEN one read containing a complete bracketed paste followed by ordinary
    input
WHEN the paste is consumed
THEN the following bytes are decoded as keys in the same feed call.

## 0009 — Queued edits are rendered
SCENARIO queued bytes redraw before any new read
GIVEN a feed call that consumes queued bytes and changes the submission
WHEN no further input is available
THEN the region is redrawn before returning, so a later submit finalizes at
    the correct cursor and newline.

## 0008 — Invalid-state calls are side-effect free
SCENARIO feed while inactive reports ESTATE without a stale event
GIVEN an inactive editor and a caller-supplied event variable holding a
    previous value
WHEN feed is called
THEN it returns ESTATE and sets the event variable to NONE.

## 0004 — Blocking equivalence
SCENARIO read matches the event interface
GIVEN the same input stream
WHEN read is used
THEN it returns SUBMIT with the text, CANCEL, EOF, or ERROR exactly as the
    event loop would.

## 0010 — Vertical movement
SCENARIO arrow keys move over visual rows
GIVEN a submission that wraps and/or contains logical newlines
WHEN Up or Down is pressed
THEN the cursor moves to the closest column of the adjacent visual row, the
    preferred column is preserved across short rows, horizontal movement
    resets it, and leaving the first/last row falls back to history.

## 0005 — Visibility and resize
SCENARIO the application controls layout timing
GIVEN an active session
WHEN resize, hide, and show are called
THEN the submission is unchanged, the region is redrawn or cleared, and
    invalid visibility transitions report ESTATE.

## 0011 — Display updates are classified
SCENARIO cursor motion never repaints content
GIVEN an active session with a drawn submission
WHEN Left, Right, Home, End, or an in-range Up/Down is processed
THEN the terminal output contains only cursor-motion escapes: no printable
    submission bytes and no erase-line or erase-screen sequences.

SCENARIO operations that change nothing emit nothing
GIVEN an active session
WHEN a key leaves the submission and cursor unchanged (Left at the start,
    Delete at the end, Ctrl-P with no history, an ignored sequence)
THEN feed emits no terminal output at all.

SCENARIO content changes still repaint
GIVEN an active session
WHEN insertion, deletion, newline, paste, or history replacement changes the
    submission
THEN the region is redrawn and the cursor is placed at its new position.

SCENARIO a burst collapses to its strongest effect
GIVEN one read containing several decoded keys
WHEN the keys are processed together
THEN the display receives one update of the strongest kind (NONE < CURSOR <
    CONTENT), never one redraw per key.

## 0014 — Output failures are reported, not lost
SCENARIO redisplay failures surface and the terminal is restored
GIVEN an active session whose terminal writes fail
WHEN a cursor update, content repaint, hide clear, finalize, or leave write
    fails
THEN the failure is reported as an I/O error, the session state remains
    recoverable where possible, and termios is restored on every exit path.

SCENARIO an interrupted write is retried
GIVEN a terminal write interrupted by EINTR
WHEN the writer retries
THEN the bytes are delivered and no error is reported.

## 0013 — Resize is a distinct redisplay transition
SCENARIO a resize reconstructs the old region before repainting
GIVEN an active session whose region was drawn at the previous width
WHEN the terminal is resized and resize is called
THEN the old rendering's hard rows are reflowed under the new width, the new
    submission is laid out at the new width, the repaint erases only rows left
    stale by the change, and the cursor ends at the new layout's position.

SCENARIO growing never merges old hard rows
GIVEN a rendering whose explicit CRLF wraps created several hard rows
WHEN the terminal grows
THEN each old hard row still occupies at least one physical row during
    reconstruction, even though the new layout may recombine them.

SCENARIO an unchanged size is a no-op
GIVEN an active session
WHEN resize is called and TIOCGWINSZ reports the same dimensions
THEN no terminal output is produced.

## 0012 — Content repaint overwrites before erasing
SCENARIO replacement text is written over the old region first
GIVEN a drawn region and a content change
WHEN the region is repainted
THEN each new row is written before its obsolete suffix is erased, whole stale
    rows are erased only after the new content is complete, and the terminal
    never shows an intentionally blank editor frame.

SCENARIO repaint leaves the same final display as a full draw
GIVEN any old and new submission and geometry
WHEN the new submission is repainted over the old region
THEN the resulting cells and cursor position equal those of a fresh full draw.
