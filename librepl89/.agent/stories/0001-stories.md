# librepl89 stories

## 0001 — The REPL author
AS the author of a language REPL,
I WANT an interactive editor that returns one complete multiline submission
with grapheme-safe cursor editing,
SO THAT users can type expressions across lines without the REPL owning
terminal handling.

## 0002 — The agent-loop author
AS the author of an agent or tool loop,
I WANT an event-driven interface plus hide/show,
SO THAT streaming model output and tool traces can interleave with
interactive input without corrupting the editor display.

## 0003 — The Unicode user
AS a user typing CJK, combining marks, emoji, and flags,
I WANT backspace, delete, and cursor motion to treat each perceived character
as one unit,
SO THAT editing never splits a UTF-8 sequence or a grapheme cluster.

## 0004 — The terminal owner
AS a process that must leave the terminal usable,
I WANT raw mode and bracketed paste restored on submit, cancel, EOF, error,
and destruction,
SO THAT the shell never inherits a broken terminal.

## 0005 — The reviewer
AS a reviewer enforcing the Unicode split,
I WANT librepl89 to contain no Unicode tables or segmentation algorithms,
SO THAT all Unicode semantics stay conformance-tested in libu89.

## 0006 — The library evaluator
AS a developer evaluating librepl89,
I WANT a small runnable echo REPL,
SO THAT I can see the submit/cancel/EOF contract in action before integrating
the library.
