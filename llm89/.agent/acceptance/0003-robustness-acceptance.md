# 0003-robustness-acceptance

## Must exhibit

ACCEPT: a custom header with a NULL name or NULL value is rejected with
LLM_EINVAL at client construction (no crash, no dereference of NULL).
ACCEPT: a typed request with a NaN or infinite temperature is rejected with
LLM_EINVAL before any JSON is built.
ACCEPT: a client whose custom-header storage allocation fails partway returns
NULL with LLM_ENOMEM and never performs an invalid free on uninitialized
pointers.

## Must reject (unacceptable behavior)

REJECT: crashing (NULL dereference) when a custom header name or value is NULL.
REJECT: freeing uninitialized memory if custom-header storage allocation fails
partway through construction.
