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

`JRPC89_ID_NONE` marks a notification (no `id` member). `JRPC89_ID_NULL` is
the JSON literal `null` id.

## Module boundaries

- `id` — id kind helpers and structural comparison.
- `request` — build request/notification object nodes via libj89's builder.
- `response` — validate a parsed response node; extract result, error, id;
  match ids.
- `error` — error code constants, classification, and error-object access.
- `io` — NDJSON framing over an fd.
- `main` — CLI demo that opens a Unix socket and drives the library.

## Dependency

libj89 provides all JSON: `j89_parse`, node accessors, the object/array/
string/integer/bool builders, and `j89_render`. The client stores message
trees in `j89_arena`s the caller owns; it never owns JSON memory itself.

## Transport contract

The library's I/O functions take an already-open `int fd` to a Unix socket.
They add/consume a trailing newline to delimit frames (NDJSON). Socket
creation, connection, and teardown are the caller's (or the CLI demo's)
responsibility.
