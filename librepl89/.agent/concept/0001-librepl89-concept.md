# librepl89 concept

## Purpose

`librepl89` is a small C89 interactive editor for one arbitrary multiline
UTF-8 submission on a modern POSIX terminal. It is the input layer a REPL or
agent loop needs: grapheme-safe cursor editing, logical newlines, visual
wrapping, whole-submission history, bracketed paste, resize, and temporary
hiding for interleaved output.

## Core abstraction

```text
editable submission
    = UTF-8 text containing LF and HT
    + cursor
    + history
    + terminal presentation
```

## Unicode boundary

Unicode facts belong to `libu89`: UTF-8 decode/validate, extended grapheme
cluster boundaries, East Asian Width, emoji and emoji-presentation, mark and
control classification, default-ignorable. `librepl89` maps grapheme clusters
to terminal cells and owns terminal policy (tab stops, wrapping, prompts,
history). No Unicode tables or segmentation algorithms appear in this
repository.

## Model

One editor object owns:

- a growable byte buffer containing valid UTF-8 with LF/HT as the only
  controls;
- a cursor that always lies on a grapheme boundary;
- an in-memory history of complete submissions;
- a tty session (raw mode, bracketed paste) and a renderer.

Explicit state machine: `start` -> active; `feed` reports events; `submit`
accepts and ends the session (buffer preserved); `cancel` discards and ends;
`EOF` ends with an empty buffer. `repl89_read` is a blocking convenience over
the same machine.

## Non-goals

Parsing, expression completeness, syntax highlighting, completion, hints,
brace matching, automatic indentation, persistent history, configurable key
maps, vi mode, mouse support, Windows console, terminfo/termcap, legacy
terminal compatibility, generic redirected-stdin processing, and Unicode word
segmentation (deferred until a second client needs it).
