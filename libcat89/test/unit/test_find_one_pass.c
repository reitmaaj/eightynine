/* cat89_test_find_one_pass.c - EN01-EN05: finder enumeration is one-pass.
 *
 * A wrapper enumeration counts object-iterator openings and fails any second
 * opening, so a count-then-re-enumerate implementation fails deterministically
 * while a one-pass collector succeeds. */
#include <cat89/cat89.h>
#include <finite_categories.h>
#include <test.h>

int cat89_test_failures = 0;

struct onepass
{
    cat89_enum *inner;
    unsigned long obj_opens;
    unsigned long mor_opens;
    int fail_second;
};

static cat89_status op_obj_open(void *ctx, cat89_obj_iter **out_iter)
{
    struct onepass *p = ctx;
    p->obj_opens = p->obj_opens + 1;
    if (p->fail_second && p->obj_opens >= 2)
    {
        return CAT89_CALLBACK;
    }
    return cat89_obj_iter_open(p->inner, out_iter);
}

static cat89_status op_obj_next(void *ctx, cat89_obj_iter *iter,
                                const cat89_obj **out_obj, int *out_done)
{
    struct onepass *p = ctx;
    return cat89_obj_iter_next(p->inner, iter, out_obj, out_done);
}

static void op_obj_close(void *ctx, cat89_obj_iter *iter)
{
    struct onepass *p = ctx;
    cat89_obj_iter_close(p->inner, iter);
}

static cat89_status op_mor_open(void *ctx, cat89_mor_iter **out_iter)
{
    struct onepass *p = ctx;
    p->mor_opens = p->mor_opens + 1;
    return cat89_mor_iter_open(p->inner, out_iter);
}

static cat89_status op_mor_next(void *ctx, cat89_mor_iter *iter,
                                cat89_mor **out_mor, int *out_done)
{
    struct onepass *p = ctx;
    return cat89_mor_iter_next(p->inner, iter, out_mor, out_done);
}

static void op_mor_close(void *ctx, cat89_mor_iter *iter)
{
    struct onepass *p = ctx;
    cat89_mor_iter_close(p->inner, iter);
}

static const cat89_enum_ops op_ops = {op_obj_open, op_obj_next, op_obj_close,
                                      op_mor_open, op_mor_next, op_mor_close,
                                      NULL,        NULL};

static cat89_status op_wrap(cat89_category *cat, cat89_enum *inner,
                            struct onepass *p, cat89_enum **out)
{
    p->inner = inner;
    p->obj_opens = 0;
    p->mor_opens = 0;
    p->fail_second = 1;
    return cat89_enum_new(cat, &op_ops, p, NULL, out);
}

static void test_en01_en02_terminal_initial(void)
{
    cat89_category *cat;
    cat89_eq *eq;
    cat89_enum *inner;
    cat89_enum *en;
    struct onepass p;
    const cat89_obj *out;

    cat = NULL;
    eq = NULL;
    inner = NULL;
    en = NULL;
    out = NULL;
    T_STATUS(cat89_fixture_build(CAT89_FIX_TERMINAL, &cat, &eq, &inner),
             CAT89_OK);
    T_STATUS(op_wrap(cat, inner, &p, &en), CAT89_OK);
    T_STATUS(cat89_find_terminal(cat, en, &out), CAT89_OK);
    T_ASSERT(out != NULL);
    T_EQ_UL(p.obj_opens, 1);
    out = NULL;
    p.obj_opens = 0;
    T_STATUS(cat89_find_initial(cat, en, &out), CAT89_OK);
    T_ASSERT(out != NULL);
    T_EQ_UL(p.obj_opens, 1);
    cat89_enum_release(en);
    cat89_enum_release(inner);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

static void test_en03_product_collection(void)
{
    cat89_category *cat;
    cat89_eq *eq;
    cat89_enum *inner;
    cat89_enum *en;
    struct onepass p;
    const cat89_obj *a;
    const cat89_obj *b;
    cat89_limit *lim;

    cat = NULL;
    eq = NULL;
    inner = NULL;
    en = NULL;
    a = NULL;
    b = NULL;
    lim = NULL;
    T_STATUS(cat89_fixture_build(CAT89_FIX_BINPROD, &cat, &eq, &inner),
             CAT89_OK);
    {
        cat89_obj_iter *it = NULL;
        int done = 0;
        const cat89_obj *o = NULL;
        T_STATUS(cat89_obj_iter_open(inner, &it), CAT89_OK);
        T_STATUS(cat89_obj_iter_next(inner, it, &o, &done), CAT89_OK);
        a = o;
        done = 0;
        T_STATUS(cat89_obj_iter_next(inner, it, &o, &done), CAT89_OK);
        b = o;
        cat89_obj_iter_close(inner, it);
    }
    T_ASSERT(a != NULL && b != NULL);
    T_STATUS(op_wrap(cat, inner, &p, &en), CAT89_OK);
    T_STATUS(
        cat89_find_binary_product(cat, en, eq, a, b, NULL, &lim, NULL, NULL),
        CAT89_OK);
    T_ASSERT(lim != NULL);
    T_EQ_UL(p.obj_opens, 1);
    cat89_limit_release(lim);
    cat89_enum_release(en);
    cat89_enum_release(inner);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

static void test_en04_pullback_collection(void)
{
    cat89_category *cat;
    cat89_eq *eq;
    cat89_enum *inner;
    cat89_enum *en;
    struct onepass p;
    cat89_mor *f;
    cat89_mor *g;
    cat89_limit *lim;

    cat = NULL;
    eq = NULL;
    inner = NULL;
    en = NULL;
    f = NULL;
    g = NULL;
    lim = NULL;
    T_STATUS(cat89_fixture_build(CAT89_FIX_PULLBACK, &cat, &eq, &inner),
             CAT89_OK);
    {
        cat89_mor_iter *it = NULL;
        int done = 0;
        cat89_mor *m = NULL;
        int skip;
        T_STATUS(cat89_mor_iter_open(inner, &it), CAT89_OK);
        for (skip = 0; skip < 8; skip = skip + 1)
        {
            m = NULL;
            done = 0;
            T_STATUS(cat89_mor_iter_next(inner, it, &m, &done), CAT89_OK);
            cat89_mor_release(cat, m);
        }
        m = NULL;
        done = 0;
        T_STATUS(cat89_mor_iter_next(inner, it, &m, &done), CAT89_OK);
        f = m;
        m = NULL;
        done = 0;
        T_STATUS(cat89_mor_iter_next(inner, it, &m, &done), CAT89_OK);
        g = m;
        cat89_mor_iter_close(inner, it);
    }
    T_ASSERT(f != NULL && g != NULL);
    T_STATUS(op_wrap(cat, inner, &p, &en), CAT89_OK);
    T_STATUS(
        cat89_find_pullback(cat, en, eq, f, g, NULL, &lim, NULL, NULL, NULL),
        CAT89_OK);
    T_ASSERT(lim != NULL);
    T_EQ_UL(p.obj_opens, 1);
    cat89_limit_release(lim);
    cat89_mor_release(cat, g);
    cat89_mor_release(cat, f);
    cat89_enum_release(en);
    cat89_enum_release(inner);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    test_en01_en02_terminal_initial();
    test_en03_product_collection();
    test_en04_pullback_collection();
    return T_END() ? 0 : 1;
}
