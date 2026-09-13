/* hm_print.c - deterministic type and scheme rendering. */
#include <hm.h>

#include "hm_internal.h"

#include <stdlib.h>
#include <string.h>

struct hm_i_render
{
    char *mem;
    size_t used;
    size_t cap;
    hm_type **seen;
    size_t seen_count;
    size_t seen_cap;
    int oom;
};

static int hm_i_rb_reserve(struct hm_i_render *r, size_t extra)
{
    size_t nc;
    void *raw;
    if (r->used + extra <= r->cap)
    {
        return 1;
    }
    nc = r->used + extra;
    raw = realloc(r->mem, nc);
    if (raw == NULL)
    {
        r->oom = 1;
        return 0;
    }
    r->mem = raw;
    r->cap = nc;
    return 1;
}

static void hm_i_rb_putc(struct hm_i_render *r, char c)
{
    int ok;
    ok = hm_i_rb_reserve(r, 1);
    if (ok == 0)
    {
        return;
    }
    r->mem[r->used] = c;
    ++r->used;
}

static void hm_i_rb_putn(struct hm_i_render *r, const char *s, size_t n)
{
    int ok;
    ok = hm_i_rb_reserve(r, n);
    if (ok == 0)
    {
        return;
    }
    memcpy(r->mem + r->used, s, n);
    r->used = r->used + n;
}

static void hm_i_rb_puts(struct hm_i_render *r, const char *s)
{
    size_t n;
    n = strlen(s);
    hm_i_rb_putn(r, s, n);
}

static void hm_i_rb_ulong(struct hm_i_render *r, unsigned long v)
{
    unsigned long q;
    char d;
    q = v / 10;
    if (q != 0)
    {
        hm_i_rb_ulong(r, q);
    }
    d = (char)('0' + (int)(v % 10));
    hm_i_rb_putc(r, d);
}

static size_t hm_i_next_cap(size_t cap)
{
    if (cap == 0)
    {
        return 8;
    }
    return cap * 2;
}

static hm_type **hm_i_grow_seen(struct hm_i_render *r)
{
    size_t nc;
    size_t bytes;
    void *raw;
    nc = hm_i_next_cap(r->seen_cap);
    bytes = nc * sizeof(hm_type *);
    raw = realloc(r->seen, bytes);
    if (raw == NULL)
    {
        return NULL;
    }
    r->seen = raw;
    r->seen_cap = nc;
    return r->seen;
}

static int hm_i_seen_find(const struct hm_i_render *r, hm_type *var,
                          size_t *out)
{
    size_t i;
    for (i = 0; i < r->seen_count; ++i)
    {
        if (r->seen[i] == var)
        {
            *out = i;
            return 1;
        }
    }
    return 0;
}

static size_t hm_i_seen_index(struct hm_i_render *r, hm_type *var)
{
    size_t idx;
    hm_type **grown;
    int present;
    present = hm_i_seen_find(r, var, &idx);
    if (present != 0)
    {
        return idx;
    }
    if (r->seen_count == r->seen_cap)
    {
        grown = hm_i_grow_seen(r);
        if (grown == NULL)
        {
            r->oom = 1;
            return 0;
        }
    }
    idx = r->seen_count;
    r->seen[idx] = var;
    ++r->seen_count;
    return idx;
}

static void hm_i_name_index(struct hm_i_render *r, size_t index)
{
    size_t letter;
    unsigned long round;
    letter = index % 26;
    round = index / 26;
    hm_i_rb_putc(r, (char)('a' + (int)letter));
    if (round != 0)
    {
        hm_i_rb_ulong(r, round);
    }
}

static void hm_i_render_var(struct hm_i_render *r, hm_type *var)
{
    size_t idx;
    idx = hm_i_seen_index(r, var);
    hm_i_rb_putc(r, '\'');
    hm_i_name_index(r, idx);
}

static void hm_i_render_type(struct hm_i_render *r, hm_type *node)
{
    hm_type *p;
    size_t arity;
    size_t i;
    p = hm_type_prune(node);
    if (p->kind == HM_TYPE_VAR)
    {
        hm_i_render_var(r, p);
        return;
    }
    hm_i_rb_puts(r, p->u.con.name);
    arity = p->u.con.arity;
    if (arity != 0)
    {
        hm_i_rb_putc(r, '(');
        for (i = 0; i < arity; ++i)
        {
            if (i != 0)
            {
                hm_i_rb_putc(r, ',');
            }
            hm_i_render_type(r, p->u.con.args[i]);
        }
        hm_i_rb_putc(r, ')');
    }
}

static void hm_i_render_scheme(struct hm_i_render *r, const hm_scheme *scheme)
{
    size_t i;
    if (scheme->count != 0)
    {
        hm_i_rb_puts(r, "forall ");
        for (i = 0; i < scheme->count; ++i)
        {
            if (i != 0)
            {
                hm_i_rb_putc(r, ' ');
            }
            hm_i_render_var(r, scheme->vars[i]);
        }
        hm_i_rb_puts(r, ". ");
    }
    hm_i_render_type(r, scheme->body);
}

static void hm_i_render_destroy(struct hm_i_render *r)
{
    free(r->mem);
    free(r->seen);
}

static hm_status hm_i_render_emit(struct hm_i_render *r, hm_write_fn write_fn,
                                  void *userdata)
{
    hm_status status;
    int done;
    if (r->oom != 0)
    {
        hm_i_render_destroy(r);
        return HM_ERROR_NOMEM;
    }
    done = write_fn(userdata, r->mem, r->used);
    if (done == 0)
    {
        status = HM_ERROR_INTERNAL;
    }
    else
    {
        status = HM_OK;
    }
    hm_i_render_destroy(r);
    return status;
}

hm_status hm_type_write(hm_type *type, hm_write_fn write_fn, void *userdata)
{
    struct hm_i_render r;
    hm_status status;
    r.mem = NULL;
    r.used = 0;
    r.cap = 0;
    r.seen = NULL;
    r.seen_count = 0;
    r.seen_cap = 0;
    r.oom = 0;
    hm_i_render_type(&r, type);
    status = hm_i_render_emit(&r, write_fn, userdata);
    return status;
}

hm_status hm_scheme_write(const hm_scheme *scheme, hm_write_fn write_fn,
                          void *userdata)
{
    struct hm_i_render r;
    hm_status status;
    r.mem = NULL;
    r.used = 0;
    r.cap = 0;
    r.seen = NULL;
    r.seen_count = 0;
    r.seen_cap = 0;
    r.oom = 0;
    hm_i_render_scheme(&r, scheme);
    status = hm_i_render_emit(&r, write_fn, userdata);
    return status;
}
