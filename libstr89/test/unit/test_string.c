/* test_string.c - owning string lifecycle, copy, and free. */

#include <string.h>

#include "str89_test.h"

static void test_init(void)
{
    str89 s;

    str89_init(&s);
    str89_test_check(s.data == NULL, "string: init data");
    str89_test_check(s.len == 0, "string: init len");
}

static void test_from_view_basic(void)
{
    static const unsigned char bytes[] = {'a', 0x00, 0xC3, 0xA9, 'z'};
    str89 s;
    str89_view v;
    int r;

    str89_init(&s);
    r = str89_view_init(&v, bytes, sizeof(bytes));
    str89_test_check_status(r, STR89_OK, "string: view accepted");
    r = str89_from_view(&s, NULL, v);
    str89_test_check_status(r, STR89_OK, "string: from_view");
    str89_test_valid_str(&s, "string: valid");
    str89_test_check(s.len == sizeof(bytes), "string: length");
    str89_test_check(s.data != bytes, "string: owns distinct storage");
    str89_test_view_is(str89_view_of(&s), bytes, sizeof(bytes),
                       "string: bytes");
    str89_free(&s, NULL);
    str89_test_check(s.data == NULL, "string: free resets data");
    str89_test_check(s.len == 0, "string: free resets len");
}

static void test_from_view_empty(void)
{
    str89_test_fault f;
    str89 s;
    str89_view v;
    int r;

    str89_test_fault_init(&f, 0x52UL);
    str89_init(&s);
    r = str89_view_init(&v, NULL, 0);
    str89_test_check_status(r, STR89_OK, "string: empty view");
    r = str89_from_view(&s, &f.alloc, v);
    str89_test_check_status(r, STR89_OK, "string: empty from_view");
    str89_test_check(s.data == NULL, "string: empty data");
    str89_test_check(s.len == 0, "string: empty len");
    str89_test_check(f.calls == 0, "string: empty performs no allocation");
    str89_free(&s, &f.alloc);
}

static void test_from_view_large(void)
{
    unsigned char bytes[4096];
    str89 s;
    str89_view v;
    size_t i;
    int r;

    for (i = 0; i < sizeof(bytes); i += 1)
    {
        bytes[i] = (unsigned char)('a' + (i % 26));
    }
    str89_init(&s);
    r = str89_view_init(&v, bytes, sizeof(bytes));
    str89_test_check_status(r, STR89_OK, "string: large view");
    r = str89_from_view(&s, NULL, v);
    str89_test_check_status(r, STR89_OK, "string: large from_view");
    str89_test_valid_str(&s, "string: large valid");
    str89_test_view_is(str89_view_of(&s), bytes, sizeof(bytes),
                       "string: large bytes");
    str89_free(&s, NULL);
}

static void test_from_view_independent(void)
{
    str89 s;
    str89_view v;
    int r;

    str89_init(&s);
    {
        str89_buf b;

        str89_buf_init(&b);
        r = str89_buf_set(&b, NULL, str89_test_cview("abc"));
        str89_test_check_status(r, STR89_OK, "string: setup buffer");
        v = str89_buf_view(&b);
        r = str89_from_view(&s, NULL, v);
        str89_test_check_status(r, STR89_OK, "string: copy from buffer");
        r = str89_buf_append(&b, NULL, str89_test_cview("def"));
        str89_test_check_status(r, STR89_OK, "string: mutate source");
        str89_test_view_is(str89_view_of(&s), (const unsigned char *)"abc", 3,
                           "string: destination unchanged");
        str89_buf_free(&b, NULL);
    }
    str89_free(&s, NULL);
}

static void test_copy(void)
{
    str89 src;
    str89 dst;
    str89_view v;
    int r;

    str89_init(&src);
    str89_init(&dst);
    r = str89_view_init(&v, (const unsigned char *)"copy me", 7);
    str89_test_check_status(r, STR89_OK, "string: copy view");
    r = str89_from_view(&src, NULL, v);
    str89_test_check_status(r, STR89_OK, "string: copy source");

    r = str89_copy(&dst, NULL, &src);
    str89_test_check_status(r, STR89_OK, "string: copy");
    str89_test_check(dst.data != src.data, "string: copy distinct storage");
    str89_test_check(
        str89_view_equal(str89_view_of(&dst), str89_view_of(&src)) != 0,
        "string: copy equal");

    r = str89_copy(&src, NULL, &src);
    str89_test_check_status(r, STR89_OK, "string: self copy no-op");
    str89_test_check(str89_view_equal(str89_view_of(&src), v) != 0,
                     "string: self copy preserved");

    str89_free(&src, NULL);
    str89_free(&dst, NULL);
}

static void test_copy_empty(void)
{
    str89 src;
    str89 dst;
    int r;

    str89_init(&src);
    str89_init(&dst);
    r = str89_copy(&dst, NULL, &src);
    str89_test_check_status(r, STR89_OK, "string: copy empty");
    str89_test_check(dst.data == NULL, "string: copy empty data");
    str89_free(&src, NULL);
    str89_free(&dst, NULL);
}

static void test_custom_allocator(void)
{
    str89_test_fault f;
    str89 s;
    str89_view v;
    int r;

    str89_test_fault_init(&f, 0x51UL);
    str89_init(&s);
    r = str89_view_init(&v, (const unsigned char *)"hello", 5);
    str89_test_check_status(r, STR89_OK, "string: allocator view");
    r = str89_from_view(&s, &f.alloc, v);
    str89_test_check_status(r, STR89_OK, "string: allocator from_view");
    str89_test_check(f.live == 1, "string: one live block");
    str89_free(&s, &f.alloc);
    str89_test_check(f.live == 0, "string: block freed");
    str89_test_check(str89_test_fault_leaked(&f) == 0, "string: no leak");
}

static void test_free_idempotent(void)
{
    str89 s;
    int r;

    str89_init(&s);
    r = str89_from_view(&s, NULL, str89_test_cview("x"));
    str89_test_check_status(r, STR89_OK, "string: idempotent setup");
    str89_free(&s, NULL);
    str89_free(&s, NULL);
    str89_test_check(s.data == NULL, "string: double free safe");
    str89_test_check(s.len == 0, "string: double free len");
}

int main(void)
{
    test_init();
    test_from_view_basic();
    test_from_view_empty();
    test_from_view_large();
    test_from_view_independent();
    test_copy();
    test_copy_empty();
    test_custom_allocator();
    test_free_idempotent();
    return str89_test_report();
}
