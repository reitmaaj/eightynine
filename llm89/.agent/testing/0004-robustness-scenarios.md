# 0004-robustness-scenarios

SCENARIO: non-streaming request receives an SSE response
GIVEN a server returns a text/event-stream body to a non-streaming llm_chat
request
WHEN llm_chat is called
THEN it returns LLM_EJSON (the SSE text is not valid JSON) without crashing.

SCENARIO: chunked error routing
GIVEN a server uses chunked transfer-encoding with a single framing header
WHEN a streaming request is served
THEN the events are delivered intact.

SCENARIO: header-list allocation failure
GIVEN the request-header list cannot be allocated
WHEN a public call runs
THEN it returns LLM_ENOMEM and error.code is LLM_ENOMEM (consistent).
