# llm89 — Minimal C89 LLM Client Library

Status: V0 implementation specification  
Language: ISO C90 / C89  
Primary dependency: libcurl  
JSON dependency: cJSON  
Protocol: OpenAI-compatible Chat Completions over HTTP(S), including SSE streaming

## 1. Purpose

`llm89` is a small C library for issuing text-chat requests to an OpenAI-compatible LLM HTTP endpoint and receiving either a complete response or an incremental text stream.

It is a protocol client only. It contains no agent loop, tool executor, conversation store, retry policy, prompt framework, CLI, scheduler, model registry, or provider discovery.

The design target is:

```text
application
    |
    | messages / generation parameters
    v
llm89
    |
    | JSON + SSE
    v
libcurl
    |
    v
HTTP(S)
```

The V0 public API shall compile as strict C89 and expose no libcurl or cJSON types.

## 2. Scope

V0 shall support:

- synchronous blocking requests;
- OpenAI-compatible `POST .../chat/completions` semantics;
- Bearer-token authentication, optionally omitted;
- system, user, and assistant text messages;
- model selection;
- optional temperature and maximum-output-token parameters;
- non-streaming text completion;
- SSE streaming text completion;
- incremental cancellation;
- connect and total-request timeouts;
- caller-supplied additional HTTP headers;
- access to the raw response JSON;
- access to every raw SSE JSON event;
- deterministic ownership and cleanup;
- secure libcurl TLS verification defaults.

V0 shall not directly model:

- tools/function calls;
- multimodal content;
- audio;
- embeddings;
- image generation;
- structured-output schemas;
- log probabilities;
- multiple choices;
- provider-specific request fields;
- asynchronous/event-loop integration;
- automatic retries;
- rate-limit handling beyond returning the HTTP error;
- conversation history management.

The raw JSON interfaces described below provide an escape hatch for protocol features not represented by the typed V0 API.

## 3. Design principles

### 3.1 One protocol, not a false universal schema

V0 targets the OpenAI-compatible Chat Completions protocol. Provider-specific APIs require separate adapters rather than conditional fields in one nominally universal request structure.

### 3.2 libcurl owns transport semantics

`llm89` shall not implement sockets, TLS, proxy handling, HTTP framing, connection reuse, DNS, or HTTP version negotiation.

### 3.3 Streaming means byte-stream parsing

A libcurl write callback may receive arbitrary byte boundaries. No callback invocation may be assumed to contain one SSE line, one SSE event, one UTF-8 code point, or one JSON object.

### 3.4 C API owns no caller inputs

Configuration and request strings remain caller-owned for the duration of the call. Returned complete responses are library-owned allocations transferred to the caller and released with `llm_response_free()`.

Streaming event pointers remain valid only for the duration of the event callback.

### 3.5 No hidden retry semantics

One public call causes at most one HTTP request. Network errors, HTTP errors, malformed JSON, malformed SSE, and protocol errors return directly to the caller.

## 4. Dependencies

### 4.1 libcurl

Required minimum: libcurl 7.32.0.

The implementation shall use the easy interface for V0. Relevant facilities include:

- `curl_global_init` / `curl_global_cleanup`;
- `curl_easy_init` / `curl_easy_cleanup`;
- `CURLOPT_URL`;
- `CURLOPT_POST`;
- `CURLOPT_POSTFIELDS`;
- `CURLOPT_POSTFIELDSIZE_LARGE`;
- `CURLOPT_HTTPHEADER`;
- `CURLOPT_WRITEFUNCTION`;
- `CURLOPT_WRITEDATA`;
- `CURLOPT_XFERINFOFUNCTION`;
- `CURLOPT_XFERINFODATA`;
- `CURLOPT_NOPROGRESS`;
- `CURLOPT_CONNECTTIMEOUT_MS`;
- `CURLOPT_TIMEOUT_MS`;
- `CURLOPT_ERRORBUFFER`;
- `CURLINFO_RESPONSE_CODE`.

The implementation shall not disable peer or host certificate verification.

### 4.2 cJSON

V0 shall use cJSON for construction and parsing of JSON. cJSON is written for ANSI C/C89 and can itself be built under strict C89 compiler settings.

The public API shall expose no cJSON objects. A future implementation may replace cJSON without changing the public ABI.

## 5. Files

A minimal source tree is:

```text
include/
    llm89.h
src/
    llm89.c
    llm89_sse.c
    llm89_sse.h
    llm89_buf.c
    llm89_buf.h
vendor/                 # optional vendoring
    cJSON.c
    cJSON.h
tests/
    test_sse.c
    test_json.c
    test_protocol.c
```

`llm89.c` contains the public API and libcurl integration. `llm89_sse.c` and `llm89_buf.c` remain private implementation units.

## 6. Public API

The complete V0 public header shall conceptually expose the following API. Exact comments may expand, but semantics shall not change.

```c
#ifndef LLM89_H
#define LLM89_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LLM89_VERSION_MAJOR 0
#define LLM89_VERSION_MINOR 1
#define LLM89_VERSION_PATCH 0

#define LLM_OK            0
#define LLM_EINVAL        1
#define LLM_ENOMEM        2
#define LLM_ECURL         3
#define LLM_EHTTP         4
#define LLM_EJSON         5
#define LLM_ESSE          6
#define LLM_EPROTO        7
#define LLM_ECANCELLED    8
#define LLM_EOVERFLOW     9

#define LLM_ROLE_SYSTEM       1
#define LLM_ROLE_USER         2
#define LLM_ROLE_ASSISTANT    3

#define LLM_EVENT_DATA        1
#define LLM_EVENT_DONE        2

typedef struct llm_client llm_client;

typedef struct llm_error {
    int code;
    long http_status;
    long transport_code;
    char message[256];
} llm_error;

typedef struct llm_header {
    const char *name;
    const char *value;
} llm_header;

typedef struct llm_client_config {
    const char *endpoint;
    const char *api_key;
    const llm_header *headers;
    size_t header_count;
    long connect_timeout_ms;
    long timeout_ms;
} llm_client_config;

typedef struct llm_message {
    int role;
    const char *content;
} llm_message;

typedef struct llm_chat_request {
    const char *model;
    const llm_message *messages;
    size_t message_count;

    int set_temperature;
    double temperature;

    int set_max_tokens;
    long max_tokens;
} llm_chat_request;

typedef struct llm_response {
    char *text;
    char *json;
    long http_status;
} llm_response;

typedef struct llm_stream_event {
    int type;

    const char *text;
    size_t text_len;

    const char *json;
    size_t json_len;
} llm_stream_event;

typedef int (*llm_stream_fn)(
    void *userdata,
    const llm_stream_event *event);

typedef int (*llm_cancel_fn)(void *userdata);

int llm_global_init(llm_error *error);
void llm_global_cleanup(void);

llm_client *llm_client_new(
    const llm_client_config *config,
    llm_error *error);

void llm_client_free(llm_client *client);

int llm_chat(
    llm_client *client,
    const llm_chat_request *request,
    llm_response *response,
    llm_cancel_fn cancel,
    void *cancel_userdata,
    llm_error *error);

int llm_chat_stream(
    llm_client *client,
    const llm_chat_request *request,
    llm_stream_fn stream,
    void *stream_userdata,
    llm_cancel_fn cancel,
    void *cancel_userdata,
    llm_error *error);

int llm_json(
    llm_client *client,
    const char *request_json,
    llm_response *response,
    llm_cancel_fn cancel,
    void *cancel_userdata,
    llm_error *error);

int llm_json_stream(
    llm_client *client,
    const char *request_json,
    llm_stream_fn stream,
    void *stream_userdata,
    llm_cancel_fn cancel,
    void *cancel_userdata,
    llm_error *error);

void llm_response_free(llm_response *response);

#ifdef __cplusplus
}
#endif

#endif
```

## 7. Global lifecycle

### 7.1 `llm_global_init`

`llm_global_init()` shall initialize libcurl global state.

It shall be called exactly once by the process before any `llm_client_new()` call, unless the embedding application already owns libcurl global initialization and the implementation provides the compile-time option described below.

Success returns `LLM_OK`.

### 7.2 `llm_global_cleanup`

`llm_global_cleanup()` shall release library-global libcurl state after all clients and transfers have ended.

### 7.3 Optional externally-owned curl lifecycle

A build-time definition:

```text
LLM89_EXTERNAL_CURL_GLOBALS
```

may suppress calls to `curl_global_init()` and `curl_global_cleanup()`. This supports applications that already own libcurl process lifecycle.

No other mutable process-global state is permitted.

## 8. Client configuration

`endpoint` contains the complete HTTP URL for the chat-completions endpoint, for example:

```text
https://api.example.com/v1/chat/completions
```

The library shall not join base URLs and paths.

If `api_key != NULL`, the client shall send:

```text
Authorization: Bearer <api_key>
```

If `api_key == NULL`, no authorization header shall be synthesized.

Every request shall send:

```text
Content-Type: application/json
```

Streaming requests shall additionally send:

```text
Accept: text/event-stream
```

Non-streaming requests shall send:

```text
Accept: application/json
```

Caller-supplied headers are appended after standard headers. Duplicate security-sensitive headers shall be rejected:

- `Authorization`;
- `Content-Type`.

The caller may supply other provider-required headers.

`connect_timeout_ms <= 0` selects the implementation default of 10,000 ms.

`timeout_ms == 0` disables a total transfer deadline. A positive value sets the libcurl total timeout. A negative value is invalid.

No API shall expose an option to disable TLS certificate or hostname verification.

## 9. Typed chat requests

### 9.1 Roles

The V0 role mapping is exact:

```text
LLM_ROLE_SYSTEM     -> "system"
LLM_ROLE_USER       -> "user"
LLM_ROLE_ASSISTANT  -> "assistant"
```

Any other role value returns `LLM_EINVAL` before network activity.

### 9.2 Required fields

A typed request requires:

- non-NULL `model`;
- non-empty model string;
- non-NULL `messages` when `message_count > 0`;
- at least one message;
- non-NULL message content.

V0 sends text content only.

### 9.3 JSON encoding

A non-stream request generates an object equivalent to:

```json
{
  "model": "MODEL",
  "messages": [
    {"role": "system", "content": "..."},
    {"role": "user", "content": "..."}
  ],
  "stream": false
}
```

A streaming request generates the same object with:

```json
"stream": true
```

If `set_temperature != 0`, the library emits:

```json
"temperature": <temperature>
```

If `set_max_tokens != 0`, the library emits:

```json
"max_tokens": <max_tokens>
```

The library shall reject negative `max_tokens`.

The library shall reject a non-finite `temperature` if the host C implementation can detect it without relying on non-C89 public interfaces. Otherwise cJSON serialization failure shall produce `LLM_EJSON`.

No V0 validation shall impose a provider-specific range on temperature.

## 10. Raw JSON requests

`llm_json()` and `llm_json_stream()` send caller-provided JSON bodies unchanged.

They exist for protocol features outside the typed V0 schema, including tools, structured output, provider-specific fields, multimodal message objects, and future fields.

The library shall validate that `request_json` parses as one top-level JSON object before network activity.

For `llm_json_stream()`, the caller is responsible for including protocol fields required to request streaming, including:

```json
"stream": true
```

For `llm_json()`, the caller controls the body entirely.

The raw functions still provide normal authentication, standard HTTP headers, timeout behavior, HTTP error handling, cancellation, response buffering, and SSE decoding.

## 11. Non-streaming response semantics

`llm_chat()` and `llm_json()` perform one synchronous HTTP request.

On successful HTTP status and syntactically valid JSON, the implementation stores the full response body in `response->json`.

`llm_chat()` additionally extracts:

```text
choices[0].message.content
```

as a newly allocated NUL-terminated UTF-8 string in `response->text`.

The following conditions produce `LLM_EPROTO`:

- no `choices` array;
- empty `choices` array;
- no first-choice `message` object;
- no string-valued `message.content`.

`llm_json()` does not interpret the response schema. It sets `response->text` to `NULL`.

On entry, successful or failed functions shall leave `response` either fully initialized or zeroed. `llm_response_free()` shall accept a zeroed response.

## 12. Streaming response semantics

`llm_chat_stream()` and `llm_json_stream()` perform one synchronous HTTP request while invoking `stream()` incrementally from the libcurl write path.

The callback runs on the thread executing the public call.

The implementation shall never call the user callback concurrently.

### 12.1 Event types

For each complete SSE `data:` event, the library emits one `LLM_EVENT_DATA` callback.

For `llm_chat_stream()`, the event fields mean:

- `json/json_len`: complete decoded SSE data payload as received, excluding SSE framing;
- `text/text_len`: `choices[0].delta.content` when present and string-valued;
- otherwise `text == NULL` and `text_len == 0`.

This permits callers to consume ordinary text while retaining access to tool-call deltas and other protocol fields without V0 modeling them.

For `llm_json_stream()`, the library shall not interpret the event object:

- `json/json_len` contains the event payload;
- `text == NULL`;
- `text_len == 0`.

When the SSE data payload equals exactly:

```text
[DONE]
```

`llm_chat_stream()` and `llm_json_stream()` emit one `LLM_EVENT_DONE` event and do not parse `[DONE]` as JSON.

A successful streaming request shall contain exactly one terminal `[DONE]` event. End-of-body before `[DONE]` returns `LLM_EPROTO`.

### 12.2 Callback lifetime

All pointers inside `llm_stream_event` remain valid only until the callback returns.

A caller that wants to retain text or JSON shall copy it before returning.

### 12.3 Callback cancellation

If `stream()` returns nonzero, the transfer shall abort and the public function shall return `LLM_ECANCELLED`.

This is intentional cancellation, not `LLM_ECURL`, even though libcurl reports the aborted write internally.

## 13. SSE decoder

The SSE decoder shall be a small independent state machine with no libcurl dependency.

### 13.1 Input

```text
sse_feed(parser, bytes, length)
```

may receive arbitrary byte partitions, including:

- partial lines;
- multiple lines;
- partial UTF-8 sequences;
- several events;
- an event boundary split across callbacks.

The decoder treats data as bytes. UTF-8 validation belongs to the JSON parser after a complete `data:` payload is assembled.

### 13.2 Line endings

The parser shall accept:

- LF;
- CRLF;
- CR.

### 13.3 Fields

Only the `data` field affects LLM event delivery.

The parser shall ignore:

- comments beginning with `:`;
- `event`;
- `id`;
- `retry`;
- unknown fields.

For a line beginning with:

```text
data:
```

one optional ASCII space immediately after `:` shall be removed.

Multiple `data:` lines in one SSE event shall be concatenated with one `\n` byte between values, as required by SSE semantics.

A blank line terminates the current event.

An event with no `data:` lines shall not trigger an LLM callback.

### 13.4 Resource limits

The client shall impose configurable-at-build-time hard limits:

```text
LLM89_MAX_RESPONSE_BYTES     default 16 MiB
LLM89_MAX_SSE_EVENT_BYTES    default 4 MiB
LLM89_MAX_ERROR_BODY_BYTES   default 1 MiB
```

Crossing a limit returns `LLM_EOVERFLOW` and aborts the transfer.

These limits prevent an unbounded server response from consuming process memory.

## 14. Cancellation

The optional `llm_cancel_fn` gives cancellation independent of incoming response data.

If non-NULL, libcurl progress callbacks shall invoke it periodically through `CURLOPT_XFERINFOFUNCTION` with progress reporting enabled via `CURLOPT_NOPROGRESS` set to zero.

If the callback returns nonzero, the transfer shall abort and the public API shall return `LLM_ECANCELLED`.

The cancellation callback executes on the calling thread and shall not call back into the same `llm_client`.

`cancel == NULL` disables explicit cancellation polling.

## 15. HTTP handling

### 15.1 Successful status

Any HTTP status in `[200, 299]` proceeds to protocol parsing.

### 15.2 Error status

Any other HTTP status returns `LLM_EHTTP`.

The implementation shall buffer up to `LLM89_MAX_ERROR_BODY_BYTES` from the body and attempt to extract:

```text
error.message
```

from a JSON error response.

If extraction succeeds, `llm_error.message` contains that message, truncated to fit the fixed buffer.

Otherwise it contains a concise message containing the HTTP status.

`llm_error.http_status` always records the response status when known.

### 15.3 Redirects

V0 shall not automatically follow HTTP redirects.

This prevents silent credential forwarding to a different origin and keeps endpoint semantics explicit.

## 16. Transport errors

A libcurl failure returns `LLM_ECURL`, except when the implementation knows that the failure resulted from caller cancellation or a library resource-limit abort.

`llm_error.transport_code` shall contain the numeric `CURLcode`, converted to `long`.

The implementation shall configure `CURLOPT_ERRORBUFFER` and copy its text into `llm_error.message` when available.

The public header shall not include `<curl/curl.h>`.

## 17. Error object

Before work begins, every public operation receiving `llm_error *error` shall clear it when non-NULL.

On error:

```text
error->code           library status code
error->http_status    HTTP status or 0
error->transport_code libcurl CURLcode or 0
error->message        NUL-terminated diagnostic
```

Diagnostics are for humans and logs. Program logic shall branch on the integer status code and, when relevant, `http_status`.

The library shall not allocate memory inside `llm_error`.

Passing `NULL` for `error` is valid.

## 18. Memory ownership

### 18.1 Caller-owned

The caller owns and must keep alive during a public call:

- `llm_client_config` input strings;
- request model;
- message array;
- message strings;
- custom header array and strings;
- raw request JSON;
- callback userdata.

`llm_client_new()` shall copy all configuration data needed after it returns. Therefore configuration inputs may be released immediately after successful construction.

### 18.2 Library allocations transferred to caller

`llm_chat()` / `llm_json()` allocate response fields.

`llm_response_free()` shall:

- free `response->text` if non-NULL;
- free `response->json` if non-NULL;
- zero the structure.

### 18.3 Streaming

Streaming mode shall retain no complete-response buffer. Memory consumption shall remain bounded by the largest incomplete SSE event plus JSON parser allocations for the current event.

## 19. Client reuse and concurrency

A client owns one reusable libcurl easy handle.

Repeated calls through the same client shall reuse that handle so libcurl can reuse connections.

A single `llm_client` shall not support concurrent calls.

Distinct clients may execute concurrently in different threads, subject to libcurl's documented thread rules.

The library shall contain no internal mutexes in V0.

A public call made while the same client already executes another call produces undefined behavior; debug builds may assert this condition.

## 20. libcurl request setup

For each transfer, the implementation shall reset request-specific options before configuring the easy handle.

Conceptually:

```text
curl_easy_reset
set URL
set POST
set request body and exact length
set headers
set write callback
set cancellation/progress callback
set connect timeout
set total timeout
set error buffer
perform
read HTTP response code
```

All strings passed to libcurl shall remain alive until `curl_easy_perform()` returns.

The request body shall use an explicit byte length; the implementation shall not depend on libcurl computing it from a trailing NUL.

A `size_t` request length shall be range-checked before conversion to `curl_off_t`. Failure returns `LLM_EOVERFLOW`.

## 21. JSON construction and parsing

### 21.1 Construction

Typed requests shall be built with cJSON rather than string interpolation.

This guarantees correct escaping of:

- quotes;
- backslashes;
- control characters;
- Unicode encoded as UTF-8.

### 21.2 Parsing

Non-streaming typed parsing requires only:

```text
root
  -> choices[0]
     -> message
        -> content
```

Streaming typed parsing requires only:

```text
root
  -> choices[0]
     -> delta
        -> content
```

The implementation shall not copy or normalize raw JSON before exposing it to callers, except for adding a private terminating NUL when required by cJSON.

### 21.3 Duplicate fields

The implementation shall not attempt to assign semantics to malformed or ambiguous duplicate-key objects generated by a server. Protocol-required lookups shall use cJSON's case-sensitive object accessors.

## 22. C89 requirements

All library-owned source and public headers shall compile under:

```text
-std=c89 -pedantic-errors
```

No library source may rely on:

- `//` comments;
- declarations after statements within a block;
- C99 `for`-loop declarations;
- `inline`;
- `_Bool` / `<stdbool.h>`;
- `<stdint.h>`;
- designated initializers;
- compound literals;
- variadic macros;
- flexible array members;
- variable-length arrays;
- `snprintf` as a C-standard requirement;
- `long long` in the public API.

Use:

- `int` for booleans and enums represented as constants;
- `size_t` for memory sizes;
- `long` for public integer generation parameters and millisecond timeouts;
- opaque structures for implementation details.

The implementation may use types declared by libcurl internally, including `curl_off_t` and `CURLcode`, without exposing them in `llm89.h`.

## 23. Compiler discipline

CI shall compile library-owned code with at least GCC and Clang using:

```text
-std=c89
-pedantic-errors
-Wall
-Wextra
-Werror
-Wstrict-prototypes
-Wmissing-prototypes
-Wshadow
-Wwrite-strings
-Wundef
-Wformat=2
```

Warnings unsupported by a compiler may be omitted for that compiler.

A separate C++ translation unit shall include `llm89.h` and call representative functions to verify that the public header remains C++-linkable through `extern "C"`.

## 24. Required internal invariants

The implementation shall preserve these invariants:

1. A public function performs no network activity after local validation failure.
2. A streaming callback observes only complete SSE events.
3. A streaming callback never observes pointers that outlive the callback invocation.
4. No complete streaming response is accumulated internally.
5. Every allocation has one obvious owner.
6. A failed public call leaks no request, response, curl-header-list, JSON-tree, or buffer allocation.
7. Callback-requested cancellation returns `LLM_ECANCELLED`, not an implementation-dependent curl error.
8. HTTP failure returns `LLM_EHTTP`, independent of whether the body contains valid JSON.
9. TLS verification remains enabled.
10. The typed API accepts only features it can represent without lossy guessing; raw JSON handles everything else.

## 25. Tests

### 25.1 SSE unit tests

The SSE parser shall pass identical logical streams under byte partitions including:

- one byte at a time;
- all bytes at once;
- every possible split point for small fixtures;
- CRLF boundaries split across calls;
- UTF-8 code points split across calls;
- multiple events in one feed;
- multiple `data:` lines;
- comments and ignored fields;
- empty events;
- `[DONE]`;
- maximum-size and over-limit events.

### 25.2 JSON tests

Typed request tests shall verify exact parsed structure, not textual key ordering.

Test at least:

- all roles;
- JSON escaping;
- omitted optional fields;
- present optional fields;
- invalid roles;
- missing model;
- zero messages;
- negative max tokens;
- raw top-level non-object JSON rejection.

### 25.3 Protocol tests

Use a deterministic local HTTP test server. Tests shall cover:

- normal JSON completion;
- streamed completion;
- arbitrary HTTP body chunking;
- HTTP 400/401/429/500;
- JSON protocol error;
- malformed SSE JSON;
- missing `[DONE]`;
- connect failure;
- total timeout;
- stream-callback cancellation;
- progress-callback cancellation;
- custom headers;
- no API key;
- response size limit;
- SSE event size limit.

Tests shall not depend on a live commercial LLM endpoint.

### 25.4 Leak checking

Run the complete test suite under at least one dynamic memory checker such as AddressSanitizer or Valgrind in addition to the strict C89 compilation jobs.

## 26. Example usage

A minimal non-streaming caller is conceptually:

```c
#include <stdio.h>
#include <string.h>
#include "llm89.h"

int main(void)
{
    llm_client_config config;
    llm_chat_request request;
    llm_message messages[2];
    llm_response response;
    llm_error error;
    llm_client *client;
    int rc;

    memset(&config, 0, sizeof(config));
    memset(&request, 0, sizeof(request));
    memset(&response, 0, sizeof(response));

    rc = llm_global_init(&error);
    if (rc != LLM_OK) {
        fprintf(stderr, "%s\n", error.message);
        return 1;
    }

    config.endpoint = "https://example.invalid/v1/chat/completions";
    config.api_key = "secret";
    config.connect_timeout_ms = 10000L;
    config.timeout_ms = 60000L;

    client = llm_client_new(&config, &error);
    if (client == NULL) {
        fprintf(stderr, "%s\n", error.message);
        llm_global_cleanup();
        return 1;
    }

    messages[0].role = LLM_ROLE_SYSTEM;
    messages[0].content = "Answer concisely.";
    messages[1].role = LLM_ROLE_USER;
    messages[1].content = "What is 2 + 2?";

    request.model = "model-name";
    request.messages = messages;
    request.message_count = 2;

    rc = llm_chat(client, &request, &response, NULL, NULL, &error);
    if (rc == LLM_OK) {
        printf("%s\n", response.text);
        llm_response_free(&response);
    } else {
        fprintf(stderr, "%s\n", error.message);
    }

    llm_client_free(client);
    llm_global_cleanup();
    return rc == LLM_OK ? 0 : 1;
}
```

A streaming caller needs only a callback:

```c
static int on_stream(void *userdata, const llm_stream_event *event)
{
    FILE *out;

    out = (FILE *)userdata;

    if (event->type == LLM_EVENT_DATA && event->text != NULL) {
        if (fwrite(event->text, 1, event->text_len, out) != event->text_len) {
            return 1;
        }
        fflush(out);
    }

    return 0;
}
```

The network call remains synchronous:

```c
rc = llm_chat_stream(
    client,
    &request,
    on_stream,
    stdout,
    NULL,
    NULL,
    &error);
```

## 27. Deliberate omissions

### Async API

V0 does not expose libcurl multi. Callers that need parallel requests may create separate clients and place blocking calls on their own worker threads.

A future async layer can use libcurl multi without changing the protocol parser or typed request model.

### Retries

Retry decisions require policy: idempotency, timeout interpretation, rate limits, cost, latency, and provider behavior. They therefore belong above this protocol library.

### Tool calls

Tool calls belong to the LLM protocol but not to the minimal typed V0 data model. `llm_json_stream()` exposes their raw deltas without loss. A later typed tool-call adapter can be added independently.

### Conversation object

The protocol accepts a message sequence. Maintaining, truncating, summarizing, or persisting that sequence belongs to the caller.

### Provider abstraction

Anthropic, Gemini, and other non-OpenAI-compatible protocols should use separate request/response adapters over the same private HTTP/SSE transport machinery. V0 shall not encode provider conditionals into `llm_chat_request`.

## 28. V0 completion criterion

V0 is complete when a strict-C89 program can:

1. construct one reusable client;
2. send system/user/assistant text messages to an OpenAI-compatible endpoint;
3. receive a complete response or incremental text deltas;
4. inspect raw JSON when the typed API does not cover a feature;
5. cancel a request;
6. distinguish validation, transport, HTTP, JSON, SSE, protocol, cancellation, and resource-limit failures;
7. release every owned resource through explicit API calls;
8. compile library-owned C under `-std=c89 -pedantic-errors -Wall -Wextra -Werror`;
9. do all HTTP/TLS work through libcurl;
10. contain no CLI or agent/runtime policy.

That boundary is the minimal sufficient library: typed text chat for the common case, raw JSON/SSE for protocol completeness, and libcurl for transport.
