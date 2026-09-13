# 0003-robustness-scenarios

SCENARIO: NULL custom header name
GIVEN an llm_client_config whose custom header has a NULL name
WHEN llm_client_new is called
THEN it returns NULL with LLM_EINVAL and performs no network activity.

SCENARIO: NULL custom header value
GIVEN an llm_client_config whose custom header has a NULL value
WHEN llm_client_new is called
THEN it returns NULL with LLM_EINVAL.

SCENARIO: non-finite temperature
GIVEN a typed request with set_temperature and a NaN or infinite temperature
WHEN llm_json_build_request is called
THEN it returns LLM_EINVAL.

SCENARIO: partial header allocation failure
GIVEN a client whose custom-header storage allocation fails partway through
WHEN llm_client_new is called
THEN it returns NULL with LLM_ENOMEM without freeing uninitialized memory
(no crash, no invalid free).
