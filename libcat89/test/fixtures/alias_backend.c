/* cat89_alias_backend.c - alias-object category fixture (F04). */

#include "alias_backend.h"
#include <cat89_internal.h>

struct alias_mor
{
    unsigned long refs;
    int id;
    const cat89_obj *dom;
    const cat89_obj *cod;
    struct alias_mor *next_live;
};

struct cat89_alias
{
    cat89_allocator allocator;
    cat89_category *category;
    unsigned char tok[6];
    struct alias_mor *live_mors;
};

struct alias_ctx
{
    struct cat89_alias *alias;
};

struct alias_row
{
    int g;
    int f;
    int r;
};

static const struct alias_row alias_rows[] = {
    {0, 0, 0}, {1, 1, 1}, {2, 2, 2}, {1, 3, 3}, {3, 0, 3},
    {2, 4, 4}, {4, 1, 4}, {2, 5, 5}, {5, 0, 5}, {4, 3, 5}};

static int alias_obj_index(const struct cat89_alias *alias,
                           const cat89_obj *obj)
{
    const unsigned char *p = (const unsigned char *)(const void *)obj;
    int i;

    for (i = 0; i < 6; i = i + 1)
    {
        if (p == &alias->tok[i])
        {
            return i;
        }
    }
    return -1;
}

static int alias_obj_same_cb(void *ctx, const cat89_obj *a, const cat89_obj *b)
{
    struct alias_ctx *cx = ctx;
    int ia;
    int ib;

    ia = alias_obj_index(cx->alias, a);
    ib = alias_obj_index(cx->alias, b);
    if (ia < 0)
    {
        return 0;
    }
    if (ib < 0)
    {
        return 0;
    }
    return ia / 2 == ib / 2;
}

static const cat89_obj *alias_obj_ptr(struct cat89_alias *alias, int i)
{
    return (const cat89_obj *)(const void *)&alias->tok[i];
}

static cat89_status alias_mor_make(struct cat89_alias *alias, int id,
                                   const cat89_obj *dom, const cat89_obj *cod,
                                   cat89_mor **out_mor)
{
    struct alias_mor *m;

    m = cat89_alloc(&alias->allocator, sizeof(*m));
    if (m == NULL)
    {
        return CAT89_NOMEM;
    }
    m->refs = 1;
    m->id = id;
    m->dom = dom;
    m->cod = cod;
    m->next_live = alias->live_mors;
    alias->live_mors = m;
    *out_mor = (cat89_mor *)m;
    return CAT89_OK;
}

static int alias_result_id(int g, int f)
{
    unsigned long i;

    for (i = 0; i < sizeof(alias_rows) / sizeof(alias_rows[0]); i = i + 1)
    {
        if (alias_rows[i].g == g)
        {
            if (alias_rows[i].f == f)
            {
                return alias_rows[i].r;
            }
        }
    }
    return -1;
}

static cat89_status alias_dom_cb(void *ctx, const cat89_mor *mor,
                                 const cat89_obj **out_obj)
{
    const struct alias_mor *m;
    (void)ctx;
    m = (const struct alias_mor *)(const void *)mor;
    *out_obj = m->dom;
    return CAT89_OK;
}

static cat89_status alias_cod_cb(void *ctx, const cat89_mor *mor,
                                 const cat89_obj **out_obj)
{
    const struct alias_mor *m;
    (void)ctx;
    m = (const struct alias_mor *)(const void *)mor;
    *out_obj = m->cod;
    return CAT89_OK;
}

static cat89_status alias_identity_cb(void *ctx, const cat89_obj *obj,
                                      cat89_mor **out_mor)
{
    struct alias_ctx *cx = ctx;
    int i;
    int id;

    i = alias_obj_index(cx->alias, obj);
    if (i < 0)
    {
        return CAT89_INVALID;
    }
    id = i / 2;
    return alias_mor_make(cx->alias, id, obj, obj, out_mor);
}

static cat89_status alias_compose_cb(void *ctx, const cat89_mor *g,
                                     const cat89_mor *f, cat89_mor **out_mor)
{
    struct alias_ctx *cx = ctx;
    const struct alias_mor *gm;
    const struct alias_mor *fm;
    int r;

    gm = (const struct alias_mor *)(const void *)g;
    fm = (const struct alias_mor *)(const void *)f;
    r = alias_result_id(gm->id, fm->id);
    if (r < 0)
    {
        return CAT89_INVALID;
    }
    return alias_mor_make(cx->alias, r, fm->dom, gm->cod, out_mor);
}

static cat89_status alias_retain_cb(void *ctx, cat89_mor *mor)
{
    struct alias_mor *m;
    (void)ctx;
    m = (struct alias_mor *)mor;
    return cat89_ref_inc(&m->refs);
}

static void alias_unlink(struct cat89_alias *alias, struct alias_mor *m)
{
    struct alias_mor **link;

    link = &alias->live_mors;
    while (*link != NULL)
    {
        if (*link == m)
        {
            *link = m->next_live;
            return;
        }
        link = &(*link)->next_live;
    }
}

static void alias_release_cb(void *ctx, cat89_mor *mor)
{
    struct alias_ctx *cx = ctx;
    struct alias_mor *m;

    m = (struct alias_mor *)mor;
    if (cat89_ref_dec(&m->refs))
    {
        alias_unlink(cx->alias, m);
        cat89_free(&cx->alias->allocator, m);
    }
}

static int alias_owns_obj_cb(void *ctx, const cat89_obj *obj)
{
    struct alias_ctx *cx = ctx;
    return alias_obj_index(cx->alias, obj) >= 0;
}

static int alias_owns_mor_cb(void *ctx, const cat89_mor *mor)
{
    struct alias_ctx *cx = ctx;
    struct alias_mor *m;

    for (m = cx->alias->live_mors; m != NULL; m = m->next_live)
    {
        if ((const cat89_mor *)(const void *)m == mor)
        {
            return 1;
        }
    }
    return 0;
}

static void alias_destroy_cb(void *ctx)
{
    struct alias_ctx *cx = ctx;
    cat89_free(&cx->alias->allocator, cx);
}

static const cat89_category_ops alias_ops = {
    alias_dom_cb,      alias_cod_cb,    alias_identity_cb, alias_compose_cb,
    alias_obj_same_cb, alias_retain_cb, alias_release_cb,  alias_owns_obj_cb,
    alias_owns_mor_cb, alias_destroy_cb};

cat89_status cat89_alias_new(const cat89_allocator *allocator,
                             cat89_category **out_category,
                             cat89_alias **out_alias)
{
    struct cat89_alias *alias;
    struct alias_ctx *cx;
    const cat89_allocator *actual;
    cat89_status st;
    int i;

    if (out_category == NULL)
    {
        return CAT89_INVALID;
    }
    *out_category = NULL;
    actual = allocator;
    if (actual == NULL)
    {
        actual = cat89_allocator_default();
    }
    alias = cat89_alloc(actual, sizeof(*alias));
    if (alias == NULL)
    {
        return CAT89_NOMEM;
    }
    alias->allocator = *actual;
    alias->category = NULL;
    alias->live_mors = NULL;
    for (i = 0; i < 6; i = i + 1)
    {
        alias->tok[i] = 0;
    }
    cx = cat89_alloc(actual, sizeof(*cx));
    if (cx == NULL)
    {
        cat89_free(actual, alias);
        return CAT89_NOMEM;
    }
    cx->alias = alias;
    st = cat89_category_new(&alias_ops, cx, actual, &alias->category);
    if (st != CAT89_OK)
    {
        cat89_free(actual, cx);
        cat89_free(actual, alias);
        return st;
    }
    *out_category = alias->category;
    if (out_alias != NULL)
    {
        *out_alias = alias;
    }
    return CAT89_OK;
}

void cat89_alias_free(cat89_alias *alias)
{
    if (alias != NULL)
    {
        cat89_free(&alias->allocator, alias);
    }
}

const cat89_obj *cat89_alias_obj(cat89_alias *alias, enum cat89_alias_obj which)
{
    return alias_obj_ptr(alias, (int)which);
}

cat89_status cat89_alias_mor(cat89_alias *alias, enum cat89_alias_mor which,
                             cat89_mor **out_mor)
{
    int id;

    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    if (alias == NULL)
    {
        return CAT89_INVALID;
    }
    id = (int)which;
    switch (id)
    {
    case CAT89_ALIAS_IDA:
        return alias_mor_make(alias, id, alias_obj_ptr(alias, 0),
                              alias_obj_ptr(alias, 0), out_mor);
    case CAT89_ALIAS_IDB:
        return alias_mor_make(alias, id, alias_obj_ptr(alias, 3),
                              alias_obj_ptr(alias, 3), out_mor);
    case CAT89_ALIAS_IDC:
        return alias_mor_make(alias, id, alias_obj_ptr(alias, 4),
                              alias_obj_ptr(alias, 4), out_mor);
    case CAT89_ALIAS_F:
        return alias_mor_make(alias, id, alias_obj_ptr(alias, 1),
                              alias_obj_ptr(alias, 3), out_mor);
    case CAT89_ALIAS_G:
        return alias_mor_make(alias, id, alias_obj_ptr(alias, 2),
                              alias_obj_ptr(alias, 5), out_mor);
    case CAT89_ALIAS_H:
        return alias_mor_make(alias, id, alias_obj_ptr(alias, 1),
                              alias_obj_ptr(alias, 5), out_mor);
    default:
        return CAT89_INVALID;
    }
}

/* --------------------------------------------------- capabilities */

static cat89_status alias_obj_equal(void *ctx, const cat89_obj *a,
                                    const cat89_obj *b, int *out_equal)
{
    struct cat89_alias *alias = ctx;
    int ia;
    int ib;

    ia = alias_obj_index(alias, a);
    ib = alias_obj_index(alias, b);
    if (ia < 0)
    {
        *out_equal = 0;
        return CAT89_OK;
    }
    if (ib < 0)
    {
        *out_equal = 0;
        return CAT89_OK;
    }
    *out_equal = ia / 2 == ib / 2 ? 1 : 0;
    return CAT89_OK;
}

static cat89_status alias_mor_equal(void *ctx, const cat89_mor *f,
                                    const cat89_mor *g, int *out_equal)
{
    const struct alias_mor *fm;
    const struct alias_mor *gm;
    (void)ctx;
    fm = (const struct alias_mor *)(const void *)f;
    gm = (const struct alias_mor *)(const void *)g;
    *out_equal = fm->id == gm->id ? 1 : 0;
    return CAT89_OK;
}

static const cat89_eq_ops alias_eq_ops = {alias_obj_equal, alias_mor_equal,
                                          NULL};

struct alias_iter
{
    struct cat89_alias *alias;
    unsigned long next;
};

static cat89_status alias_obj_iter_open(void *ctx, cat89_obj_iter **out_iter)
{
    struct cat89_alias *alias = ctx;
    struct alias_iter *it;

    it = cat89_alloc(&alias->allocator, sizeof(*it));
    if (it == NULL)
    {
        return CAT89_NOMEM;
    }
    it->alias = alias;
    it->next = 0;
    *out_iter = (cat89_obj_iter *)it;
    return CAT89_OK;
}

static cat89_status alias_obj_iter_next(void *ctx, cat89_obj_iter *iter,
                                        const cat89_obj **out_obj,
                                        int *out_done)
{
    struct alias_iter *it = (struct alias_iter *)iter;
    (void)ctx;
    if (it->next >= 6)
    {
        *out_obj = NULL;
        *out_done = 1;
        return CAT89_OK;
    }
    *out_obj = alias_obj_ptr(it->alias, (int)it->next);
    it->next = it->next + 1;
    *out_done = 0;
    return CAT89_OK;
}

static void alias_obj_iter_close(void *ctx, cat89_obj_iter *iter)
{
    struct cat89_alias *alias = ctx;
    if (iter != NULL)
    {
        cat89_free(&alias->allocator, iter);
    }
}

static cat89_status alias_mor_iter_open(void *ctx, cat89_mor_iter **out_iter)
{
    struct cat89_alias *alias = ctx;
    struct alias_iter *it;

    it = cat89_alloc(&alias->allocator, sizeof(*it));
    if (it == NULL)
    {
        return CAT89_NOMEM;
    }
    it->alias = alias;
    it->next = 0;
    *out_iter = (cat89_mor_iter *)it;
    return CAT89_OK;
}

static cat89_status alias_mor_iter_next(void *ctx, cat89_mor_iter *iter,
                                        cat89_mor **out_mor, int *out_done)
{
    struct alias_iter *it = (struct alias_iter *)iter;
    cat89_status st;
    (void)ctx;
    if (it->next >= 6)
    {
        *out_mor = NULL;
        *out_done = 1;
        return CAT89_OK;
    }
    st = cat89_alias_mor(it->alias, (enum cat89_alias_mor)it->next, out_mor);
    if (st != CAT89_OK)
    {
        return st;
    }
    it->next = it->next + 1;
    *out_done = 0;
    return CAT89_OK;
}

static void alias_mor_iter_close(void *ctx, cat89_mor_iter *iter)
{
    struct cat89_alias *alias = ctx;
    if (iter != NULL)
    {
        cat89_free(&alias->allocator, iter);
    }
}

static const cat89_enum_ops alias_enum_ops = {alias_obj_iter_open,
                                              alias_obj_iter_next,
                                              alias_obj_iter_close,
                                              alias_mor_iter_open,
                                              alias_mor_iter_next,
                                              alias_mor_iter_close,
                                              NULL,
                                              NULL};

cat89_status cat89_alias_eq(cat89_alias *alias, cat89_eq **out_eq)
{
    if (alias == NULL)
    {
        return CAT89_INVALID;
    }
    return cat89_eq_new(alias->category, &alias_eq_ops, alias, NULL, out_eq);
}

cat89_status cat89_alias_enum(cat89_alias *alias, cat89_enum **out_enum)
{
    if (alias == NULL)
    {
        return CAT89_INVALID;
    }
    return cat89_enum_new(alias->category, &alias_enum_ops, alias, NULL,
                          out_enum);
}
