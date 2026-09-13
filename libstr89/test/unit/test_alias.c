/* test_alias.c - overlapping source/destination mutation. */

#include <string.h>

#include "str89_test.h"

static const unsigned char base[] = "abcdef";

static str89_view vbytes(const unsigned char *b, size_t n)
{
    str89_view v;
    int r;

    r = str89_view_init(&v, b, n);
    str89_test_check_status(r, STR89_OK, "alias: view accepted");
    return v;
}

static void fill_to_cap(str89_buf *b)
{
    unsigned char fill[64];
    size_t need;
    size_t i;
    int r;

    need = b->cap - b->len;
    if (need > sizeof(fill))
    {
        need = sizeof(fill);
    }
    for (i = 0; i < need; i += 1)
    {
        fill[i] = (unsigned char)('0' + (i % 10));
    }
    r = str89_buf_append(b, NULL, vbytes(fill, need));
    str89_test_check_status(r, STR89_OK, "alias: fill");
}

static void snapshot(const str89_buf *b, unsigned char *out, size_t *n)
{
    *n = b->len;
    if (*n != 0)
    {
        memcpy(out, b->data, *n);
    }
}

static str89_view subview(const str89_buf *b, size_t start, size_t len)
{
    str89_view v;

    v.data = b->data + start;
    v.len = len;
    return v;
}

static void append_alias_case(size_t start, size_t len, int force,
                              const char *what)
{
    str89_buf b;
    unsigned char snap[512];
    unsigned char want[1024];
    size_t n;
    str89_view v;
    int r;

    str89_buf_init(&b);
    r = str89_buf_set(&b, NULL, vbytes(base, sizeof(base) - 1));
    str89_test_check_status(r, STR89_OK, what);
    if (force != 0)
    {
        fill_to_cap(&b);
    }
    snapshot(&b, snap, &n);
    v = subview(&b, start, len);
    memcpy(want, snap, n);
    memcpy(want + n, snap + start, len);
    r = str89_buf_append(&b, NULL, v);
    str89_test_check_status(r, STR89_OK, what);
    str89_test_valid_buf(&b, what);
    str89_test_view_is(str89_buf_view(&b), want, n + len, what);
    str89_buf_free(&b, NULL);
}

static void insert_alias_case(size_t at, size_t start, size_t len, int force,
                              const char *what)
{
    str89_buf b;
    unsigned char snap[512];
    unsigned char want[1024];
    size_t n;
    str89_view v;
    int r;

    str89_buf_init(&b);
    r = str89_buf_set(&b, NULL, vbytes(base, sizeof(base) - 1));
    str89_test_check_status(r, STR89_OK, what);
    if (force != 0)
    {
        fill_to_cap(&b);
    }
    snapshot(&b, snap, &n);
    v = subview(&b, start, len);
    memcpy(want, snap, at);
    memcpy(want + at, snap + start, len);
    memcpy(want + at + len, snap + at, n - at);
    r = str89_buf_insert(&b, NULL, at, v);
    str89_test_check_status(r, STR89_OK, what);
    str89_test_valid_buf(&b, what);
    str89_test_view_is(str89_buf_view(&b), want, n + len, what);
    str89_buf_free(&b, NULL);
}

static void replace_alias_case(size_t off, size_t rmlen, size_t start,
                               size_t len, int force, const char *what)
{
    str89_buf b;
    unsigned char snap[512];
    unsigned char want[1024];
    size_t n;
    str89_view v;
    int r;

    str89_buf_init(&b);
    r = str89_buf_set(&b, NULL, vbytes(base, sizeof(base) - 1));
    str89_test_check_status(r, STR89_OK, what);
    if (force != 0)
    {
        fill_to_cap(&b);
    }
    snapshot(&b, snap, &n);
    v = subview(&b, start, len);
    memcpy(want, snap, off);
    memcpy(want + off, snap + start, len);
    memcpy(want + off + len, snap + off + rmlen, n - off - rmlen);
    r = str89_buf_replace(&b, NULL, off, rmlen, v);
    str89_test_check_status(r, STR89_OK, what);
    str89_test_valid_buf(&b, what);
    str89_test_view_is(str89_buf_view(&b), want, n - rmlen + len, what);
    str89_buf_free(&b, NULL);
}

static void test_append_alias(void)
{
    append_alias_case(0, 6, 0, "alias append: whole spare");
    append_alias_case(0, 6, 1, "alias append: whole realloc");
    append_alias_case(0, 3, 0, "alias append: prefix spare");
    append_alias_case(0, 3, 1, "alias append: prefix realloc");
    append_alias_case(2, 2, 0, "alias append: interior spare");
    append_alias_case(2, 2, 1, "alias append: interior realloc");
    append_alias_case(3, 3, 0, "alias append: suffix spare");
    append_alias_case(3, 3, 1, "alias append: suffix realloc");
}

static void test_insert_alias(void)
{
    insert_alias_case(0, 0, 6, 0, "alias insert: at 0 whole");
    insert_alias_case(0, 0, 6, 1, "alias insert: at 0 whole realloc");
    insert_alias_case(3, 0, 6, 0, "alias insert: at 3 whole");
    insert_alias_case(3, 0, 6, 1, "alias insert: at 3 whole realloc");
    insert_alias_case(6, 0, 6, 0, "alias insert: at end whole");
    insert_alias_case(6, 0, 6, 1, "alias insert: at end whole realloc");
    insert_alias_case(3, 0, 2, 0, "alias insert: prefix");
    insert_alias_case(3, 0, 2, 1, "alias insert: prefix realloc");
    insert_alias_case(3, 4, 2, 0, "alias insert: suffix");
    insert_alias_case(3, 4, 2, 1, "alias insert: suffix realloc");
    insert_alias_case(2, 2, 2, 0, "alias insert: interior");
    insert_alias_case(2, 2, 2, 1, "alias insert: interior realloc");
    insert_alias_case(4, 1, 4, 0, "alias insert: straddling view");
    insert_alias_case(4, 1, 4, 1, "alias insert: straddling view realloc");
}

static void test_replace_alias(void)
{
    replace_alias_case(3, 2, 0, 2, 0, "alias replace: before spare");
    replace_alias_case(3, 2, 0, 2, 1, "alias replace: before realloc");
    replace_alias_case(1, 2, 4, 2, 0, "alias replace: after spare");
    replace_alias_case(1, 2, 4, 2, 1, "alias replace: after realloc");
    replace_alias_case(2, 2, 2, 2, 0, "alias replace: exact spare");
    replace_alias_case(2, 2, 2, 2, 1, "alias replace: exact realloc");
    replace_alias_case(2, 2, 1, 4, 0, "alias replace: spanning spare");
    replace_alias_case(2, 2, 1, 4, 1, "alias replace: spanning realloc");
    replace_alias_case(3, 2, 2, 3, 0, "alias replace: overlap spare");
    replace_alias_case(3, 2, 2, 3, 1, "alias replace: overlap realloc");
    replace_alias_case(2, 1, 0, 6, 0, "alias replace: grow spare");
    replace_alias_case(2, 1, 0, 6, 1, "alias replace: grow realloc");
    replace_alias_case(1, 4, 3, 2, 0, "alias replace: shrink spare");
    replace_alias_case(1, 4, 3, 2, 1, "alias replace: shrink realloc");
}

int main(void)
{
    test_append_alias();
    test_insert_alias();
    test_replace_alias();
    return str89_test_report();
}
