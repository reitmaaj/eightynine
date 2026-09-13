/* str.c - refcounted immutable byte-string carrier (S) for j89_alg. */
#include <stddef.h>

#include "alg_internal.h"

j89a_str *j89a_str_new(const void *bytes, size_t len, j89a_alloc *al)
{
    j89a_str *s;
    size_t base;
    size_t size;
    base = offsetof(struct j89a_str, data);
    if (len > (size_t)-1 - base - 1)
    {
        al->failed = 1;
        return NULL;
    }
    size = base + len + 1;
    s = (j89a_str *)j89a_malloc(al, size);
    if (s == NULL)
    {
        return NULL;
    }
    s->hdr.refs = 1;
    s->hdr.freer = al->free;
    s->hdr.fctx = al->ctx;
    s->len = len;
    if (len != 0)
    {
        memcpy(s->data, bytes, len);
    }
    s->data[len] = '\0';
    return s;
}

void j89a_str_retain(j89a_str *s)
{
    if (s == NULL)
    {
        return;
    }
    s->hdr.refs = s->hdr.refs + 1;
}

void j89a_str_release(j89a_str *s)
{
    if (s == NULL)
    {
        return;
    }
    s->hdr.refs = s->hdr.refs - 1;
    if (s->hdr.refs == 0)
    {
        j89a_nfree(s->hdr.freer, s->hdr.fctx, s);
    }
}

size_t j89a_str_len(const j89a_str *s)
{
    return s->len;
}

const void *j89a_str_data(const j89a_str *s)
{
    return s->data;
}
