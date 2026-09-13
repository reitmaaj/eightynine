#ifndef LLM89_JSON_H
#define LLM89_JSON_H

#include <stddef.h>

#include "llm89.h"

/* llm89_json - request construction and response extraction using libj89.
 * No libcurl dependency; unit-testable in isolation. */

/* Build the typed chat request body as a malloc'd NUL-terminated string.
 * `streaming` selects "stream": true/false. Returns the LLM error code and,
 * on LLM_OK, sets *out to a malloc'd string the caller must free. */
int llm_json_build_request(const llm_chat_request *req, int streaming,
                           char **out);

/* Extract choices[0].message.content (non-stream). Returns a malloc'd
 * NUL-terminated string, or NULL when absent/unparseable/out-of-memory. */
char *llm_json_extract_content(const char *json, size_t len);

/* Extract choices[0].delta.content (stream). Returns a malloc'd
 * NUL-terminated string, or NULL when absent/unparseable/out-of-memory. */
char *llm_json_extract_delta(const char *json, size_t len);

/* Return 0 if `json` is one top-level JSON object, nonzero otherwise. */
int llm_json_validate_object(const char *json, size_t len);

/* Return 1 if `json` is any syntactically valid JSON value, 0 otherwise. */
int llm_json_parses(const char *json, size_t len);

/* Extract error.message from an HTTP error body. Returns a malloc'd string
 * or NULL. */
char *llm_json_extract_error_message(const char *json, size_t len);

#endif
