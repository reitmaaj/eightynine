/* buf.c - owning mutable UTF-8 builders. */

#include <string.h>

#include "str89_internal.h"

static size_t alias_offset(const unsigned char *base, const unsigned char *src)
{
    return (size_t)(src - base);
}

static const unsigned char *shift_ptr(const unsigned char *base, size_t off)
{
    return base + off;
}

void str89_buf_init(str89_buf *s)
{
    s->data = NULL;
    s->len = 0;
    s->cap = 0;
}

void str89_buf_clear(str89_buf *s)
{
    s->len = 0;
}

void str89_buf_free(str89_buf *s, const str89_alloc *alloc)
{
    if (s->data != NULL)
    {
        str89__free(alloc, s->data);
    }
    s->data = NULL;
    s->len = 0;
    s->cap = 0;
}

int str89_buf_reserve(str89_buf *s, const str89_alloc *alloc, size_t capacity)
{
    unsigned char *p;
    size_t nc;

    if (capacity <= s->cap)
    {
        return STR89_OK;
    }
    nc = str89__grow_cap(s->cap, capacity);
    if (nc == 0)
    {
        return STR89_ERANGE;
    }
    p = str89__realloc(alloc, s->data, nc);
    if (p == NULL)
    {
        return STR89_ENOMEM;
    }
    s->data = p;
    s->cap = nc;
    return STR89_OK;
}

/* Copy an aliased source into a fresh temporary block. On success *tmp owns
 * the block and *from points at it. */
static int copy_alias(const str89_alloc *alloc, str89_view src,
                      unsigned char **tmp, const unsigned char **from)
{
    unsigned char *p;

    p = str89__malloc(alloc, src.len);
    if (p == NULL)
    {
        return STR89_ENOMEM;
    }
    memcpy(p, src.data, src.len);
    *tmp = p;
    *from = p;
    return STR89_OK;
}

static void drop_tmp(const str89_alloc *alloc, unsigned char *tmp)
{
    if (tmp != NULL)
    {
        str89__free(alloc, tmp);
    }
}

static void insert_bytes(str89_buf *s, size_t offset, const unsigned char *src,
                         size_t n)
{
    size_t tail;

    tail = s->len - offset;
    if (tail != 0)
    {
        memmove(s->data + offset + n, s->data + offset, tail);
    }
    if (n != 0)
    {
        memmove(s->data + offset, src, n);
    }
    s->len = s->len + n;
}

static void replace_bytes(str89_buf *s, size_t offset, size_t len,
                          const unsigned char *src, size_t n)
{
    size_t tail;

    tail = s->len - offset - len;
    if (tail != 0)
    {
        memmove(s->data + offset + n, s->data + offset + len, tail);
    }
    if (n != 0)
    {
        memmove(s->data + offset, src, n);
    }
    s->len = s->len - len + n;
}

int str89_buf_set(str89_buf *s, const str89_alloc *alloc, str89_view src)
{
    int alias;
    size_t off;
    int r;
    const unsigned char *from;

    alias = str89__overlaps(s->data, s->len, src.data);
    off = 0;
    if (alias)
    {
        off = alias_offset(s->data, src.data);
    }
    r = str89_buf_reserve(s, alloc, src.len);
    if (r != STR89_OK)
    {
        return r;
    }
    from = src.data;
    if (alias)
    {
        from = shift_ptr(s->data, off);
    }
    if (src.len != 0)
    {
        memmove(s->data, from, src.len);
    }
    s->len = src.len;
    return STR89_OK;
}

int str89_buf_append(str89_buf *s, const str89_alloc *alloc, str89_view src)
{
    int alias;
    size_t off;
    size_t need;
    int r;
    const unsigned char *from;

    if (src.len == 0)
    {
        return STR89_OK;
    }
    r = str89__add(s->len, src.len, &need);
    if (r != STR89_OK)
    {
        return r;
    }
    alias = str89__overlaps(s->data, s->len, src.data);
    off = 0;
    if (alias)
    {
        off = alias_offset(s->data, src.data);
    }
    r = str89_buf_reserve(s, alloc, need);
    if (r != STR89_OK)
    {
        return r;
    }
    from = src.data;
    if (alias)
    {
        from = shift_ptr(s->data, off);
    }
    memmove(s->data + s->len, from, src.len);
    s->len = need;
    return STR89_OK;
}

int str89_buf_insert(str89_buf *s, const str89_alloc *alloc, size_t offset,
                     str89_view src)
{
    str89_view cur;
    int alias;
    size_t need;
    int b;
    int r;
    unsigned char *tmp;
    const unsigned char *from;

    if (offset > s->len)
    {
        return STR89_ERANGE;
    }
    cur = str89_buf_view(s);
    b = str89_view_is_boundary(cur, offset);
    if (b == 0)
    {
        return STR89_EBOUND;
    }
    if (src.len == 0)
    {
        return STR89_OK;
    }
    r = str89__add(s->len, src.len, &need);
    if (r != STR89_OK)
    {
        return r;
    }
    tmp = NULL;
    from = src.data;
    alias = str89__overlaps(s->data, s->len, src.data);
    if (alias)
    {
        r = copy_alias(alloc, src, &tmp, &from);
        if (r != STR89_OK)
        {
            return r;
        }
    }
    r = str89_buf_reserve(s, alloc, need);
    if (r != STR89_OK)
    {
        drop_tmp(alloc, tmp);
        return r;
    }
    insert_bytes(s, offset, from, src.len);
    drop_tmp(alloc, tmp);
    return STR89_OK;
}

int str89_buf_erase(str89_buf *s, size_t offset, size_t len)
{
    size_t end;
    size_t tail;
    int r;
    int b;
    str89_view cur;

    r = str89__add(offset, len, &end);
    if (r != STR89_OK)
    {
        return r;
    }
    if (end > s->len)
    {
        return STR89_ERANGE;
    }
    cur = str89_buf_view(s);
    b = str89_view_is_boundary(cur, offset);
    if (b == 0)
    {
        return STR89_EBOUND;
    }
    b = str89_view_is_boundary(cur, end);
    if (b == 0)
    {
        return STR89_EBOUND;
    }
    tail = s->len - end;
    if (tail != 0)
    {
        memmove(s->data + offset, s->data + end, tail);
    }
    s->len = s->len - len;
    return STR89_OK;
}

int str89_buf_replace(str89_buf *s, const str89_alloc *alloc, size_t offset,
                      size_t len, str89_view src)
{
    size_t end;
    size_t base;
    size_t newlen;
    int r;
    int b;
    int alias;
    str89_view cur;
    unsigned char *tmp;
    const unsigned char *from;

    r = str89__add(offset, len, &end);
    if (r != STR89_OK)
    {
        return r;
    }
    if (end > s->len)
    {
        return STR89_ERANGE;
    }
    if (len == 0)
    {
        r = str89_buf_insert(s, alloc, offset, src);
        return r;
    }
    if (src.len == 0)
    {
        r = str89_buf_erase(s, offset, len);
        return r;
    }
    cur = str89_buf_view(s);
    b = str89_view_is_boundary(cur, offset);
    if (b == 0)
    {
        return STR89_EBOUND;
    }
    b = str89_view_is_boundary(cur, end);
    if (b == 0)
    {
        return STR89_EBOUND;
    }
    base = s->len - len;
    r = str89__add(base, src.len, &newlen);
    if (r != STR89_OK)
    {
        return r;
    }
    tmp = NULL;
    from = src.data;
    alias = str89__overlaps(s->data, s->len, src.data);
    if (alias)
    {
        r = copy_alias(alloc, src, &tmp, &from);
        if (r != STR89_OK)
        {
            return r;
        }
    }
    r = str89_buf_reserve(s, alloc, newlen);
    if (r != STR89_OK)
    {
        drop_tmp(alloc, tmp);
        return r;
    }
    replace_bytes(s, offset, len, from, src.len);
    drop_tmp(alloc, tmp);
    return STR89_OK;
}

int str89_buf_append_cp(str89_buf *s, const str89_alloc *alloc, u89_cp cp)
{
    unsigned char tmp[4];
    str89_view v;
    int w;
    int r;

    w = u89_utf8_encode(cp, tmp);
    if (w == 0)
    {
        return STR89_EINVAL;
    }
    v.data = tmp;
    v.len = (size_t)w;
    r = str89_buf_append(s, alloc, v);
    return r;
}

int str89_buf_insert_cp(str89_buf *s, const str89_alloc *alloc, size_t offset,
                        u89_cp cp)
{
    unsigned char tmp[4];
    str89_view v;
    int w;
    int r;

    w = u89_utf8_encode(cp, tmp);
    if (w == 0)
    {
        return STR89_EINVAL;
    }
    v.data = tmp;
    v.len = (size_t)w;
    r = str89_buf_insert(s, alloc, offset, v);
    return r;
}

int str89_take(str89 *out, str89_buf *src)
{
    if (out->data != NULL)
    {
        return STR89_EINVAL;
    }
    if (out->len != 0)
    {
        return STR89_EINVAL;
    }
    out->data = src->data;
    out->len = src->len;
    src->data = NULL;
    src->len = 0;
    src->cap = 0;
    return STR89_OK;
}
