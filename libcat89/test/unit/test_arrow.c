/* cat89_test_arrow.c - arrow category C^-> tests.
 *
 * Realizes the arrow category over the C2 groupoid via cat89_arrow_* and checks
 * that commuting squares build while non-commuting squares are rejected, then
 * exhaustively checks identity + associativity over the collected arrow
 * morphisms with an externally supplied equality. */
#include <stdlib.h>

#include "finite_categories.h"
#include <cat89/cat89.h>
#include <test.h>

int cat89_test_failures = 0;

#define MAXMOR 64
#define MAXPM 512

struct aeq_ctx
{
    cat89_category *category;
    cat89_eq *base;
};

static cat89_status aeq_mor_equal(void *ctx, const cat89_mor *f,
                                  const cat89_mor *g, int *out_equal)
{
    struct aeq_ctx *pc = ctx;
    cat89_status st;
    int el;

    st = cat89_mor_equal(pc->base, cat89_comma_left_mor(pc->category, f),
                         cat89_comma_left_mor(pc->category, g), &el);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (el == 0)
    {
        *out_equal = 0;
        return CAT89_OK;
    }
    st = cat89_mor_equal(pc->base, cat89_comma_right_mor(pc->category, f),
                         cat89_comma_right_mor(pc->category, g), out_equal);
    return st;
}

static void aeq_destroy(void *ctx)
{
    struct aeq_ctx *pc = ctx;

    cat89_eq_release(pc->base);
    free(pc);
}

static cat89_status make_arrow_eq(cat89_category *arrow, cat89_eq *base,
                                  cat89_eq **out_eq)
{
    static const cat89_eq_ops ops = {NULL, aeq_mor_equal, aeq_destroy};
    struct aeq_ctx *pc;
    cat89_status st;

    pc = (struct aeq_ctx *)malloc(sizeof(*pc));
    if (pc == NULL)
    {
        return CAT89_NOMEM;
    }
    pc->category = arrow;
    pc->base = base;
    st = cat89_eq_retain(base);
    if (st != CAT89_OK)
    {
        free(pc);
        return st;
    }
    st = cat89_eq_new(arrow, &ops, pc, NULL, out_eq);
    if (st != CAT89_OK)
    {
        cat89_eq_release(base);
        free(pc);
        return st;
    }
    return CAT89_OK;
}

static unsigned long collect_morphs(cat89_enum *enumeration, cat89_mor **arr,
                                    unsigned long capacity)
{
    cat89_mor_iter *it;
    cat89_mor *mor;
    int done;
    unsigned long n;

    it = NULL;
    mor = NULL;
    if (cat89_mor_iter_open(enumeration, &it) != CAT89_OK)
    {
        return 0;
    }
    done = 0;
    n = 0;
    while (!done)
    {
        mor = NULL;
        cat89_mor_iter_next(enumeration, it, &mor, &done);
        if (!done)
        {
            if (n < capacity)
            {
                arr[n] = mor;
                n = n + 1;
            }
        }
    }
    cat89_mor_iter_close(enumeration, it);
    return n;
}

static int composable(cat89_category *cat, const cat89_mor *g,
                      const cat89_mor *f)
{
    const cat89_obj *codf;
    const cat89_obj *domg;

    cat89_cod(cat, f, &codf);
    cat89_dom(cat, g, &domg);
    return cat89_obj_same(cat, codf, domg) != 0;
}

int main(void)
{
    cat89_category *cat;
    cat89_eq *eq;
    cat89_enum *enumeration;
    cat89_category *arrow;
    cat89_eq *aeq;
    cat89_mor *morphs[MAXMOR];
    cat89_mor *pm[MAXPM];
    cat89_mor *cand;
    const cat89_obj *objs[MAXMOR];
    const cat89_obj *c;
    const cat89_obj *d;
    unsigned long n;
    unsigned long no;
    unsigned long nc;
    unsigned long i;
    unsigned long j;
    unsigned long u;
    unsigned long v;
    unsigned long k;
    unsigned long assoc_cases;
    int invalid_count;
    int valid;
    cat89_status st;

    T_START();
    cat = NULL;
    eq = NULL;
    enumeration = NULL;
    T_STATUS(
        cat89_fixture_build(CAT89_FIX_C2_GROUPOID, &cat, &eq, &enumeration),
        CAT89_OK);
    n = collect_morphs(enumeration, morphs, MAXMOR);
    T_EQ_UL(n, 2);

    arrow = NULL;
    aeq = NULL;
    T_STATUS(cat89_arrow_category_new(cat, eq, NULL, &arrow), CAT89_OK);
    T_ASSERT(arrow != NULL);

    no = 0;
    for (i = 0; i < n; i = i + 1)
    {
        c = NULL;
        d = NULL;
        T_STATUS(cat89_dom(cat, morphs[i], &c), CAT89_OK);
        T_STATUS(cat89_cod(cat, morphs[i], &d), CAT89_OK);
        T_STATUS(cat89_arrow_obj_new(arrow, c, morphs[i], d, &objs[no]),
                 CAT89_OK);
        no = no + 1;
    }
    T_EQ_UL(no, n);

    T_STATUS(make_arrow_eq(arrow, eq, &aeq), CAT89_OK);

    nc = 0;
    invalid_count = 0;
    for (i = 0; i < no; i = i + 1)
    {
        for (j = 0; j < no; j = j + 1)
        {
            for (u = 0; u < n; u = u + 1)
            {
                for (v = 0; v < n; v = v + 1)
                {
                    cand = NULL;
                    st = cat89_arrow_mor_new(arrow, objs[i], objs[j], morphs[u],
                                             morphs[v], &cand);
                    if (st == CAT89_OK)
                    {
                        T_ASSERT(cand != NULL);
                        pm[nc] = cand;
                        nc = nc + 1;
                    }
                    else
                    {
                        T_STATUS(st, CAT89_INVALID);
                        invalid_count = invalid_count + 1;
                    }
                }
            }
        }
    }
    T_ASSERT(nc > 0);
    T_ASSERT(invalid_count > 0);

    for (i = 0; i < nc; i = i + 1)
    {
        valid = -1;
        T_STATUS(cat89_check_left_identity(arrow, aeq, pm[i], &valid),
                 CAT89_OK);
        T_EQ_UL(valid, 1);
        valid = -1;
        T_STATUS(cat89_check_right_identity(arrow, aeq, pm[i], &valid),
                 CAT89_OK);
        T_EQ_UL(valid, 1);
    }

    assoc_cases = 0;
    for (i = 0; i < nc; i = i + 1)
    {
        for (j = 0; j < nc; j = j + 1)
        {
            for (k = 0; k < nc; k = k + 1)
            {
                if (!composable(arrow, pm[j], pm[k]))
                {
                    continue;
                }
                if (!composable(arrow, pm[i], pm[j]))
                {
                    continue;
                }
                valid = -1;
                T_STATUS(cat89_check_associativity(arrow, aeq, pm[i], pm[j],
                                                   pm[k], &valid),
                         CAT89_OK);
                T_EQ_UL(valid, 1);
                assoc_cases = assoc_cases + 1;
            }
        }
    }
    T_ASSERT(assoc_cases > 0);

    for (i = 0; i < nc; i = i + 1)
    {
        cat89_mor_release(arrow, pm[i]);
    }
    for (i = 0; i < n; i = i + 1)
    {
        cat89_mor_release(cat, morphs[i]);
    }
    cat89_eq_release(aeq);
    cat89_category_release(arrow);
    cat89_enum_release(enumeration);
    cat89_eq_release(eq);
    cat89_category_release(cat);
    return T_END() ? 0 : 1;
}
