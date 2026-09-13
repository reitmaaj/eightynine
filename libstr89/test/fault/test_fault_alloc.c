/* test_fault_alloc.c - exhaustive injected-allocation failure sweeps. */

#include <string.h>

#include "str89_test.h"

static str89_view vbytes(const unsigned char *b, size_t n)
{
    str89_view v;
    int r;

    r = str89_view_init(&v, b, n);
    str89_test_check_status(r, STR89_OK, "fault: view accepted");
    return v;
}

static void check_buf(const str89_buf *b, const unsigned char *want,
                      size_t wantlen, size_t wantcap, const char *what)
{
    str89_test_check(b->len == wantlen, what);
    str89_test_check(b->cap == wantcap, what);
    str89_test_view_is(str89_buf_view(b), want, wantlen, what);
}

static void test_from_view_failures(void)
{
    str89_test_fault f;
    str89 s;
    str89_view v;
    int r;
    int done;
    long k;

    done = 0;
    for (k = 1; done == 0 && k < 8; k += 1)
    {
        str89_test_fault_init(&f, 0xA1UL);
        str89_init(&s);
        r = str89_view_init(&v, (const unsigned char *)"payload", 7);
        str89_test_check_status(r, STR89_OK, "fault from_view: view");
        str89_test_fault_fail_next(&f, k);
        r = str89_from_view(&s, &f.alloc, v);
        if (r == STR89_OK)
        {
            str89_test_check(s.len == 7, "fault from_view: content");
            str89_test_check(f.live == 1, "fault from_view: one block");
            str89_free(&s, &f.alloc);
            done = 1;
        }
        else
        {
            str89_test_check_status(r, STR89_ENOMEM, "fault from_view: ENOMEM");
            str89_test_check(s.data == NULL, "fault from_view: data unchanged");
            str89_test_check(s.len == 0, "fault from_view: len unchanged");
        }
        str89_test_check(str89_test_fault_leaked(&f) == 0,
                         "fault from_view: no leak");
    }
    str89_test_check(done != 0, "fault from_view: success reached");
}

static void test_copy_failures(void)
{
    str89_test_fault f;
    str89 src;
    str89 dst;
    int r;
    int done;
    long k;

    done = 0;
    for (k = 1; done == 0 && k < 8; k += 1)
    {
        str89_test_fault_init(&f, 0xA2UL);
        str89_init(&src);
        str89_init(&dst);
        r = str89_from_view(&src, &f.alloc,
                            vbytes((const unsigned char *)"source", 6));
        str89_test_check_status(r, STR89_OK, "fault copy: setup");
        str89_test_fault_fail_next(&f, k);
        r = str89_copy(&dst, &f.alloc, &src);
        if (r == STR89_OK)
        {
            str89_test_check(dst.len == 6, "fault copy: content");
            str89_free(&dst, &f.alloc);
            done = 1;
        }
        else
        {
            str89_test_check_status(r, STR89_ENOMEM, "fault copy: ENOMEM");
            str89_test_check(dst.data == NULL, "fault copy: data unchanged");
        }
        str89_free(&src, &f.alloc);
        str89_test_check(str89_test_fault_leaked(&f) == 0,
                         "fault copy: no leak");
    }
    str89_test_check(done != 0, "fault copy: success reached");
}

static void test_reserve_failures(void)
{
    str89_test_fault f;
    str89_buf b;
    size_t cap;
    int r;
    int done;
    long k;

    done = 0;
    for (k = 1; done == 0 && k < 8; k += 1)
    {
        str89_test_fault_init(&f, 0xA3UL);
        str89_buf_init(&b);
        r = str89_buf_set(&b, &f.alloc,
                          vbytes((const unsigned char *)"abcdef", 6));
        str89_test_check_status(r, STR89_OK, "fault reserve: setup");
        cap = b.cap;
        str89_test_fault_fail_next(&f, k);
        r = str89_buf_reserve(&b, &f.alloc, cap + 100);
        if (r == STR89_OK)
        {
            str89_test_check(b.cap >= cap + 100, "fault reserve: grown");
            done = 1;
        }
        else
        {
            str89_test_check_status(r, STR89_ENOMEM, "fault reserve: ENOMEM");
            check_buf(&b, (const unsigned char *)"abcdef", 6, cap,
                      "fault reserve: unchanged");
        }
        str89_buf_free(&b, &f.alloc);
        str89_test_check(str89_test_fault_leaked(&f) == 0,
                         "fault reserve: no leak");
    }
    str89_test_check(done != 0, "fault reserve: success reached");
}

static void test_set_failures(void)
{
    str89_test_fault f;
    str89_buf b;
    size_t cap;
    int r;
    int done;
    long k;

    done = 0;
    for (k = 1; done == 0 && k < 8; k += 1)
    {
        str89_test_fault_init(&f, 0xA4UL);
        str89_buf_init(&b);
        r = str89_buf_set(&b, &f.alloc,
                          vbytes((const unsigned char *)"abcdef", 6));
        str89_test_check_status(r, STR89_OK, "fault set: setup");
        cap = b.cap;
        str89_test_fault_fail_next(&f, k);
        r = str89_buf_set(
            &b, &f.alloc,
            vbytes((const unsigned char *)"0123456789abcdef", 16));
        if (r == STR89_OK)
        {
            str89_test_view_is(str89_buf_view(&b),
                               (const unsigned char *)"0123456789abcdef", 16,
                               "fault set: content");
            done = 1;
        }
        else
        {
            str89_test_check_status(r, STR89_ENOMEM, "fault set: ENOMEM");
            check_buf(&b, (const unsigned char *)"abcdef", 6, cap,
                      "fault set: unchanged");
        }
        str89_buf_free(&b, &f.alloc);
        str89_test_check(str89_test_fault_leaked(&f) == 0,
                         "fault set: no leak");
    }
    str89_test_check(done != 0, "fault set: success reached");
}

static void test_append_failures(void)
{
    str89_test_fault f;
    str89_buf b;
    size_t cap;
    int r;
    int done;
    long k;

    done = 0;
    for (k = 1; done == 0 && k < 8; k += 1)
    {
        str89_test_fault_init(&f, 0xA5UL);
        str89_buf_init(&b);
        r = str89_buf_set(&b, &f.alloc,
                          vbytes((const unsigned char *)"abcdef", 6));
        str89_test_check_status(r, STR89_OK, "fault append: setup");
        cap = b.cap;
        str89_test_fault_fail_next(&f, k);
        r = str89_buf_append(&b, &f.alloc,
                             vbytes((const unsigned char *)"XY", 2));
        if (r == STR89_OK)
        {
            str89_test_view_is(str89_buf_view(&b),
                               (const unsigned char *)"abcdefXY", 8,
                               "fault append: content");
            done = 1;
        }
        else
        {
            str89_test_check_status(r, STR89_ENOMEM, "fault append: ENOMEM");
            check_buf(&b, (const unsigned char *)"abcdef", 6, cap,
                      "fault append: unchanged");
        }
        str89_buf_free(&b, &f.alloc);
        str89_test_check(str89_test_fault_leaked(&f) == 0,
                         "fault append: no leak");
    }
    str89_test_check(done != 0, "fault append: success reached");
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
    str89_test_check_status(r, STR89_OK, "fault: fill");
}

static void test_insert_alias_failures(void)
{
    str89_test_fault f;
    str89_buf b;
    unsigned char snap[256];
    size_t n;
    size_t cap;
    str89_view src;
    int r;
    int done;
    long k;

    done = 0;
    for (k = 1; done == 0 && k < 8; k += 1)
    {
        str89_test_fault_init(&f, 0xA6UL);
        str89_buf_init(&b);
        r = str89_buf_set(&b, &f.alloc,
                          vbytes((const unsigned char *)"abcdef", 6));
        str89_test_check_status(r, STR89_OK, "fault insert: setup");
        fill_to_cap(&b);
        n = b.len;
        cap = b.cap;
        memcpy(snap, b.data, n);
        src = str89_buf_view(&b);
        str89_test_fault_fail_next(&f, k);
        r = str89_buf_insert(&b, &f.alloc, 3, src);
        if (r == STR89_OK)
        {
            str89_test_check(b.len == n * 2, "fault insert: length");
            done = 1;
        }
        else
        {
            str89_test_check_status(r, STR89_ENOMEM, "fault insert: ENOMEM");
            check_buf(&b, snap, n, cap, "fault insert: unchanged");
        }
        str89_buf_free(&b, &f.alloc);
        str89_test_check(str89_test_fault_leaked(&f) == 0,
                         "fault insert: no leak");
    }
    str89_test_check(done != 0, "fault insert: success reached");
}

static void test_replace_alias_failures(void)
{
    str89_test_fault f;
    str89_buf b;
    unsigned char snap[256];
    size_t n;
    size_t cap;
    str89_view src;
    int r;
    int done;
    long k;

    done = 0;
    for (k = 1; done == 0 && k < 8; k += 1)
    {
        str89_test_fault_init(&f, 0xA7UL);
        str89_buf_init(&b);
        r = str89_buf_set(&b, &f.alloc,
                          vbytes((const unsigned char *)"abcdef", 6));
        str89_test_check_status(r, STR89_OK, "fault replace: setup");
        fill_to_cap(&b);
        n = b.len;
        cap = b.cap;
        memcpy(snap, b.data, n);
        src = str89_buf_view(&b);
        str89_test_fault_fail_next(&f, k);
        r = str89_buf_replace(&b, &f.alloc, 1, 1, src);
        if (r == STR89_OK)
        {
            str89_test_check(b.len == n - 1 + n, "fault replace: length");
            done = 1;
        }
        else
        {
            str89_test_check_status(r, STR89_ENOMEM, "fault replace: ENOMEM");
            check_buf(&b, snap, n, cap, "fault replace: unchanged");
        }
        str89_buf_free(&b, &f.alloc);
        str89_test_check(str89_test_fault_leaked(&f) == 0,
                         "fault replace: no leak");
    }
    str89_test_check(done != 0, "fault replace: success reached");
}

static void test_cp_failures(void)
{
    str89_test_fault f;
    str89_buf b;
    size_t cap;
    int r;
    int done;
    long k;

    done = 0;
    for (k = 1; done == 0 && k < 8; k += 1)
    {
        str89_test_fault_init(&f, 0xA8UL);
        str89_buf_init(&b);
        r = str89_buf_set(&b, &f.alloc,
                          vbytes((const unsigned char *)"abcdef", 6));
        str89_test_check_status(r, STR89_OK, "fault cp: setup");
        cap = b.cap;
        str89_test_fault_fail_next(&f, k);
        r = str89_buf_append_cp(&b, &f.alloc, 0x20AC);
        if (r == STR89_OK)
        {
            str89_test_check(b.len == 9, "fault cp: length");
            done = 1;
        }
        else
        {
            str89_test_check_status(r, STR89_ENOMEM, "fault cp: ENOMEM");
            check_buf(&b, (const unsigned char *)"abcdef", 6, cap,
                      "fault cp: unchanged");
        }
        str89_buf_free(&b, &f.alloc);
        str89_test_check(str89_test_fault_leaked(&f) == 0, "fault cp: no leak");
    }
    str89_test_check(done != 0, "fault cp: success reached");
}

static void test_cross_allocator(void)
{
    str89_test_fault a;
    str89_test_fault b;
    str89 s;
    int r;

    str89_test_fault_init(&a, 0xB1UL);
    str89_test_fault_init(&b, 0xB2UL);
    str89_init(&s);
    r = str89_from_view(&s, &a.alloc,
                        vbytes((const unsigned char *)"owned", 5));
    str89_test_check_status(r, STR89_OK, "cross: allocated by A");
    str89_free(&s, &a.alloc);
    str89_test_check(a.bad_free == 0, "cross: A free clean");
    str89_test_check(b.bad_free == 0, "cross: B untouched");
    str89_test_check(str89_test_fault_leaked(&a) == 0, "cross: A no leak");
}

int main(void)
{
    test_from_view_failures();
    test_copy_failures();
    test_reserve_failures();
    test_set_failures();
    test_append_failures();
    test_insert_alias_failures();
    test_replace_alias_failures();
    test_cp_failures();
    test_cross_allocator();
    return str89_test_report();
}
