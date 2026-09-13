/* cat89_mock_backend.c - instrumented structural backend for tests.
 *
 * To keep counters readable after the category (and thus the backend ctx) is
 * destroyed, the observable counters/lifecycle live in a SEPARATE observer
 * (`struct cat89_mock`) that is NOT the category ctx. The category ctx is a
 * small `struct cat89_mock_ctx` pointing at the observer; category destruction
 * frees only the ctx. The test frees the observer with cat89_mock_free once it
 * is done reading counters. This avoids the category-destroy use-after-free. */

#include <stdlib.h>

#include <cat89_internal.h>
#include <cat89_mock_backend.h>

struct cat89_mock_mor
{
    unsigned long refs;
    const cat89_obj *dom;
    const cat89_obj *cod;
    int is_id;
    struct cat89_mock_mor *next_live;
};

/* Observer: counters + objects + allocator + fault config (outlives ctx). */
struct cat89_mock
{
    cat89_allocator allocator;

    unsigned char tok_a;
    unsigned char tok_b;
    unsigned char tok_c;

    unsigned long dom_calls;
    unsigned long cod_calls;
    unsigned long compose_calls;
    unsigned long identity_calls;
    unsigned long retain_calls;
    unsigned long release_calls;
    unsigned long obj_same_calls;
    unsigned long owns_obj_calls;
    unsigned long owns_mor_calls;
    unsigned long mor_free_calls;
    unsigned long destroy_calls;

    struct cat89_mock_mor *live_mors;

    unsigned long fail_dom_after;
    cat89_status fail_dom_status;
    unsigned long fail_cod_after;
    cat89_status fail_cod_status;
    unsigned long fail_compose_after;
    cat89_status fail_compose_status;
    unsigned long fail_identity_after;
    cat89_status fail_identity_status;
    unsigned long fail_retain_after;
    cat89_status fail_retain_status;
};

/* Category ctx: just points at the observer. Freed at category destroy. */
struct cat89_mock_ctx
{
    struct cat89_mock *mock;
};

static struct cat89_mock_mor *mor_ptr(const cat89_mor *mor)
{
    return (struct cat89_mock_mor *)(void *)mor;
}

static void mock_register(struct cat89_mock *mock, struct cat89_mock_mor *mor)
{
    mor->next_live = mock->live_mors;
    mock->live_mors = mor;
}

static void mock_unlink(struct cat89_mock *mock, struct cat89_mock_mor *mor)
{
    struct cat89_mock_mor **link;

    link = &mock->live_mors;
    while (*link != NULL)
    {
        if (*link == mor)
        {
            *link = mor->next_live;
            return;
        }
        link = &(*link)->next_live;
    }
}

static struct cat89_mock *ctx_mock(struct cat89_mock_ctx *cx)
{
    return cx->mock;
}

static cat89_status mock_ops_retain(struct cat89_mock *mock,
                                    const cat89_mor *mor, cat89_mor **out_mor);

static cat89_status mock_dom(void *ctx, const cat89_mor *mor,
                             const cat89_obj **out_obj)
{
    struct cat89_mock *mock = ctx_mock(ctx);
    mock->dom_calls = mock->dom_calls + 1;
    if (mock->fail_dom_status != CAT89_OK &&
        mock->dom_calls > mock->fail_dom_after)
    {
        return mock->fail_dom_status;
    }
    *out_obj = mor_ptr(mor)->dom;
    return CAT89_OK;
}

static cat89_status mock_cod(void *ctx, const cat89_mor *mor,
                             const cat89_obj **out_obj)
{
    struct cat89_mock *mock = ctx_mock(ctx);
    mock->cod_calls = mock->cod_calls + 1;
    if (mock->fail_cod_status != CAT89_OK &&
        mock->cod_calls > mock->fail_cod_after)
    {
        return mock->fail_cod_status;
    }
    *out_obj = mor_ptr(mor)->cod;
    return CAT89_OK;
}

static cat89_status mock_identity(void *ctx, const cat89_obj *obj,
                                  cat89_mor **out_mor)
{
    struct cat89_mock *mock = ctx_mock(ctx);
    struct cat89_mock_mor *id;
    mock->identity_calls = mock->identity_calls + 1;
    if (mock->fail_identity_status != CAT89_OK &&
        mock->identity_calls > mock->fail_identity_after)
    {
        return mock->fail_identity_status;
    }

    id = cat89_alloc(&mock->allocator, sizeof(*id));
    if (id == NULL)
    {
        return CAT89_NOMEM;
    }
    id->refs = 1;
    id->dom = obj;
    id->cod = obj;
    id->is_id = 1;
    mock_register(mock, id);
    *out_mor = (cat89_mor *)id;
    return CAT89_OK;
}

static cat89_status mock_compose(void *ctx, const cat89_mor *g,
                                 const cat89_mor *f, cat89_mor **out_mor)
{
    struct cat89_mock *mock = ctx_mock(ctx);
    struct cat89_mock_mor *res;

    mock->compose_calls = mock->compose_calls + 1;
    if (mock->fail_compose_status != CAT89_OK &&
        mock->compose_calls > mock->fail_compose_after)
    {
        return mock->fail_compose_status;
    }

    if (mor_ptr(f)->is_id)
    {
        return mock_ops_retain(mock, g, out_mor);
    }
    if (mor_ptr(g)->is_id)
    {
        return mock_ops_retain(mock, f, out_mor);
    }

    res = cat89_alloc(&mock->allocator, sizeof(*res));
    if (res == NULL)
    {
        return CAT89_NOMEM;
    }
    res->refs = 1;
    res->dom = mor_ptr(f)->dom;
    res->cod = mor_ptr(g)->cod;
    res->is_id = 0;
    mock_register(mock, res);
    *out_mor = (cat89_mor *)res;
    return CAT89_OK;
}

/* retained copy of a mock morphism (shares dom/cod). */
static cat89_status mock_ops_retain(struct cat89_mock *mock,
                                    const cat89_mor *mor, cat89_mor **out_mor)
{
    struct cat89_mock_mor *src = mor_ptr(mor);
    struct cat89_mock_mor *cp;
    cp = cat89_alloc(&mock->allocator, sizeof(*cp));
    if (cp == NULL)
    {
        return CAT89_NOMEM;
    }
    cp->refs = 1;
    cp->dom = src->dom;
    cp->cod = src->cod;
    cp->is_id = src->is_id;
    mock_register(mock, cp);
    *out_mor = (cat89_mor *)cp;
    return CAT89_OK;
}

static int mock_obj_same(void *ctx, const cat89_obj *a, const cat89_obj *b)
{
    struct cat89_mock *mock = ctx_mock(ctx);
    mock->obj_same_calls = mock->obj_same_calls + 1;
    return a == b;
}

static cat89_status mock_retain(void *ctx, cat89_mor *mor)
{
    struct cat89_mock *mock = ctx_mock(ctx);
    mock->retain_calls = mock->retain_calls + 1;
    if (mock->fail_retain_status != CAT89_OK &&
        mock->retain_calls > mock->fail_retain_after)
    {
        return mock->fail_retain_status;
    }
    if (mor_ptr(mor)->refs == 0)
    {
        return CAT89_INVALID;
    }
    mor_ptr(mor)->refs = mor_ptr(mor)->refs + 1;
    return CAT89_OK;
}

static void mock_release(void *ctx, cat89_mor *mor)
{
    struct cat89_mock *mock = ctx_mock(ctx);
    struct cat89_mock_mor *m = mor_ptr(mor);
    mock->release_calls = mock->release_calls + 1;
    if (m->refs > 0)
    {
        m->refs = m->refs - 1;
    }
    if (m->refs == 0)
    {
        mock_unlink(mock, m);
        mock->mor_free_calls = mock->mor_free_calls + 1;
        cat89_free(&mock->allocator, m);
    }
}

static void mock_destroy(void *ctx)
{
    struct cat89_mock_ctx *cx = ctx;
    struct cat89_mock *mock = cx->mock;

    mock->destroy_calls = mock->destroy_calls + 1;
    cat89_free(&mock->allocator, cx);
}

static int mock_owns_obj(void *ctx, const cat89_obj *obj)
{
    struct cat89_mock *mock = ctx_mock(ctx);
    mock->owns_obj_calls = mock->owns_obj_calls + 1;
    if (obj == (const cat89_obj *)(const void *)&mock->tok_a)
    {
        return 1;
    }
    if (obj == (const cat89_obj *)(const void *)&mock->tok_b)
    {
        return 1;
    }
    if (obj == (const cat89_obj *)(const void *)&mock->tok_c)
    {
        return 1;
    }
    return 0;
}

static int mock_owns_mor(void *ctx, const cat89_mor *mor)
{
    struct cat89_mock *mock = ctx_mock(ctx);
    struct cat89_mock_mor *m;

    mock->owns_mor_calls = mock->owns_mor_calls + 1;
    for (m = mock->live_mors; m != NULL; m = m->next_live)
    {
        if ((const cat89_mor *)(const void *)m == mor)
        {
            return 1;
        }
    }
    return 0;
}

static const cat89_category_ops mock_ops = {
    mock_dom,    mock_cod,     mock_identity, mock_compose,  mock_obj_same,
    mock_retain, mock_release, mock_owns_obj, mock_owns_mor, mock_destroy};

cat89_status cat89_mock_new(const cat89_allocator *allocator,
                            cat89_category **out_category,
                            cat89_mock **out_mock)
{
    cat89_status st;
    struct cat89_mock *mock;
    struct cat89_mock_ctx *cx;

    if (out_category == NULL)
    {
        return CAT89_INVALID;
    }
    *out_category = NULL;
    if (allocator == NULL)
    {
        allocator = cat89_allocator_default();
    }

    mock = cat89_alloc(allocator, sizeof(*mock));
    if (mock == NULL)
    {
        return CAT89_NOMEM;
    }
    mock->allocator = *allocator;
    mock->tok_a = 0;
    mock->tok_b = 0;
    mock->tok_c = 0;
    mock->dom_calls = 0;
    mock->cod_calls = 0;
    mock->compose_calls = 0;
    mock->identity_calls = 0;
    mock->retain_calls = 0;
    mock->release_calls = 0;
    mock->obj_same_calls = 0;
    mock->owns_obj_calls = 0;
    mock->owns_mor_calls = 0;
    mock->mor_free_calls = 0;
    mock->destroy_calls = 0;
    mock->live_mors = NULL;
    mock->fail_dom_after = 0;
    mock->fail_dom_status = CAT89_OK;
    mock->fail_cod_after = 0;
    mock->fail_cod_status = CAT89_OK;
    mock->fail_compose_after = 0;
    mock->fail_compose_status = CAT89_OK;
    mock->fail_identity_after = 0;
    mock->fail_identity_status = CAT89_OK;
    mock->fail_retain_after = 0;
    mock->fail_retain_status = CAT89_OK;

    cx = cat89_alloc(&mock->allocator, sizeof(*cx));
    if (cx == NULL)
    {
        cat89_free(allocator, mock);
        return CAT89_NOMEM;
    }
    cx->mock = mock;

    st = cat89_category_new(&mock_ops, cx, &mock->allocator, out_category);
    if (st != CAT89_OK)
    {
        cat89_free(&mock->allocator, cx);
        cat89_free(allocator, mock);
        return st;
    }

    if (out_mock != NULL)
    {
        *out_mock = mock;
    }
    return CAT89_OK;
}

void cat89_mock_free(cat89_mock *mock)
{
    if (mock != NULL)
    {
        cat89_free(&mock->allocator, mock);
    }
}

const cat89_obj *cat89_mock_obj(cat89_mock *mock, enum cat89_mock_obj which)
{
    switch (which)
    {
    case CAT89_MOCK_A:
        return (const cat89_obj *)(const void *)&mock->tok_a;
    case CAT89_MOCK_B:
        return (const cat89_obj *)(const void *)&mock->tok_b;
    default:
        return (const cat89_obj *)(const void *)&mock->tok_c;
    }
}

static const cat89_obj *mock_obj_for(struct cat89_mock *mock,
                                     enum cat89_mock_obj which)
{
    return cat89_mock_obj(mock, which);
}

cat89_status cat89_mock_mor(cat89_mock *mock, enum cat89_mock_obj dom,
                            enum cat89_mock_obj cod, cat89_mor **out_mor)
{
    struct cat89_mock_mor *m;
    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    m = cat89_alloc(&mock->allocator, sizeof(*m));
    if (m == NULL)
    {
        return CAT89_NOMEM;
    }
    m->refs = 1;
    m->dom = mock_obj_for(mock, dom);
    m->cod = mock_obj_for(mock, cod);
    m->is_id = 0;
    mock_register(mock, m);
    *out_mor = (cat89_mor *)m;
    return CAT89_OK;
}

unsigned long cat89_mock_dom_calls(cat89_mock *mock)
{
    return mock->dom_calls;
}
unsigned long cat89_mock_cod_calls(cat89_mock *mock)
{
    return mock->cod_calls;
}
unsigned long cat89_mock_compose_calls(cat89_mock *mock)
{
    return mock->compose_calls;
}
unsigned long cat89_mock_identity_calls(cat89_mock *mock)
{
    return mock->identity_calls;
}
unsigned long cat89_mock_retain_calls(cat89_mock *mock)
{
    return mock->retain_calls;
}
unsigned long cat89_mock_release_calls(cat89_mock *mock)
{
    return mock->release_calls;
}
unsigned long cat89_mock_obj_same_calls(cat89_mock *mock)
{
    return mock->obj_same_calls;
}
unsigned long cat89_mock_owns_obj_calls(cat89_mock *mock)
{
    return mock->owns_obj_calls;
}
unsigned long cat89_mock_owns_mor_calls(cat89_mock *mock)
{
    return mock->owns_mor_calls;
}
void cat89_mock_reset_calls(cat89_mock *mock)
{
    mock->dom_calls = 0;
    mock->cod_calls = 0;
    mock->compose_calls = 0;
    mock->identity_calls = 0;
    mock->retain_calls = 0;
    mock->release_calls = 0;
    mock->obj_same_calls = 0;
    mock->owns_obj_calls = 0;
    mock->owns_mor_calls = 0;
    mock->mor_free_calls = 0;
    mock->destroy_calls = 0;
}
unsigned long cat89_mock_mor_free_calls(cat89_mock *mock)
{
    return mock->mor_free_calls;
}
unsigned long cat89_mock_destroy_calls(cat89_mock *mock)
{
    return mock->destroy_calls;
}

static void mock_fail_set(unsigned long *after, cat89_status *status,
                          unsigned long a, cat89_status s)
{
    *after = a;
    *status = s;
}

void cat89_mock_fail_dom(cat89_mock *mock, unsigned long after,
                         cat89_status status)
{
    mock_fail_set(&mock->fail_dom_after, &mock->fail_dom_status, after, status);
}

void cat89_mock_fail_cod(cat89_mock *mock, unsigned long after,
                         cat89_status status)
{
    mock_fail_set(&mock->fail_cod_after, &mock->fail_cod_status, after, status);
}

void cat89_mock_fail_compose(cat89_mock *mock, unsigned long after,
                             cat89_status status)
{
    mock_fail_set(&mock->fail_compose_after, &mock->fail_compose_status, after,
                  status);
}

void cat89_mock_fail_identity(cat89_mock *mock, unsigned long after,
                              cat89_status status)
{
    mock_fail_set(&mock->fail_identity_after, &mock->fail_identity_status,
                  after, status);
}

void cat89_mock_fail_retain(cat89_mock *mock, unsigned long after,
                            cat89_status status)
{
    mock_fail_set(&mock->fail_retain_after, &mock->fail_retain_status, after,
                  status);
}
