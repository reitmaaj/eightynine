# 0006 — JSON strings and keys owned by libstr89

## Representation

Every `J89_STRING` node and every object key in a `j89_arena` is now an owned
`str89` value rather than a byte range in the arena block.

- A string node stores a **registry offset** in `base`; `n` remains the byte
  length.
- An object member stores a registry offset in `ko`; `kl` remains the key byte
  length.
- The registry is an intrusive singly linked list of

  ```c
  struct j89_strnode { j89_len next; str89 s; };
  ```

  allocated inside the arena block. Its head is the new `j89_arena.strhead`
  field (`J89_BAD` when empty). `j89_arena_destroy` walks the registry,
  `str89_free`s each value, then frees the block.

`j89_arena` is a public struct, so adding `strhead` changes `sizeof` but no
existing field offset or public symbol. Consumers recompile; no API or
semantic break.

## NUL termination

`j89.h` promises that string bytes and keys are NUL-terminated immediately
after their reported length (and `llm89` relies on it). `str89` itself makes no
terminator guarantee, so the arena adds the terminator itself:

1. reserve `len + 1` through `str89_buf`;
2. append the validated bytes;
3. write `data[len] = '\0'` (within the reserved capacity);
4. `str89_take` transfers the block into an owned `str89`;
5. register the `str89` in the arena.

The parser follows the same pattern directly from its builder, so a parsed
string needs no second copy.

## Parser and builder

- `j89_parse_string` builds into a `str89_buf`; ordinary bytes are appended
  with `str89_buf_append`, `\uXXXX` scalars with `str89_buf_append_cp`. The
  buffer is taken and registered on success; any failure frees the buffer (and
  the taken string) and records the error.
- `j89_string_new` and `j89_object_set` validate through `str89_view_init`
  (replacing `u89_utf8_valid`) before allocating. `j89_add_string` builds and
  registers the owned value.
- Accessors (`j89_string_value`, `j89_string_length`, `j89_object_key`,
  `j89_object_key_length`) resolve the registry entry and return the `str89`
  data pointer and length, preserving the embedded-NUL and terminator
  contracts.

## Dependency

```text
libu89  (Unicode facts)
   ^
libstr89 (validated UTF-8 storage)
   ^
libj89  (JSON syntax and representation)
   ^
libjrpc89 / llm89
```

`j89.h` does not include `str89.h`; the dependency is internal. `j89_alg`
remains an uninterpreted byte-string carrier and has no `str89` or `u89`
dependency.

## Failure behavior

Allocation failure at any step marks the arena failed and returns `J89_BAD`;
any string already registered stays owned by the arena and is released by
`j89_arena_destroy`. A string whose registration failed is freed by the
caller. The `j89_failed()` / `j89_error()` contract is unchanged.
