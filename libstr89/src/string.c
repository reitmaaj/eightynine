/* string.c - owning finalized UTF-8 strings. */

#include <string.h>

#include "str89_internal.h"

static int set_empty(str89 *out)
{
    out->data = NULL;
    out->len = 0;
    return STR89_OK;
}

void str89_init(str89 *s)
{
    s->data = NULL;
    s->len = 0;
}

int str89_from_view(str89 *out, const str89_alloc *alloc, str89_view src)
{
    unsigned char *p;
    int r;

    if (src.len == 0)
    {
        r = set_empty(out);
        return r;
    }
    p = str89__malloc(alloc, src.len);
    if (p == NULL)
    {
        return STR89_ENOMEM;
    }
    memcpy(p, src.data, src.len);
    out->data = p;
    out->len = src.len;
    return STR89_OK;
}

int str89_copy(str89 *out, const str89_alloc *alloc, const str89 *src)
{
    str89_view v;
    int r;

    if (out == src)
    {
        return STR89_OK;
    }
    v = str89_view_of(src);
    r = str89_from_view(out, alloc, v);
    return r;
}

void str89_free(str89 *s, const str89_alloc *alloc)
{
    if (s->data != NULL)
    {
        str89__free(alloc, s->data);
    }
    s->data = NULL;
    s->len = 0;
}
