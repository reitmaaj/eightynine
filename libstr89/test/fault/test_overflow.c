/* test_overflow.c - checked size arithmetic and overflow paths. */

#include "str89_internal.h"
#include "str89_test.h"

static void test_add(void)
{
    size_t out;
    int r;

    out = 123;
    r = str89__add(0, 0, &out);
    str89_test_check_status(r, STR89_OK, "add: 0+0");
    str89_test_check(out == 0, "add: 0+0 value");

    r = str89__add(1, 2, &out);
    str89_test_check_status(r, STR89_OK, "add: small");
    str89_test_check(out == 3, "add: small value");

    r = str89__add(STR89_NPOS - 1, 1, &out);
    str89_test_check_status(r, STR89_OK, "add: max-1 + 1");
    str89_test_check(out == STR89_NPOS, "add: max-1 + 1 value");

    out = 456;
    r = str89__add(STR89_NPOS, 1, &out);
    str89_test_check_status(r, STR89_ERANGE, "add: overflow");
    str89_test_check(out == 456, "add: overflow leaves out");

    r = str89__add(1, STR89_NPOS, &out);
    str89_test_check_status(r, STR89_ERANGE, "add: overflow reversed");
}

static void test_grow_cap(void)
{
    size_t n;

    n = str89__grow_cap(0, 0);
    str89_test_check(n == 16, "grow: 0 -> 16");
    n = str89__grow_cap(0, 1);
    str89_test_check(n == 16, "grow: 0 -> >= 1");
    n = str89__grow_cap(0, 16);
    str89_test_check(n == 16, "grow: 0 -> 16 exact");
    n = str89__grow_cap(0, 17);
    str89_test_check(n == 32, "grow: 0 -> 32");
    n = str89__grow_cap(16, 17);
    str89_test_check(n == 32, "grow: 16 -> 32");
    n = str89__grow_cap(16, 16);
    str89_test_check(n == 16, "grow: sufficient");
    n = str89__grow_cap(STR89_NPOS / 2 + 1, STR89_NPOS);
    str89_test_check(n == 0, "grow: doubling overflow");
    n = str89__grow_cap(STR89_NPOS, 0);
    str89_test_check(n == STR89_NPOS, "grow: no growth needed");
}

static void test_append_overflow(void)
{
    str89_test_fault f;
    str89_buf b;
    str89_view huge;
    long calls;
    int r;

    str89_test_fault_init(&f, 0xC1UL);
    str89_buf_init(&b);
    r = str89_buf_set(&b, &f.alloc, str89_test_cview("a"));
    str89_test_check_status(r, STR89_OK, "overflow append: setup");
    huge.data = b.data;
    huge.len = STR89_NPOS;
    calls = f.calls;
    r = str89_buf_append(&b, &f.alloc, huge);
    str89_test_check_status(r, STR89_ERANGE, "overflow append: ERANGE");
    str89_test_check(f.calls == calls, "overflow append: no allocation");
    str89_test_view_is(str89_buf_view(&b), (const unsigned char *)"a", 1,
                       "overflow append: unchanged");
    str89_buf_free(&b, &f.alloc);
}

static void test_erase_overflow(void)
{
    str89_test_fault f;
    str89_buf b;
    long calls;
    int r;

    str89_test_fault_init(&f, 0xC2UL);
    str89_buf_init(&b);
    r = str89_buf_set(&b, &f.alloc, str89_test_cview("abc"));
    str89_test_check_status(r, STR89_OK, "overflow erase: setup");
    calls = f.calls;
    r = str89_buf_erase(&b, 1, STR89_NPOS);
    str89_test_check_status(r, STR89_ERANGE, "overflow erase: ERANGE");
    str89_test_check(f.calls == calls, "overflow erase: no allocation");
    r = str89_buf_erase(&b, STR89_NPOS, 0);
    str89_test_check_status(r, STR89_ERANGE, "overflow erase: huge offset");
    str89_test_view_is(str89_buf_view(&b), (const unsigned char *)"abc", 3,
                       "overflow erase: unchanged");
    str89_buf_free(&b, &f.alloc);
}

static void test_replace_overflow(void)
{
    str89_test_fault f;
    str89_buf b;
    long calls;
    int r;

    str89_test_fault_init(&f, 0xC3UL);
    str89_buf_init(&b);
    r = str89_buf_set(&b, &f.alloc, str89_test_cview("abc"));
    str89_test_check_status(r, STR89_OK, "overflow replace: setup");
    calls = f.calls;
    r = str89_buf_replace(&b, &f.alloc, 1, STR89_NPOS, str89_test_cview("x"));
    str89_test_check_status(r, STR89_ERANGE, "overflow replace: ERANGE");
    str89_test_check(f.calls == calls, "overflow replace: no allocation");
    str89_test_view_is(str89_buf_view(&b), (const unsigned char *)"abc", 3,
                       "overflow replace: unchanged");
    str89_buf_free(&b, &f.alloc);
}

static void test_insert_overflow(void)
{
    str89_test_fault f;
    str89_buf b;
    long calls;
    int r;

    str89_test_fault_init(&f, 0xC4UL);
    str89_buf_init(&b);
    r = str89_buf_set(&b, &f.alloc, str89_test_cview("abc"));
    str89_test_check_status(r, STR89_OK, "overflow insert: setup");
    calls = f.calls;
    r = str89_buf_insert(&b, &f.alloc, STR89_NPOS, str89_test_cview("x"));
    str89_test_check_status(r, STR89_ERANGE, "overflow insert: ERANGE");
    str89_test_check(f.calls == calls, "overflow insert: no allocation");
    str89_test_view_is(str89_buf_view(&b), (const unsigned char *)"abc", 3,
                       "overflow insert: unchanged");
    str89_buf_free(&b, &f.alloc);
}

static void test_find_overflow(void)
{
    str89_view v;
    size_t at;
    int r;

    r = str89_view_init(&v, (const unsigned char *)"abc", 3);
    str89_test_check_status(r, STR89_OK, "overflow find: view");
    r = str89_view_find(v, v, STR89_NPOS, &at);
    str89_test_check_status(r, STR89_ERANGE, "overflow find: ERANGE");
}

int main(void)
{
    test_add();
    test_grow_cap();
    test_append_overflow();
    test_erase_overflow();
    test_replace_overflow();
    test_insert_overflow();
    test_find_overflow();
    return str89_test_report();
}
