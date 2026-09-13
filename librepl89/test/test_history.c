#include <string.h>

#include "test.h"
#include "repl89_internal.h"

static void *fail_alloc(size_t n)
{
    (void)n;
    return NULL;
}

static void *fail_grow(void *p, size_t n)
{
    (void)p;
    (void)n;
    return NULL;
}

static void fail_release(void *p)
{
    (void)p;
}

static int entry_is(const repl89_entry *e, const char *text)
{
    size_t n;

    if (e == NULL) {
        return 0;
    }
    n = strlen(text);
    if (e->len != n) {
        return 0;
    }
    if (n == 0) {
        return 1;
    }
    return memcmp(e->data, text, n) == 0;
}

static void test_storage(void)
{
    repl89_hist h;
    const repl89_entry *e;

    repl89_hist_init(&h, 4);
    rp_check(repl89_hist_at(&h, 0) == NULL, "HI-01 empty");

    rp_check(repl89_hist_add(&h, "one", 3) == REPL89_OK, "HI-02 add");
    rp_check(h.count == 1, "HI-02 count");
    rp_check(entry_is(repl89_hist_at(&h, 0), "one"), "HI-02 entry");

    rp_check(repl89_hist_add(&h, "one", 3) == REPL89_OK, "HI-03 duplicate");
    rp_check(h.count == 2, "HI-03 duplicates retained");

    rp_check(repl89_hist_add(&h, "a\nb\tc", 5) == REPL89_OK, "HI-04 multiline");
    e = repl89_hist_at(&h, 2);
    rp_check(entry_is(e, "a\nb\tc"), "HI-04 stored exactly");

    rp_check(repl89_hist_add(&h, "\xC3\xA9\xF0\x9F\x98\x80", 6) == REPL89_OK,
             "HI-05 utf8");
    e = repl89_hist_at(&h, 3);
    rp_check(entry_is(e, "\xC3\xA9\xF0\x9F\x98\x80"), "HI-05 stored exactly");

    /* limit reached: oldest evicted */
    rp_check(repl89_hist_add(&h, "newest", 6) == REPL89_OK, "HI-06 add at limit");
    rp_check(h.count == 4, "HI-06 count stays at limit");
    rp_check(entry_is(repl89_hist_at(&h, 0), "one"), "HI-06 second kept");
    rp_check(entry_is(repl89_hist_at(&h, 3), "newest"), "HI-06 newest kept");

    repl89_hist_clear(&h);
    rp_check(h.count == 0, "clear empties");
    repl89_hist_free(&h);
}

static void test_limits_and_faults(void)
{
    repl89_hist h;
    repl89_error err;

    repl89_hist_init(&h, 0);
    rp_check(repl89_hist_add(&h, "ignored", 7) == REPL89_OK, "HI-01 disabled add");
    rp_check(h.count == 0, "HI-01 nothing retained");

    repl89_hist_init(&h, 2);
    rp_check(repl89_hist_add(&h, "keep", 4) == REPL89_OK, "HI-14 preload");
    repl89_mem_set_hooks(fail_alloc, fail_grow, fail_release);
    err = repl89_hist_add(&h, "0123456789012345678901234567890123456789", 40);
    rp_check(err == REPL89_ENOMEM, "HI-14 ENOMEM");
    repl89_mem_reset_hooks();
    rp_check(h.count == 1, "HI-14 count unchanged");
    rp_check(entry_is(repl89_hist_at(&h, 0), "keep"), "HI-14 entry unchanged");

    err = repl89_hist_add(&h, "\r", 1);
    rp_check(err == REPL89_EINVAL, "HI-15 CR rejected");
    err = repl89_hist_add(&h, "\xC0\x80", 2);
    rp_check(err == REPL89_EUTF8, "HI-15 malformed rejected");
    rp_check(h.count == 1, "HI-15 rejected atomically");

    repl89_hist_free(&h);
}

static void test_navigation(void)
{
    repl89_hist h;
    const repl89_entry *e;

    repl89_hist_init(&h, 4);
    rp_check(repl89_hist_add(&h, "first", 5) == REPL89_OK, "HI-07 preload a");
    rp_check(repl89_hist_add(&h, "second", 6) == REPL89_OK, "HI-07 preload b");
    repl89_hist_nav_reset(&h);

    e = repl89_hist_nav_prev(&h);
    rp_check(entry_is(e, "second"), "HI-07 newest first");
    e = repl89_hist_nav_prev(&h);
    rp_check(entry_is(e, "first"), "HI-07 older next");
    rp_check(repl89_hist_nav_prev(&h) == NULL, "HI-07 past oldest");

    e = repl89_hist_nav_next(&h);
    rp_check(entry_is(e, "second"), "HI-08 forward");
    rp_check(repl89_hist_nav_next(&h) == NULL, "HI-08 draft reached");

    repl89_hist_free(&h);
}

static void test_independence(void)
{
    repl89_hist a;
    repl89_hist b;

    repl89_hist_init(&a, 4);
    repl89_hist_init(&b, 4);
    rp_check(repl89_hist_add(&a, "only-a", 6) == REPL89_OK, "HI-13 add a");
    rp_check(a.count == 1, "HI-13 a count");
    rp_check(b.count == 0, "HI-13 b count");
    rp_check(repl89_hist_add(&b, "only-b", 6) == REPL89_OK, "HI-13 add b");
    rp_check(entry_is(repl89_hist_at(&a, 0), "only-a"), "HI-13 a entry");
    rp_check(entry_is(repl89_hist_at(&b, 0), "only-b"), "HI-13 b entry");
    repl89_hist_free(&a);
    repl89_hist_free(&b);
}

void test_history(void)
{
    test_storage();
    test_limits_and_faults();
    test_navigation();
    test_independence();
}
