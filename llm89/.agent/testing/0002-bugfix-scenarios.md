# 0002-bugfix-scenarios

SCENARIO: non-streaming malformed JSON response
GIVEN an HTTP 2xx response whose body is not syntactically valid JSON
WHEN llm_chat is called
THEN it returns LLM_EJSON, not LLM_EPROTO.

SCENARIO: non-streaming valid JSON with wrong shape
GIVEN an HTTP 2xx response whose body is valid JSON but lacks
`choices[0].message.content`
WHEN llm_chat is called
THEN it returns LLM_EPROTO.

SCENARIO: streaming HTTP error body extraction
GIVEN a streaming request that receives a non-2xx HTTP status with a JSON
`error.message` body
WHEN llm_chat_stream is called
THEN it returns LLM_EHTTP with http_status set
AND error.message contains the server's `error.message` text.
