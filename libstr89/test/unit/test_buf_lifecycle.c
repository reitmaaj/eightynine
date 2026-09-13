/* test_buf_lifecycle.c - init, clear, free, and reserve. */

#include "str89_test.h"

static void test_lifecycle(void)
{
    str89_buf b;
    int r;

    str89_buf_init(&b);
    str89_test_check(b.data == NULL, "buf: init data");
    str89_test_check(b.len == 0, "buf: init len");
    str89_test_check(b.cap == 0, "buf: init cap");

    r = str89_buf_append(&b, NULL, str89_test_cview("abc"));
    str89_test_check_status(r, STR89_OK, "buf: append");
    str89_test_check(b.len == 3, "buf: len after append");
    str89_test_check(b.cap >= 3, "buf: cap after append");

    str89_buf_clear(&b);
    str89_test_check(b.len == 0, "buf: clear len");
    str89_test_check(b.data != NULL, "buf: clear retains allocation");
    str89_test_check(b.cap >= 3, "buf: clear retains capacity");

    r = str89_buf_append(&b, NULL, str89_test_cview("xy"));
    str89_test_check_status(r, STR89_OK, "buf: append after clear");
    str89_test_view_is(str89_buf_view(&b), (const unsigned char *)"xy", 2,
                       "buf: content after clear and append");

    str89_buf_free(&b, NULL);
    str89_test_check(b.data == NULL, "buf: free data");
    str89_test_check(b.len == 0, "buf: free len");
    str89_test_check(b.cap == 0, "buf: free cap");

    str89_buf_free(&b, NULL);
    str89_test_check(b.data == NULL, "buf: double free safe");
}

static void test_clear_empty(void)
{
    str89_buf b;

    str89_buf_init(&b);
    str89_buf_clear(&b);
    str89_test_check(b.len == 0, "buf: clear empty");
    str89_test_check(b.data == NULL, "buf: clear empty data");
    str89_buf_free(&b, NULL);
}

static void test_reserve_no_change(void)
{
    str89_buf b;
    size_t cap;
    int r;

    str89_buf_init(&b);
    r = str89_buf_set(&b, NULL, str89_test_cview("hello"));
    str89_test_check_status(r, STR89_OK, "reserve: setup");
    cap = b.cap;

    r = str89_buf_reserve(&b, NULL, 0);
    str89_test_check_status(r, STR89_OK, "reserve: 0");
    r = str89_buf_reserve(&b, NULL, 1);
    str89_test_check_status(r, STR89_OK, "reserve: below len");
    r = str89_buf_reserve(&b, NULL, b.len);
    str89_test_check_status(r, STR89_OK, "reserve: equal len");
    r = str89_buf_reserve(&b, NULL, cap);
    str89_test_check_status(r, STR89_OK, "reserve: equal cap");
    r = str89_buf_reserve(&b, NULL, cap - 1);
    str89_test_check_status(r, STR89_OK, "reserve: below cap");
    str89_test_check(b.cap == cap, "reserve: cap unchanged when sufficient");
    str89_test_view_is(str89_buf_view(&b), (const unsigned char *)"hello", 5,
                       "reserve: content unchanged");

    r = str89_buf_reserve(&b, NULL, cap + 64);
    str89_test_check_status(r, STR89_OK, "reserve: grow");
    str89_test_check(b.cap >= cap + 64, "reserve: cap >= request");
    str89_test_view_is(str89_buf_view(&b), (const unsigned char *)"hello", 5,
                       "reserve: content preserved on grow");
    str89_buf_free(&b, NULL);
}

static void test_reserve_overflow(void)
{
    str89_test_fault f;
    str89_buf b;
    size_t cap;
    int r;

    str89_test_fault_init(&f, 0x61UL);
    str89_buf_init(&b);
    r = str89_buf_set(&b, &f.alloc, str89_test_cview("abc"));
    str89_test_check_status(r, STR89_OK, "reserve: fault setup");
    cap = b.cap;
    f.fail_at = 0;
    r = str89_buf_reserve(&b, &f.alloc, STR89_NPOS);
    str89_test_check_status(r, STR89_ERANGE, "reserve: SIZE_MAX rejected");
    str89_test_check(b.cap == cap, "reserve: overflow leaves cap");
    str89_test_check(b.len == 3, "reserve: overflow leaves len");
    str89_test_view_is(str89_buf_view(&b), (const unsigned char *)"abc", 3,
                       "reserve: overflow leaves content");
    str89_buf_free(&b, &f.alloc);
    str89_test_check(str89_test_fault_leaked(&f) == 0, "reserve: no leak");
}

static void test_reserve_failure(void)
{
    str89_test_fault f;
    str89_buf b;
    size_t cap;
    size_t len;
    int r;

    str89_test_fault_init(&f, 0x62UL);
    str89_buf_init(&b);
    r = str89_buf_set(&b, &f.alloc, str89_test_cview("abc"));
    str89_test_check_status(r, STR89_OK, "reserve: failure setup");
    cap = b.cap;
    len = b.len;
    str89_test_fault_fail_next(&f, 1);
    r = str89_buf_reserve(&b, &f.alloc, cap + 1);
    str89_test_check_status(r, STR89_ENOMEM, "reserve: ENOMEM");
    str89_test_check(b.cap == cap, "reserve: failure leaves cap");
    str89_test_check(b.len == len, "reserve: failure leaves len");
    str89_test_view_is(str89_buf_view(&b), (const unsigned char *)"abc", 3,
                       "reserve: failure leaves content");
    str89_buf_free(&b, &f.alloc);
    str89_test_check(str89_test_fault_leaked(&f) == 0,
                     "reserve: failure no leak");
}

int main(void)
{
    test_lifecycle();
    test_clear_empty();
    test_reserve_no_change();
    test_reserve_overflow();
    test_reserve_failure();
    return str89_test_report();
}
