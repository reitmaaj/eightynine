/* test_model_invalid.c - randomized invalid operations must preserve state. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "str89_test.h"

#define INV_MAX 512
#define INV_POOL_N 8
#define INV_DEFAULT_SEEDS 300
#define INV_OPS 60

static const u89_cp pool[INV_POOL_N] = {0x0000, 0x0041, 0x0080,  0x07FF,
                                        0x20AC, 0xFFFF, 0x10000, 0x1F600};

static size_t gen_string(unsigned long *st, unsigned char *out, size_t cap)
{
    size_t n;
    size_t count;
    size_t i;
    u89_cp cp;
    int w;

    n = 0;
    count = (size_t)(str89_test_rand(st) % 5);
    for (i = 0; i < count; i += 1)
    {
        if (n + 4 > cap)
        {
            break;
        }
        cp = pool[str89_test_rand(st) % INV_POOL_N];
        w = u89_utf8_encode(cp, out + n);
        n += (size_t)w;
    }
    return n;
}

static str89_view view_of(const unsigned char *p, size_t n)
{
    str89_view v;
    int r;

    r = str89_view_init(&v, p, n);
    str89_test_check_status(r, STR89_OK, "invalid: view");
    return v;
}

static size_t continuation_at(const unsigned char *b, size_t len)
{
    size_t i;

    for (i = 0; i < len; i += 1)
    {
        if ((b[i] & 0xC0) == 0x80)
        {
            return i;
        }
    }
    return STR89_NPOS;
}

static void check_unchanged(const str89_buf *b, const unsigned char *snap,
                            size_t len, size_t cap, const char *what)
{
    str89_test_check(b->len == len, what);
    str89_test_check(b->cap == cap, what);
    str89_test_view_is(str89_buf_view(b), snap, len, what);
}

static void run_one(unsigned long seed)
{
    str89_test_fault f;
    str89_buf b;
    unsigned char tmp[64];
    unsigned char snap[INV_MAX];
    size_t n;
    size_t len;
    size_t cap;
    size_t at;
    size_t i;
    unsigned long st;
    int op;
    int r;
    int step;

    str89_test_fault_init(&f, 0xE1UL);
    str89_buf_init(&b);
    st = seed | 1UL;
    n = gen_string(&st, tmp, sizeof(tmp));
    r = str89_buf_set(&b, &f.alloc, view_of(tmp, n));
    str89_test_check_status(r, STR89_OK, "invalid: setup");

    for (step = 0; step < INV_OPS; step += 1)
    {
        len = b.len;
        cap = b.cap;
        if (len != 0)
        {
            memcpy(snap, b.data, len);
        }
        at = continuation_at(b.data, len);
        op = (int)(str89_test_rand(&st) % 8);
        if (op == 0)
        {
            if (at == STR89_NPOS)
            {
                at = len + 1;
            }
            r = str89_buf_insert(&b, &f.alloc, at, view_of(tmp, n));
            if (at > len)
            {
                str89_test_check_status(r, STR89_ERANGE,
                                        "invalid: insert range");
            }
            else
            {
                str89_test_check_status(r, STR89_EBOUND,
                                        "invalid: insert boundary");
            }
            check_unchanged(&b, snap, len, cap, "invalid: insert state");
        }
        else if (op == 1)
        {
            if (at == STR89_NPOS)
            {
                at = len + 1;
            }
            r = str89_buf_erase(&b, at, 0);
            if (at > len)
            {
                str89_test_check_status(r, STR89_ERANGE,
                                        "invalid: erase range");
            }
            else
            {
                str89_test_check_status(r, STR89_EBOUND,
                                        "invalid: erase boundary");
            }
            check_unchanged(&b, snap, len, cap, "invalid: erase state");
        }
        else if (op == 2)
        {
            if (at == STR89_NPOS || at == 0)
            {
                at = STR89_NPOS;
            }
            else
            {
                r = str89_buf_replace(&b, &f.alloc, 0, at, view_of(tmp, n));
                str89_test_check_status(r, STR89_EBOUND,
                                        "invalid: replace boundary");
                check_unchanged(&b, snap, len, cap, "invalid: replace state");
            }
        }
        else if (op == 3)
        {
            r = str89_buf_insert(&b, &f.alloc, len + 1, view_of(tmp, n));
            str89_test_check_status(r, STR89_ERANGE, "invalid: offset");
            check_unchanged(&b, snap, len, cap, "invalid: offset state");
        }
        else if (op == 4)
        {
            str89_view huge;

            huge.data = b.data;
            huge.len = STR89_NPOS;
            r = str89_buf_append(&b, &f.alloc, huge);
            str89_test_check_status(r, STR89_ERANGE, "invalid: huge append");
            check_unchanged(&b, snap, len, cap, "invalid: huge state");
        }
        else if (op == 5)
        {
            r = str89_buf_append_cp(&b, &f.alloc, 0xD800);
            str89_test_check_status(r, STR89_EINVAL, "invalid: surrogate");
            check_unchanged(&b, snap, len, cap, "invalid: surrogate state");
        }
        else if (op == 6)
        {
            str89_test_fault_fail_next(&f, 1);
            r = str89_buf_reserve(&b, &f.alloc, cap + 1);
            str89_test_check_status(r, STR89_ENOMEM, "invalid: ENOMEM");
            check_unchanged(&b, snap, len, cap, "invalid: ENOMEM state");
        }
        else
        {
            str89_view v;

            v = str89_buf_view(&b);
            if (at != STR89_NPOS)
            {
                r = str89_view_find(v, view_of(tmp, n), at, &i);
                str89_test_check_status(r, STR89_EBOUND,
                                        "invalid: find boundary");
            }
        }
    }
    str89_buf_free(&b, &f.alloc);
    str89_test_check(str89_test_fault_leaked(&f) == 0, "invalid: no leak");
}

int main(void)
{
    const char *env;
    long seeds;
    long i;

    seeds = INV_DEFAULT_SEEDS;
    env = getenv("STR89_MODEL_SEEDS");
    if (env != NULL)
    {
        seeds = strtol(env, NULL, 10);
    }
    for (i = 1; i <= seeds; i += 1)
    {
        run_one((unsigned long)i * 40503UL);
    }
    return str89_test_report();
}
