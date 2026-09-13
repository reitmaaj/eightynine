#ifndef LLM89_BUF_H
#define LLM89_BUF_H

#include <stddef.h>

/* llm89_buf - a growable byte buffer with a hard size limit and a private
 * terminating NUL. Used for response bodies, SSE event payloads, and error
 * bodies. The limit guards against unbounded server output. */

typedef struct llm_buf
{
    char *data;
    size_t len;   /* number of live bytes (excluding the trailing NUL) */
    size_t cap;   /* allocated bytes, including room for the NUL */
    size_t limit; /* hard cap on len; 0 means no limit */
    int failed;   /* an append crossed the limit */
} llm_buf;

void llm_buf_init(llm_buf *b, size_t limit);
void llm_buf_destroy(llm_buf *b);
int llm_buf_append(llm_buf *b, const char *bytes, size_t n);
void llm_buf_reset(llm_buf *b);
const char *llm_buf_data(const llm_buf *b);
size_t llm_buf_len(const llm_buf *b);
int llm_buf_failed(const llm_buf *b);

#endif
