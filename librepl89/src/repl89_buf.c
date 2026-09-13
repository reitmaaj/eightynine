/* repl89_buf.c - growable byte buffer. All mutation is atomic: a failed
 * reservation leaves the buffer unchanged. */

#include <string.h>

#include "repl89_internal.h"

void repl89_buf_init(repl89_buf *b)
{
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

void repl89_buf_free(repl89_buf *b)
{
    repl89_mem_free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

void repl89_buf_clear(repl89_buf *b)
{
    b->len = 0;
}

static void cap_double(size_t *cap, size_t *rem)
{
    *cap = *cap * 2;
    *rem = *rem / 2;
}

static int cap_more(size_t *cap, size_t *rem)
{
    if (*rem <= 16)
    {
        return 0;
    }
    if (*cap > (((size_t)-1) / 2))
    {
        return 0;
    }
    cap_double(cap, rem);
    return 1;
}

size_t repl89_cap_for(size_t need)
{
    size_t cap;
    size_t rem;
    int go;

    cap = 16;
    rem = need;
    go = 1;
    while (go != 0)
    {
        go = cap_more(&cap, &rem);
    }
    return cap;
}

repl89_error repl89_buf_reserve(repl89_buf *b, size_t extra)
{
    size_t need;
    size_t cap;
    char *p;

    if (extra <= b->cap - b->len)
    {
        return REPL89_OK;
    }
    need = b->len + extra;
    cap = repl89_cap_for(need);
    if (cap < need)
    {
        cap = need;
    }
    p = repl89_mem_realloc(b->data, cap);
    if (p == NULL)
    {
        return REPL89_ENOMEM;
    }
    b->data = p;
    b->cap = cap;
    return REPL89_OK;
}

repl89_error repl89_buf_insert(repl89_buf *b, size_t pos, const char *p,
                               size_t n)
{
    repl89_error err;
    size_t tail;

    if (n == 0)
    {
        return REPL89_OK;
    }
    err = repl89_buf_reserve(b, n);
    if (err != REPL89_OK)
    {
        return err;
    }
    tail = b->len - pos;
    if (tail > 0)
    {
        memmove(b->data + pos + n, b->data + pos, tail);
    }
    memcpy(b->data + pos, p, n);
    b->len = b->len + n;
    return REPL89_OK;
}

repl89_error repl89_buf_delete(repl89_buf *b, size_t pos, size_t n)
{
    size_t tail;

    if (n == 0)
    {
        return REPL89_OK;
    }
    tail = b->len - pos - n;
    if (tail > 0)
    {
        memmove(b->data + pos, b->data + pos + n, tail);
    }
    b->len = b->len - n;
    return REPL89_OK;
}
