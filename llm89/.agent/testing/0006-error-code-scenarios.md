# 0006-error-code-scenarios

SCENARIO: every error return sets error.code
GIVEN any public call returns a nonzero status
WHEN the call completes
THEN error.code equals that returned status code, for validation, transport,
HTTP, JSON, SSE, protocol, cancellation, and overflow failures alike.

SCENARIO: validation-before-network error codes
GIVEN an invalid role, a non-object raw body, or NULL arguments
WHEN the public call returns LLM_EINVAL or LLM_EJSON before any network
activity
THEN error.code is LLM_EINVAL or LLM_EJSON respectively.
