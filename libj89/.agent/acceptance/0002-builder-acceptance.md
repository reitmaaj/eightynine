# 0002-builder-acceptance

## Must exhibit

ACCEPT: a caller can construct an object, an array, string/integer/double
values, and assign them by slot in a single arena.
ACCEPT: a constructed tree renders via `j89_render` to valid JSON text whose
logical structure round-trips through `j89_parse`.
ACCEPT: strings containing `"`, `\`, newline, tab, return, backspace, form-feed,
and other control bytes render with correct JSON escapes.
ACCEPT: an integer value renders as an integer token and re-parses to an
INTEGER node; a double value re-parses to a FLOAT node with the same value.
ACCEPT: rendered output is NUL-terminated so it can be consumed as a C string.
ACCEPT: the builder shares the existing arena and needs no separate cleanup;
`j89_arena_destroy` releases everything.

## Must reject (unacceptable behavior)

REJECT: rendering or setting a value without first building a valid tree
(no use-after-free, no crash, no out-of-bounds arena access).
REJECT: leaving declared object/array slots unfilled and then rendering an
uninitialized slot; every slot must be assigned before render.
REJECT: a builder call that cannot allocate corrupting the arena silently;
it must mark the arena failed so `j89_failed`/`j89_error` report it.
REJECT: emitting non-JSON or unparsable text for any successfully built tree.
