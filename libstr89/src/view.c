/* view.c - validation, boundaries, subviews, equality, ordering, search. */

#include <string.h>

#include "str89_internal.h"

static const unsigned char *slice_at(const unsigned char *base, size_t offset)
{
    if (offset == 0)
    {
        return base;
    }
    return base + offset;
}

static size_t min_len(size_t a, size_t b)
{
    if (a < b)
    {
        return a;
    }
    return b;
}

static int sign_of(int c)
{
    if (c < 0)
    {
        return -1;
    }
    if (c > 0)
    {
        return 1;
    }
    return 0;
}

static int common_prefix(str89_view a, str89_view b, size_t n)
{
    int c;

    if (n == 0)
    {
        return 0;
    }
    c = memcmp(a.data, b.data, n);
    c = sign_of(c);
    return c;
}

static int length_order(size_t a, size_t b)
{
    if (a < b)
    {
        return -1;
    }
    if (a > b)
    {
        return 1;
    }
    return 0;
}

int str89_view_init(str89_view *out, const unsigned char *data, size_t len)
{
    int ok;
    str89_view v;

    ok = str89__data_ok(data, len);
    if (ok == 0)
    {
        return STR89_EINVAL;
    }
    ok = u89_utf8_valid(data, len);
    if (ok == 0)
    {
        return STR89_EUTF8;
    }
    v.data = data;
    v.len = len;
    *out = v;
    return STR89_OK;
}

str89_view str89_view_of(const str89 *s)
{
    str89_view v;

    v.data = s->data;
    v.len = s->len;
    return v;
}

str89_view str89_buf_view(const str89_buf *s)
{
    str89_view v;

    v.data = s->data;
    v.len = s->len;
    return v;
}

int str89_view_is_boundary(str89_view s, size_t offset)
{
    u89_status st;

    if (offset == 0)
    {
        return 1;
    }
    if (offset == s.len)
    {
        return 1;
    }
    if (offset > s.len)
    {
        return 0;
    }
    st = u89_utf8_prev(s.data, s.len, offset, NULL, NULL);
    if (st == U89_OK)
    {
        return 1;
    }
    return 0;
}

int str89_view_sub(str89_view s, size_t offset, size_t len, str89_view *out)
{
    size_t end;
    int r;
    int b;
    str89_view v;

    r = str89__add(offset, len, &end);
    if (r != STR89_OK)
    {
        return r;
    }
    if (end > s.len)
    {
        return STR89_ERANGE;
    }
    b = str89_view_is_boundary(s, offset);
    if (b == 0)
    {
        return STR89_EBOUND;
    }
    b = str89_view_is_boundary(s, end);
    if (b == 0)
    {
        return STR89_EBOUND;
    }
    v.data = slice_at(s.data, offset);
    v.len = len;
    *out = v;
    return STR89_OK;
}

int str89_view_equal(str89_view a, str89_view b)
{
    int c;

    if (a.len != b.len)
    {
        return 0;
    }
    if (a.len == 0)
    {
        return 1;
    }
    c = memcmp(a.data, b.data, a.len);
    if (c == 0)
    {
        return 1;
    }
    return 0;
}

int str89_view_compare(str89_view a, str89_view b)
{
    size_t n;
    int c;

    n = min_len(a.len, b.len);
    c = common_prefix(a, b, n);
    if (c != 0)
    {
        return c;
    }
    c = length_order(a.len, b.len);
    return c;
}

static int match_at(str89_view haystack, str89_view needle, size_t at)
{
    int c;

    c = memcmp(haystack.data + at, needle.data, needle.len);
    if (c == 0)
    {
        return 1;
    }
    return 0;
}

static int found_at(size_t at, size_t *offset)
{
    *offset = at;
    return STR89_OK;
}

int str89_view_find(str89_view haystack, str89_view needle, size_t from,
                    size_t *offset)
{
    size_t i;
    int b;
    int hit;
    int r;

    if (from > haystack.len)
    {
        return STR89_ERANGE;
    }
    b = str89_view_is_boundary(haystack, from);
    if (b == 0)
    {
        return STR89_EBOUND;
    }
    if (needle.len == 0)
    {
        r = found_at(from, offset);
        return r;
    }
    for (i = from; needle.len <= haystack.len - i; ++i)
    {
        hit = match_at(haystack, needle, i);
        if (hit != 0)
        {
            r = found_at(i, offset);
            return r;
        }
    }
    *offset = STR89_NPOS;
    return STR89_OK;
}
