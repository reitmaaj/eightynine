# 0001-stakeholders

SCENARIO: application developer
AS a C application developer who must talk to an OpenAI-compatible endpoint
I WANT a small, dependency-thin blocking client for text chat
SO THAT I can ship complete and streamed completions without owning sockets,
TLS, or HTTP framing.

SCENARIO: protocol integrator
AS a developer integrating a provider feature the typed API does not model
I WANT raw JSON request and raw SSE event access
SO THAT tools, structured output, and provider-specific fields reach the
endpoint without lossy guessing.

SCENARIO: embedded / constrained-environment developer
AS a developer constrained by strict C89 and small static footprints
I WANT a library that compiles warning-clean under `-std=c89 -pedantic-errors`
SO THAT I can build it into strict toolchains and static analyzers.

SCENARIO: application owner
AS an application owner with cancellation and timeout requirements
I WANT connect/total timeouts and a cancellation hook independent of response
data
SO THAT hung endpoints never block the application indefinitely.

SCENARIO: library consumer
AS a library consumer
I WANT explicit, deterministic ownership and cleanup
SO THAT every returned allocation has one obvious owner and can be released
with `llm_response_free`, without leaks or hidden lifetimes.
