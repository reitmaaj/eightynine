# 0001-llm89-concept

`llm89` is a minimal C89 library that issues text-chat requests to an
OpenAI-compatible LLM HTTP endpoint and returns either a complete response or
an incremental text stream.

It is a protocol client only: it contains no agent loop, tool executor,
conversation store, retry policy, prompt framework, CLI, scheduler, model
registry, or provider discovery.

Position in the system:

```text
application
    | messages / generation parameters
    v
llm89
    | JSON + SSE
    v
libcurl
    | HTTP(S)
    v
endpoint
```

V0 scope:

- synchronous blocking requests only;
- `POST .../chat/completions` semantics;
- Bearer-token auth (optional);
- system / user / assistant text messages;
- model selection, optional temperature and max-tokens;
- non-streaming and SSE streaming completion;
- incremental cancellation;
- connect and total timeouts;
- caller-supplied extra headers;
- raw response JSON and raw SSE event JSON access;
- deterministic ownership and cleanup;
- libcurl TLS verification left enabled.

JSON is handled by the sibling `libj89` project (parse + build), replacing the
spec's cJSON. This is the one deliberate deviation from `spec.md`.

The raw JSON interfaces (`llm_json`, `llm_json_stream`) are the escape hatch for
protocol features outside the typed V0 model (tools, structured output,
provider fields, multimodal objects).

One public call causes at most one HTTP request. No retries, no rate-limit
handling beyond returning the HTTP error.
