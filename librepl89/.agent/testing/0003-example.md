# librepl89 testing scenarios — echo example

## 0001 — Example echoes a submission
SCENARIO a submitted line is echoed byte-for-byte
GIVEN the echo example running on a pseudoterminal
WHEN a submission is typed and Enter is pressed
THEN the example writes the exact submission bytes followed by one LF and
    prompts again with `> `.

## 0002 — Multiline and UTF-8 survive the round trip
SCENARIO Ctrl-J and non-ASCII content are echoed exactly
GIVEN the echo example running on a pseudoterminal
WHEN a submission contains an embedded LF (Ctrl-J) and UTF-8 scalars
THEN the echoed bytes contain the same scalars and LF, unmodified.

## 0003 — Cancel discards the submission
SCENARIO Ctrl-C ends a submission without echoing it
GIVEN the echo example running on a pseudoterminal with text typed
WHEN Ctrl-C is pressed
THEN the example does not emit the typed text as a completed echo line and
    prompts again.

## 0004 — EOF ends the example
SCENARIO Ctrl-D on an empty submission exits
GIVEN the echo example running on a pseudoterminal
WHEN Ctrl-D is pressed with an empty submission
THEN the example exits with status 0 and the terminal is restored.

## 0005 — Submitted lines enter history
SCENARIO Up recalls the previous submission
GIVEN the echo example after one submission
WHEN Up is pressed in the next submission
THEN the previous submission reappears in the editor and can be submitted
    again.

## 0006 — The event example resizes immediately
SCENARIO SIGWINCH interrupts the event loop
GIVEN the event-driven echo example blocked in feed
WHEN the terminal size changes and SIGWINCH is delivered
THEN the example calls resize, redraws the region at the new width, and
    continues reading input normally.

## 0007 — The event example restores before output
SCENARIO echoed output is written outside raw mode
GIVEN the event-driven example receives a submission or cancel event
WHEN it writes the echo or cancel marker
THEN it has already ended the session, so the terminal is restored and the
    output is newline-translated like ordinary application output.
