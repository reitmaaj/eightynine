# 0006-error-code-acceptance

## Must exhibit

ACCEPT: on every nonzero return from a public operation, `error->code` matches
the returned status code (LLM_EINVAL, LLM_ENOMEM, LLM_ECURL, LLM_EHTTP,
LLM_EJSON, LLM_EPROTO, LLM_ECANCELLED, LLM_EOVERFLOW).
ACCEPT: validation failures that occur before any network activity still report
their code (LLM_EINVAL / LLM_EJSON) in `error->code`.

## Must reject (unacceptable behavior)

REJECT: returning a nonzero status while leaving `error->code` as 0 (unset),
regardless of whether the failure was detected before or during the transfer.
