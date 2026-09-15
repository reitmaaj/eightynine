# Foundations

## Message shapes

JSON-RPC 2.0 request object:

```json
{ "jsonrpc": "2.0", "method": "...", "params": {...}, "id": <int|string|null> }
```

A notification is a request without an `id` member. A response is exactly
one of:

```json
{ "jsonrpc": "2.0", "result": <any>, "id": <int|string|null> }
{ "jsonrpc": "2.0", "error": { "code": <int>, "message": <string>, "data": <any> }, "id": ... }
```

`id` in a response must match the `id` of the request that prompted it. An
error response for a request whose id could not be parsed carries `id: null`.

`params`, when present, must be an array or object; scalar params are
rejected.

## Data model

`jrpc89_id` discriminates the three legal id forms plus "no id":

```c
typedef enum
{
    JRPC89_ID_NONE,
    JRPC89_ID_INT,
    JRPC89_ID_STRING,
    JRPC89_ID_NULL
} jrpc89_id_kind;

typedef struct
{
    jrpc89_id_kind kind;
    j89_int num;
    const char *str;
    j89_len len;
} jrpc89_id;
```

`JRPC89_ID_NONE` marks a notification (no `id` member) and is legal only for
request construction. A decoded response never carries `NONE`.

`jrpc89_response_decode` produces a checked view:

```c
typedef struct
{
    jrpc89_response_kind kind;
    jrpc89_id id;
    j89_len result;
    jrpc89_error error;
} jrpc89_response;
```

On `JRPC89_OK`, every field permitted by `kind` is usable without further
structural checks; the other fields are zeroed or sentinel (`J89_BAD`).
`*out` is unchanged on any non-OK return.

## Status model

One status namespace covers protocol and transport failures:

`JRPC89_OK`, `JRPC89_EINVAL`, `JRPC89_ENOMEM`, `JRPC89_EPROTO`,
`JRPC89_EOF`, `JRPC89_ETOOLONG`, `JRPC89_ETRUNC`, `JRPC89_EIO`.

libjrpc89 never writes libj89's arena error buffer. Builder failures map to
`EINVAL` when libj89 recorded a diagnostic (invalid input) and to `ENOMEM`
otherwise (allocation), with the arena required to be clean at entry so the
distinction is deterministic.

## Module boundaries

- `id` — id validation, equality, and node construction.
- `request` — build request/notification object nodes via libj89's builder.
- `response` — decode a parsed response into a checked result-or-error view.
- `error` — error code constants and reserved-range classification.
- `io` — NDJSON framing logic, ISO C89, behind an internal syscall seam.
- `io_posix` — the POSIX read/write adapter implementing the seam.
- `tool/jrpc89.c` — CLI demo that drives the library over a provided fd.

## Dependency

libj89 provides all JSON: `j89_parse`, node accessors, the object/array/
string/integer/bool/null builders, and `j89_render`. The protocol core
stores message trees in `j89_arena`s the caller owns; it never owns JSON
memory itself.

## Transport contract

The library's I/O functions take an already-open `int fd` to a blocking,
connected stream socket (AF_UNIX or TCP `SOCK_STREAM`). They add/consume a
trailing newline to delimit frames (NDJSON). Socket creation, connection,
and teardown are the caller's (or the CLI demo's) responsibility.
