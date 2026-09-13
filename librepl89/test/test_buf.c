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

void test_buf(void)
{
    repl89_buf b;
    char big[128];

    repl89_buf_init(&b);
    rp_check(b.data == NULL, "buf starts empty");
    rp_check(repl89_buf_insert(&b, 0, "abc", 3) == REPL89_OK, "insert abc");
    rp_check(b.len == 3, "len 3");
    rp_check(memcmp(b.data, "abc", 3) == 0, "content abc");
    rp_check(repl89_buf_insert(&b, 1, "XY", 2) == REPL89_OK, "insert mid");
    rp_check(b.len == 5, "len 5");
    rp_check(memcmp(b.data, "aXYbc", 5) == 0, "content aXYbc");
    rp_check(repl89_buf_delete(&b, 1, 2) == REPL89_OK, "delete mid");
    rp_check(b.len == 3, "len 3 after delete");
    rp_check(memcmp(b.data, "abc", 3) == 0, "content abc again");

    memset(big, 'x', sizeof big);
    repl89_mem_set_hooks(fail_alloc, fail_grow, fail_release);
    rp_check(repl89_buf_insert(&b, b.len, big, sizeof big) == REPL89_ENOMEM,
             "reserve failure reported");
    repl89_mem_reset_hooks();
    rp_check(b.len == 3, "len unchanged after ENOMEM");
    rp_check(memcmp(b.data, "abc", 3) == 0, "content unchanged after ENOMEM");

    repl89_buf_clear(&b);
    rp_check(b.len == 0, "clear empties");
    repl89_buf_free(&b);
    rp_check(b.data == NULL, "free releases storage");
}
