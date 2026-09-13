/* cat89_test_slice.c - slice category C/A tests.
 *
 * Builds the slice of COMP2 over the object with domain object C (the codomain
 * of g : B -> C), enumerates the slice objects (arrows into C), and
 * exhaustively checks identity + associativity over the collected slice
 * morphisms, using an externally supplied equality on the underlying triangle
 * legs. */
#include <stdlib.h>

#include "finite_categories.h"
#include <cat89/cat89.h>
#include <test.h>

int cat89_test_failures = 0;

#define MAXMOR 64
#define MAXPM 512

struct seq_ctx
{
    cat89_category *category;
    cat89_eq *base;
};

static cat89_status seq_mor_equal(void *ctx, const cat89_mor *f,
                                  const cat89_mor *g, int *out_equal)
{
    struct seq_ctx *pc = ctx;
    cat89_status st;

    st = cat89_mor_equal(pc->base, cat89_slice_mor_mor(pc->category, f),
                         cat89_slice_mor_mor(pc->category, g), out_equal);
    return st;
}

static void seq_destroy(void *ctx)
{
    struct seq_ctx *pc = ctx;

    cat89_eq_release(pc->base);
    free(pc);
}

static cat89_status make_slice_eq(cat89_category *slice, cat89_eq *base,
                                  cat89_eq **out_eq)
{
    static const cat89_eq_ops ops = {NULL, seq_mor_equal, seq_destroy};
    struct seq_ctx *pc;
    cat89_status st;

    pc = (struct seq_ctx *)malloc(sizeof(*pc));
    if (pc == NULL)
    {
        return CAT89_NOMEM;
    }
    pc->category = slice;
    pc->base = base;
    st = cat89_eq_retain(base);
    if (st != CAT89_OK)
    {
        free(pc);
        return st;
    }
    st = cat89_eq_new(slice, &ops, pc, NULL, out_eq);
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
    cat89_category *slice;
    cat89_eq *seq;
    cat89_mor *morphs[MAXMOR];
    cat89_mor *pm[MAXPM];
    cat89_mor *cand;
    const cat89_obj *objs[MAXMOR];
    const cat89_obj *apex;
    const cat89_obj *cf;
    const cat89_obj *dh;
    const cat89_obj *ch;
    const cat89_obj *df;
    const cat89_obj *dfg;
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

    apex = NULL;
    T_STATUS(cat89_cod(cat, morphs[0], &apex), CAT89_OK);

    slice = NULL;
    seq = NULL;
    T_STATUS(cat89_slice_category_new(cat, eq, apex, NULL, &slice), CAT89_OK);
    T_ASSERT(slice != NULL);

    no = 0;
    for (i = 0; i < n; i = i + 1)
    {
        cf = NULL;
        T_STATUS(cat89_cod(cat, morphs[i], &cf), CAT89_OK);
        if (cat89_obj_same(cat, cf, apex) == 0)
        {
            continue;
        }
        T_STATUS(cat89_slice_obj_new(slice, morphs[i], &objs[no]), CAT89_OK);
        no = no + 1;
    }
    T_EQ_UL(no, 2);

    T_STATUS(make_slice_eq(slice, eq, &seq), CAT89_OK);

    nc = 0;
    invalid_count = 0;
    for (i = 0; i < no; i = i + 1)
    {
        for (j = 0; j < no; j = j + 1)
        {
            fleg = cat89_slice_obj_mor(slice, objs[i]);
            gleg = cat89_slice_obj_mor(slice, objs[j]);
            df = NULL;
            dfg = NULL;
            T_STATUS(cat89_dom(cat, fleg, &df), CAT89_OK);
            T_STATUS(cat89_dom(cat, gleg, &dfg), CAT89_OK);
            for (k = 0; k < n; k = k + 1)
            {
                dh = NULL;
                ch = NULL;
                T_STATUS(cat89_dom(cat, morphs[k], &dh), CAT89_OK);
                T_STATUS(cat89_cod(cat, morphs[k], &ch), CAT89_OK);
                s1 = cat89_obj_same(cat, dh, df);
                s2 = cat89_obj_same(cat, ch, dfg);
                if (s1 == 0 || s2 == 0)
                {
                    continue;
                }
                cand = NULL;
                st = cat89_slice_mor_new(slice, objs[i], objs[j], morphs[k],
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
        T_STATUS(cat89_check_left_identity(slice, seq, pm[m], &valid),
                 CAT89_OK);
        T_EQ_UL(valid, 1);
        valid = -1;
        T_STATUS(cat89_check_right_identity(slice, seq, pm[m], &valid),
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
                if (!composable(slice, pm[j], pm[k]))
                {
                    continue;
                }
                if (!composable(slice, pm[i], pm[j]))
                {
                    continue;
                }
                valid = -1;
                T_STATUS(cat89_check_associativity(slice, seq, pm[i], pm[j],
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
        cat89_mor_release(slice, pm[i]);
    }
    for (i = 0; i < n; i = i + 1)
    {
        cat89_mor_release(cat, morphs[i]);
    }
    cat89_eq_release(seq);
    cat89_category_release(slice);
    cat89_enum_release(enumeration);
    cat89_eq_release(eq);
    cat89_category_release(cat);
    return T_END() ? 0 : 1;
}
