/* test_take.c - zero-copy ownership transfer. */

#include "str89_test.h"

static void test_take_empty(void)
{
    str89_test_fault f;
    str89 s;
    str89_buf b;
    int r;

    str89_test_fault_init(&f, 0x71UL);
    str89_init(&s);
    str89_buf_init(&b);
    r = str89_take(&s, &b);
    str89_test_check_status(r, STR89_OK, "take: empty");
    str89_test_check(s.data == NULL, "take: empty data");
    str89_test_check(s.len == 0, "take: empty len");
    str89_test_check(b.data == NULL, "take: empty source data");
    str89_test_check(b.len == 0, "take: empty source len");
    str89_test_check(b.cap == 0, "take: empty source cap");
    str89_test_check(f.calls == 0, "take: no allocation");
    str89_free(&s, &f.alloc);
}

static void test_take_nonempty(void)
{
    str89_test_fault f;
    str89 s;
    str89_buf b;
    unsigned char *old;
    size_t oldlen;
    long calls;
    int r;

    str89_test_fault_init(&f, 0x72UL);
    str89_init(&s);
    str89_buf_init(&b);
    r = str89_buf_append(&b, &f.alloc, str89_test_cview("hello"));
    str89_test_check_status(r, STR89_OK, "take: setup");
    old = b.data;
    oldlen = b.len;
    calls = f.calls;
    str89_test_fault_fail_next(&f, 1);
    r = str89_take(&s, &b);
    str89_test_check_status(r, STR89_OK, "take: transfer");
    str89_test_check(s.data == old, "take: exact pointer transfer");
    str89_test_check(s.len == oldlen, "take: length transfer");
    str89_test_check(b.data == NULL, "take: source data reset");
    str89_test_check(b.len == 0, "take: source len reset");
    str89_test_check(b.cap == 0, "take: source cap reset");
    str89_test_check(f.calls == calls, "take: zero allocation calls");
    str89_test_valid_str(&s, "take: valid");
    str89_free(&s, &f.alloc);
    str89_test_check(str89_test_fault_leaked(&f) == 0, "take: no leak");
}

static void test_take_nul_and_large(void)
{
    static const unsigned char bytes[] = {'a', 0x00, 0xC3, 0xA9, 'z'};
    str89_test_fault f;
    str89 s;
    str89_buf b;
    int r;

    str89_test_fault_init(&f, 0x73UL);
    str89_init(&s);
    str89_buf_init(&b);
    r = str89_buf_set(&b, &f.alloc, str89_test_cview("x"));
    str89_test_check_status(r, STR89_OK, "take: nul setup");
    r = str89_buf_erase(&b, 0, 1);
    str89_test_check_status(r, STR89_OK, "take: nul clear");
    r = str89_buf_append(&b, &f.alloc, str89_test_cview(""));
    str89_test_check_status(r, STR89_OK, "take: nul append");
    {
        str89_view v;
        int vr;

        vr = str89_view_init(&v, bytes, sizeof(bytes));
        str89_test_check_status(vr, STR89_OK, "take: nul view");
        r = str89_buf_set(&b, &f.alloc, v);
        str89_test_check_status(r, STR89_OK, "take: nul set");
    }
    r = str89_take(&s, &b);
    str89_test_check_status(r, STR89_OK, "take: nul take");
    str89_test_view_is(str89_view_of(&s), bytes, sizeof(bytes),
                       "take: nul bytes");
    str89_free(&s, &f.alloc);
    str89_test_check(str89_test_fault_leaked(&f) == 0, "take: nul no leak");
}

static void test_take_nonempty_destination(void)
{
    str89 s;
    str89_buf b;
    str89_view v;
    unsigned char *old;
    int r;

    str89_init(&s);
    str89_buf_init(&b);
    r = str89_from_view(&s, NULL, str89_test_cview("dest"));
    str89_test_check_status(r, STR89_OK, "take: dest setup");
    old = s.data;
    r = str89_buf_append(&b, NULL, str89_test_cview("src"));
    str89_test_check_status(r, STR89_OK, "take: source setup");
    r = str89_take(&s, &b);
    str89_test_check_status(r, STR89_EINVAL, "take: non-empty destination");
    str89_test_check(s.data == old, "take: destination unchanged");
    v = str89_view_of(&s);
    str89_test_view_is(v, (const unsigned char *)"dest", 4,
                       "take: destination content");
    str89_test_check(b.len == 3, "take: source unchanged");
    str89_free(&s, NULL);
    str89_buf_free(&b, NULL);
}

int main(void)
{
    test_take_empty();
    test_take_nonempty();
    test_take_nul_and_large();
    test_take_nonempty_destination();
    return str89_test_report();
}
