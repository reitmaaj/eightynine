/* str89_test.c - shared test harness for libstr89. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "str89_test.h"
#include "u89.h"

#define FAULT_MAGIC 0x5789ABCDUL

int str89_test_failures = 0;
int str89_test_checks = 0;

void str89_test_check(int cond, const char *what)
{
    str89_test_checks += 1;
    if (cond == 0)
    {
        str89_test_failures += 1;
        fprintf(stderr, "FAIL: %s\n", what);
    }
}

int str89_test_report(void)
{
    if (str89_test_failures != 0)
    {
        fprintf(stderr, "%d/%d checks FAILED\n", str89_test_failures,
                str89_test_checks);
        return 1;
    }
    printf("all %d checks passed\n", str89_test_checks);
    return 0;
}

void str89_test_check_status(int got, int want, const char *what)
{
    str89_test_checks += 1;
    if (got != want)
    {
        str89_test_failures += 1;
        fprintf(stderr, "FAIL: %s (got %d, want %d)\n", what, got, want);
    }
}

str89_view str89_test_cview(const char *s)
{
    str89_view v;

    v.data = (const unsigned char *)s;
    v.len = strlen(s);
    return v;
}

size_t str89_test_put_cp(unsigned char *buf, size_t at, u89_cp cp)
{
    int w;

    w = u89_utf8_encode(cp, buf + at);
    return at + (size_t)w;
}

void str89_test_valid_view(str89_view v, const char *what)
{
    int ok;

    if (v.len != 0)
    {
        str89_test_check(v.data != NULL, what);
        if (v.data == NULL)
        {
            return;
        }
    }
    ok = u89_utf8_valid(v.data, v.len);
    str89_test_check(ok != 0, what);
}

void str89_test_valid_str(const str89 *s, const char *what)
{
    str89_view v;

    v = str89_view_of(s);
    str89_test_valid_view(v, what);
}

void str89_test_valid_buf(const str89_buf *s, const char *what)
{
    str89_view v;

    str89_test_check(s->len <= s->cap, what);
    if (s->cap != 0)
    {
        str89_test_check(s->data != NULL, what);
    }
    if (s->data == NULL)
    {
        str89_test_check(s->cap == 0, what);
    }
    v = str89_buf_view(s);
    str89_test_valid_view(v, what);
}

void str89_test_view_is(str89_view v, const unsigned char *bytes, size_t len,
                        const char *what)
{
    int eq;

    str89_test_check(v.len == len, what);
    if (v.len != len)
    {
        return;
    }
    if (len == 0)
    {
        return;
    }
    eq = memcmp(v.data, bytes, len);
    str89_test_check(eq == 0, what);
}

/* ---- Fault allocator ------------------------------------------------------
 */

struct str89_test_hdr
{
    unsigned long magic;
    size_t size;
    str89_test_fault *owner;
};

static struct str89_test_hdr *fault_hdr(void *p)
{
    unsigned char *base;
    struct str89_test_hdr *h;

    base = (unsigned char *)p;
    h = (struct str89_test_hdr *)(base - sizeof(struct str89_test_hdr));
    return h;
}

static void fault_register(str89_test_fault *f, void *p, size_t size)
{
    int i;

    for (i = 0; i < STR89_TEST_SLOTS; i += 1)
    {
        if (f->slots[i].used == 0)
        {
            f->slots[i].used = 1;
            f->slots[i].p = p;
            f->slots[i].size = size;
            return;
        }
    }
    f->bad_free = 1;
}

static void fault_unregister(str89_test_fault *f, void *p)
{
    int i;

    for (i = 0; i < STR89_TEST_SLOTS; i += 1)
    {
        if (f->slots[i].used != 0)
        {
            if (f->slots[i].p == p)
            {
                f->slots[i].used = 0;
                f->slots[i].p = NULL;
                return;
            }
        }
    }
    f->bad_free = 1;
}

static void *fault_malloc(void *ctx, size_t size)
{
    str89_test_fault *f;
    struct str89_test_hdr *h;
    unsigned char *base;
    void *user;

    f = (str89_test_fault *)ctx;
    f->calls += 1;
    if (f->fail_at != 0)
    {
        if (f->calls == f->fail_at)
        {
            return NULL;
        }
    }
    if (size > STR89_NPOS - sizeof(struct str89_test_hdr))
    {
        return NULL;
    }
    base = (unsigned char *)malloc(sizeof(struct str89_test_hdr) + size);
    if (base == NULL)
    {
        return NULL;
    }
    h = (struct str89_test_hdr *)base;
    h->magic = f->tag ^ FAULT_MAGIC;
    h->size = size;
    h->owner = f;
    user = base + sizeof(struct str89_test_hdr);
    fault_register(f, user, size);
    f->live += 1;
    return user;
}

static void *fault_realloc(void *ctx, void *ptr, size_t size)
{
    str89_test_fault *f;
    struct str89_test_hdr *h;
    struct str89_test_hdr *nh;
    unsigned char *base;
    void *user;

    f = (str89_test_fault *)ctx;
    if (ptr == NULL)
    {
        return fault_malloc(ctx, size);
    }
    f->calls += 1;
    if (f->fail_at != 0)
    {
        if (f->calls == f->fail_at)
        {
            return NULL;
        }
    }
    h = fault_hdr(ptr);
    if (h->magic != (f->tag ^ FAULT_MAGIC))
    {
        f->bad_free = 1;
        return NULL;
    }
    if (size > STR89_NPOS - sizeof(struct str89_test_hdr))
    {
        return NULL;
    }
    base = (unsigned char *)realloc(h, sizeof(struct str89_test_hdr) + size);
    if (base == NULL)
    {
        return NULL;
    }
    fault_unregister(f, ptr);
    nh = (struct str89_test_hdr *)base;
    nh->size = size;
    user = base + sizeof(struct str89_test_hdr);
    fault_register(f, user, size);
    return user;
}

static void fault_free(void *ctx, void *ptr)
{
    str89_test_fault *f;
    struct str89_test_hdr *h;

    f = (str89_test_fault *)ctx;
    if (ptr == NULL)
    {
        return;
    }
    f->frees += 1;
    h = fault_hdr(ptr);
    if (h->magic != (f->tag ^ FAULT_MAGIC))
    {
        f->bad_free = 1;
        return;
    }
    if (h->owner != f)
    {
        f->bad_free = 1;
        return;
    }
    h->magic = 0;
    fault_unregister(f, ptr);
    f->live -= 1;
    free(h);
}

void str89_test_fault_init(str89_test_fault *f, unsigned long tag)
{
    int i;

    f->alloc.ctx = f;
    f->alloc.malloc_fn = fault_malloc;
    f->alloc.realloc_fn = fault_realloc;
    f->alloc.free_fn = fault_free;
    f->fail_at = 0;
    f->calls = 0;
    f->frees = 0;
    f->live = 0;
    f->bad_free = 0;
    f->tag = tag;
    for (i = 0; i < STR89_TEST_SLOTS; i += 1)
    {
        f->slots[i].used = 0;
        f->slots[i].p = NULL;
        f->slots[i].size = 0;
    }
}

void str89_test_fault_fail_next(str89_test_fault *f, long n)
{
    if (n == 0)
    {
        f->fail_at = 0;
        return;
    }
    f->fail_at = f->calls + n;
}

int str89_test_fault_leaked(const str89_test_fault *f)
{
    if (f->live != 0)
    {
        return 1;
    }
    if (f->bad_free != 0)
    {
        return 1;
    }
    return 0;
}

/* ---- PRNG -----------------------------------------------------------------
 */

unsigned long str89_test_rand(unsigned long *state)
{
    unsigned long x;

    x = *state;
    x = x ^ (x << 13);
    x = x ^ (x >> 7);
    x = x ^ (x << 17);
    *state = x;
    return x;
}
