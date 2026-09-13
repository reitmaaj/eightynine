/* cat89_test_coslice.c - coslice category A/C tests.
 *
 * Builds the coslice of the C2 groupoid over its single object, enumerates the
 * coslice objects (arrows out of the source) and exhaustively checks identity +
 * associativity over the collected coslice morphisms, with an externally
 * supplied equality on the underlying triangle legs. */
#include <stdlib.h>

#include "finite_categories.h"
#include <cat89/cat89.h>
#include <test.h>

int cat89_test_failures = 0;

#define MAXMOR 64
#define MAXPM 512

struct cseq_ctx
{
    cat89_category *category;
    cat89_eq *base;
};

static cat89_status cseq_mor_equal(void *ctx, const cat89_mor *f,
                                   const cat89_mor *g, int *out_equal)
{
    struct cseq_ctx *pc = ctx;
    cat89_status st;

    st = cat89_mor_equal(pc->base, cat89_coslice_mor_mor(pc->category, f),
                         cat89_coslice_mor_mor(pc->category, g), out_equal);
    return st;
}

static void cseq_destroy(void *ctx)
{
    struct cseq_ctx *pc = ctx;

    cat89_eq_release(pc->base);
    free(pc);
}

static cat89_status make_coslice_eq(cat89_category *coslice, cat89_eq *base,
                                    cat89_eq **out_eq)
{
    static const cat89_eq_ops ops = {NULL, cseq_mor_equal, cseq_destroy};
    struct cseq_ctx *pc;
    cat89_status st;

    pc = (struct cseq_ctx *)malloc(sizeof(*pc));
    if (pc == NULL)
    {
        return CAT89_NOMEM;
    }
    pc->category = coslice;
    pc->base = base;
    st = cat89_eq_retain(base);
    if (st != CAT89_OK)
    {
        free(pc);
        return st;
    }
    st = cat89_eq_new(coslice, &ops, pc, NULL, out_eq);
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
    cat89_category *coslice;
    cat89_eq *ceq;
    cat89_mor *morphs[MAXMOR];
    cat89_mor *pm[MAXPM];
    cat89_mor *cand;
    const cat89_obj *objs[MAXMOR];
    const cat89_obj *source;
    const cat89_obj *df;
    const cat89_obj *cfx;
    const cat89_obj *cgy;
    const cat89_obj *dk;
    const cat89_obj *ckk;
    const cat89_mor *fleg;
    const cat89_mor *gleg;
    unsigned long n;
    unsigned long no;
    unsigned long nc;
    unsigned long i;
    unsigned long j;
    unsigned long k;
    unsigned long m;
    unsigned long assoc_cases;
    int invalid_count;
    int valid;
    int s1;
    int s2;
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

    source = NULL;
    T_STATUS(cat89_dom(cat, morphs[0], &source), CAT89_OK);

    coslice = NULL;
    ceq = NULL;
    T_STATUS(cat89_coslice_category_new(cat, eq, source, NULL, &coslice),
             CAT89_OK);
    T_ASSERT(coslice != NULL);

    no = 0;
    for (i = 0; i < n; i = i + 1)
    {
        df = NULL;
        T_STATUS(cat89_dom(cat, morphs[i], &df), CAT89_OK);
        if (cat89_obj_same(cat, df, source) == 0)
        {
            continue;
        }
        T_STATUS(cat89_coslice_obj_new(coslice, morphs[i], &objs[no]),
                 CAT89_OK);
        no = no + 1;
    }
    T_EQ_UL(no, 2);

    T_STATUS(make_coslice_eq(coslice, eq, &ceq), CAT89_OK);

    nc = 0;
    invalid_count = 0;
    for (i = 0; i < no; i = i + 1)
    {
        for (j = 0; j < no; j = j + 1)
        {
            fleg = cat89_coslice_obj_mor(coslice, objs[i]);
            gleg = cat89_coslice_obj_mor(coslice, objs[j]);
            cfx = NULL;
            cgy = NULL;
            T_STATUS(cat89_cod(cat, fleg, &cfx), CAT89_OK);
            T_STATUS(cat89_cod(cat, gleg, &cgy), CAT89_OK);
            for (k = 0; k < n; k = k + 1)
            {
                dk = NULL;
                ckk = NULL;
                T_STATUS(cat89_dom(cat, morphs[k], &dk), CAT89_OK);
                T_STATUS(cat89_cod(cat, morphs[k], &ckk), CAT89_OK);
                s1 = cat89_obj_same(cat, dk, cfx);
                s2 = cat89_obj_same(cat, ckk, cgy);
                if (s1 == 0)
                {
                    continue;
                }
                if (s2 == 0)
                {
                    continue;
                }
                cand = NULL;
                st = cat89_coslice_mor_new(coslice, objs[i], objs[j], morphs[k],
                                           &cand);
                if (st == CAT89_OK)
                {
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
    T_ASSERT(nc > 0);
    T_ASSERT(invalid_count > 0);

    for (m = 0; m < nc; m = m + 1)
    {
        valid = -1;
        T_STATUS(cat89_check_left_identity(coslice, ceq, pm[m], &valid),
                 CAT89_OK);
        T_EQ_UL(valid, 1);
        valid = -1;
        T_STATUS(cat89_check_right_identity(coslice, ceq, pm[m], &valid),
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
                if (!composable(coslice, pm[j], pm[k]))
                {
                    continue;
                }
                if (!composable(coslice, pm[i], pm[j]))
                {
                    continue;
                }
                valid = -1;
                T_STATUS(cat89_check_associativity(coslice, ceq, pm[i], pm[j],
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
        cat89_mor_release(coslice, pm[i]);
    }
    for (i = 0; i < n; i = i + 1)
    {
        cat89_mor_release(cat, morphs[i]);
    }
    cat89_eq_release(ceq);
    cat89_category_release(coslice);
    cat89_enum_release(enumeration);
    cat89_eq_release(eq);
    cat89_category_release(cat);
    return T_END() ? 0 : 1;
}
