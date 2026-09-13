#ifndef LLM89_SSE_H
#define LLM89_SSE_H

#include <stddef.h>

#include "llm89_buf.h"

/* llm89_sse - a byte-oriented SSE field parser with no libcurl dependency.
 *
 * Feed arbitrary byte partitions via llm_sse_feed. Each complete `data:` event
 * (terminated by a blank line) is delivered to the configured emit callback
 * with the fully assembled payload (multi-line `data` concatenated with '\n').
 * [DONE] detection is left to the caller, which also decides when to stop. */

typedef int (*llm_sse_emit_fn)(void *userdata, const char *payload,
                               size_t len);

typedef struct llm_sse
{
    llm_buf line; /* current field line (excludes terminator) */
    llm_buf data; /* assembled data payload of the current event */
    int has_data; /* current event has at least one data line */
    int after_cr; /* last consumed byte was a carriage return */
    int failed;   /* overflow encountered */
    int stopped;  /* emit callback requested stop */
    void *userdata;
    llm_sse_emit_fn emit;
} llm_sse;

void llm_sse_init(llm_sse *p, size_t max_event_bytes, void *userdata,
                  llm_sse_emit_fn emit);
void llm_sse_destroy(llm_sse *p);
/* Returns 0 on success; 1 on overflow (parser failed); -1 if the emit
 * callback requested a stop. */
int llm_sse_feed(llm_sse *p, const char *bytes, size_t len);
int llm_sse_failed(const llm_sse *p);

#endif
