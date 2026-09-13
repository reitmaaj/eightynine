/* test_model.c - randomized valid-operation sequences against a byte model. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "str89_test.h"

#define MODEL_MAX 4096
#define MODEL_POOL_N 15
#define MODEL_DEFAULT_SEEDS 500
#define MODEL_OPS 100

typedef struct model
{
    unsigned char b[MODEL_MAX];
    size_t len;
} model;

static const u89_cp pool[MODEL_POOL_N] = {
    0x0000, 0x0041,  0x007F,   0x0080, 0x07FF, 0x0800, 0xD7FF, 0xE000,
    0xFFFF, 0x10000, 0x10FFFF, 0x20AC, 0x0065, 0x0301, 0x1F600};

static size_t gen_string(unsigned long *st, unsigned char *out, size_t cap)
{
    size_t n;
    size_t count;
    size_t i;
    u89_cp cp;
    int w;

    n = 0;
    count = (size_t)(str89_test_rand(st) % 6);
    for (i = 0; i < count; i += 1)
    {
        if (n + 4 > cap)
        {
            break;
        }
        cp = pool[str89_test_rand(st) % MODEL_POOL_N];
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
    str89_test_check_status(r, STR89_OK, "model: view");
    return v;
}

static int is_boundary(const model *m, size_t off)
{
    u89_status st;

    if (off == 0)
    {
        return 1;
    }
    if (off == m->len)
    {
        return 1;
    }
    if (off > m->len)
    {
        return 0;
    }
    st = u89_utf8_prev(m->b, m->len, off, NULL, NULL);
    return (st == U89_OK);
}

static size_t pick_boundary(unsigned long *st, const model *m)
{
    size_t off;
    size_t i;

    off = (size_t)(str89_test_rand(st) % (m->len + 1));
    for (i = 0; i <= m->len; i += 1)
    {
        if (off + i <= m->len)
        {
            if (is_boundary(m, off + i))
            {
                return off + i;
            }
        }
    }
    return 0;
}

static size_t pick_end(unsigned long *st, const model *m, size_t start)
{
    size_t off;

    off = start + (size_t)(str89_test_rand(st) % (m->len - start + 1));
    while (off <= m->len)
    {
        if (is_boundary(m, off))
        {
            return off;
        }
        off += 1;
    }
    return m->len;
}

static void model_insert(model *m, size_t off, const unsigned char *src,
                         size_t n)
{
    memmove(m->b + off + n, m->b + off, m->len - off);
    memcpy(m->b + off, src, n);
    m->len += n;
}

static void model_erase(model *m, size_t off, size_t end)
{
    memmove(m->b + off, m->b + end, m->len - end);
    m->len -= end - off;
}

static void check_state(const str89_buf *b, const model *m, unsigned long seed,
                        int step)
{
    str89_view v;
    int eq;

    v = str89_buf_view(b);
    if (v.len != m->len)
    {
        str89_test_check(0, "model: length mismatch");
        fprintf(stderr, "  seed %lu step %d: got %lu want %lu\n", seed, step,
                (unsigned long)v.len, (unsigned long)m->len);
        return;
    }
    if (m->len != 0)
    {
        eq = memcmp(v.data, m->b, m->len);
        if (eq != 0)
        {
            str89_test_check(0, "model: byte mismatch");
            fprintf(stderr, "  seed %lu step %d\n", seed, step);
            return;
        }
    }
    str89_test_valid_buf(b, "model: valid");
}

static void run_one(unsigned long seed)
{
    str89_test_fault f;
    str89_buf b;
    model m;
    unsigned char tmp[64];
    size_t n;
    size_t off;
    size_t end;
    size_t cap;
    u89_cp cp;
    unsigned long st;
    int op;
    int r;
    int step;
    int w;

    str89_test_fault_init(&f, 0xD1UL);
    str89_buf_init(&b);
    m.len = 0;
    st = seed | 1UL;

    for (step = 0; step < MODEL_OPS; step += 1)
    {
        op = (int)(str89_test_rand(&st) % 10);
        if (op == 0)
        {
            n = gen_string(&st, tmp, sizeof(tmp));
            m.len = 0;
            model_insert(&m, 0, tmp, n);
            r = str89_buf_set(&b, &f.alloc, view_of(tmp, n));
            str89_test_check_status(r, STR89_OK, "model: set");
        }
        else if (op == 1)
        {
            n = gen_string(&st, tmp, sizeof(tmp));
            model_insert(&m, m.len, tmp, n);
            r = str89_buf_append(&b, &f.alloc, view_of(tmp, n));
            str89_test_check_status(r, STR89_OK, "model: append");
        }
        else if (op == 2)
        {
            n = gen_string(&st, tmp, sizeof(tmp));
            off = pick_boundary(&st, &m);
            model_insert(&m, off, tmp, n);
            r = str89_buf_insert(&b, &f.alloc, off, view_of(tmp, n));
            str89_test_check_status(r, STR89_OK, "model: insert");
        }
        else if (op == 3)
        {
            off = pick_boundary(&st, &m);
            end = pick_end(&st, &m, off);
            model_erase(&m, off, end);
            r = str89_buf_erase(&b, off, end - off);
            str89_test_check_status(r, STR89_OK, "model: erase");
        }
        else if (op == 4)
        {
            n = gen_string(&st, tmp, sizeof(tmp));
            off = pick_boundary(&st, &m);
            end = pick_end(&st, &m, off);
            model_erase(&m, off, end);
            model_insert(&m, off, tmp, n);
            r = str89_buf_replace(&b, &f.alloc, off, end - off,
                                  view_of(tmp, n));
            str89_test_check_status(r, STR89_OK, "model: replace");
        }
        else if (op == 5)
        {
            cp = pool[str89_test_rand(&st) % MODEL_POOL_N];
            w = u89_utf8_encode(cp, tmp);
            model_insert(&m, m.len, tmp, (size_t)w);
            r = str89_buf_append_cp(&b, &f.alloc, cp);
            str89_test_check_status(r, STR89_OK, "model: append_cp");
        }
        else if (op == 6)
        {
            cp = pool[str89_test_rand(&st) % MODEL_POOL_N];
            w = u89_utf8_encode(cp, tmp);
            off = pick_boundary(&st, &m);
            model_insert(&m, off, tmp, (size_t)w);
            r = str89_buf_insert_cp(&b, &f.alloc, off, cp);
            str89_test_check_status(r, STR89_OK, "model: insert_cp");
        }
        else if (op == 7)
        {
            m.len = 0;
            str89_buf_clear(&b);
        }
        else if (op == 8)
        {
            cap = (size_t)(str89_test_rand(&st) % 128);
            r = str89_buf_reserve(&b, &f.alloc, cap);
            str89_test_check_status(r, STR89_OK, "model: reserve");
            str89_test_check(b.cap >= cap, "model: reserve capacity");
        }
        else
        {
            str89 s;

            str89_init(&s);
            r = str89_take(&s, &b);
            str89_test_check_status(r, STR89_OK, "model: take");
            str89_test_check(s.len == m.len, "model: take length");
            r = str89_buf_set(&b, &f.alloc, str89_view_of(&s));
            str89_test_check_status(r, STR89_OK, "model: take restore");
            str89_free(&s, &f.alloc);
        }
        check_state(&b, &m, seed, step);
    }
    str89_buf_free(&b, &f.alloc);
    str89_test_check(str89_test_fault_leaked(&f) == 0, "model: no leak");
}

int main(void)
{
    const char *env;
    long seeds;
    long i;

    seeds = MODEL_DEFAULT_SEEDS;
    env = getenv("STR89_MODEL_SEEDS");
    if (env != NULL)
    {
        seeds = strtol(env, NULL, 10);
    }
    for (i = 1; i <= seeds; i += 1)
    {
        run_one((unsigned long)i * 2654435761UL);
    }
    return str89_test_report();
}
