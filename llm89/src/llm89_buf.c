/* llm89_buf.c - growable byte buffer with a hard size limit. */
#include <stdlib.h>
#include <string.h>

#include "llm89_buf.h"

static size_t next_cap(size_t need)
{
    size_t cap;
    cap = 256;
    if (need <= cap)
    {
        return cap;
    }
    while (cap < need)
    {
        cap = cap * 2;
    }
    return cap;
}

void llm_buf_init(llm_buf *b, size_t limit)
{
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
    b->limit = limit;
    b->failed = 0;
}

void llm_buf_destroy(llm_buf *b)
{
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

int llm_buf_append(llm_buf *b, const char *bytes, size_t n)
{
    size_t i;
    size_t over;
    over = (b->limit != 0) && (n > b->limit - b->len);
    if (over)
    {
        b->failed = 1;
        return 1;
    }
    if (b->cap < b->len + n + 1)
    {
        size_t ncap;
        char *np;
        ncap = next_cap(b->len + n + 1);
        np = (char *)realloc(b->data, ncap);
        if (np == NULL)
        {
            b->failed = 1;
            return 1;
        }
        b->data = np;
        b->cap = ncap;
    }
    i = 0;
    while (i < n)
    {
        b->data[b->len + i] = bytes[i];
        i = i + 1;
    }
    b->len = b->len + n;
    b->data[b->len] = '\0';
    return 0;
}

void llm_buf_reset(llm_buf *b)
{
    b->len = 0;
    b->failed = 0;
    if (b->data != NULL)
    {
        b->data[0] = '\0';
    }
}

const char *llm_buf_data(const llm_buf *b)
{
    if (b->data == NULL)
    {
        static const char empty[] = "";
        return empty;
    }
    return b->data;
}

size_t llm_buf_len(const llm_buf *b)
{
    return b->len;
}

int llm_buf_failed(const llm_buf *b)
{
    return b->failed;
}
