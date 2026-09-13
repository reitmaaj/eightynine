/* cat89_test_find_equalizer.c - finite equalizer finder.
 *
 * Finds the equalizer E of parallel arrows f, g : X -> Y in the EQUALIZER
 * ambient and verifies the returned cat89_limit: apex E (not the decoy Z), the
 * leg e : E -> X (dom E, cod X), and the universal factor of a real candidate
 * cone via cat89_limit_factor (CAT-I9). Also NOT_FOUND and INVALID. */
#include <stdlib.h>

#include <cat89/cat89.h>
#include <finite_categories.h>
#include <test.h>

int cat89_test_failures = 0;

static const cat89_obj *obj_index(cat89_enum *enumeration, unsigned long want)
{
    cat89_obj_iter *it;
    const cat89_obj *obj;
    unsigned long i;
    int done;

    it = NULL;
    if (cat89_obj_iter_open(enumeration, &it) != CAT89_OK)
    {
        return NULL;
    }
    i = 0;
    done = 0;
    obj = NULL;
    while (!done)
    {
        obj = NULL;
        cat89_obj_iter_next(enumeration, it, &obj, &done);
        if (!done)
        {
            if (i == want)
            {
                cat89_obj_iter_close(enumeration, it);
                return obj;
            }
            i = i + 1;
        }
    }
    cat89_obj_iter_close(enumeration, it);
    return NULL;
}

static cat89_status grab_mor(cat89_category *cat, cat89_enum *enumeration,
                             unsigned long want, cat89_mor **out)
{
    cat89_mor_iter *it;
    cat89_mor *mor;
    unsigned long i;
    int done;

    it = NULL;
    *out = NULL;
    if (cat89_mor_iter_open(enumeration, &it) != CAT89_OK)
    {
        return CAT89_INVALID;
    }
    i = 0;
    done = 0;
    while (!done)
    {
        mor = NULL;
        cat89_mor_iter_next(enumeration, it, &mor, &done);
        if (!done)
        {
            if (i == want)
            {
                cat89_mor_iter_close(enumeration, it);
                *out = mor;
                return CAT89_OK;
            }
            cat89_mor_release(cat, mor);
            i = i + 1;
        }
    }
    cat89_mor_iter_close(enumeration, it);
    return CAT89_NOT_FOUND;
}

struct legctx
{
    cat89_category *cat;
    const cat89_obj *pt0;
    const cat89_obj *pt1;
    cat89_mor *l0;
    cat89_mor *l1;
};

static cat89_status cand_leg(void *ctx, const cat89_obj *shape_obj,
                             cat89_mor **out_mor)
{
    struct legctx *lc = ctx;
    cat89_status st;

    *out_mor = NULL;
    if (shape_obj == lc->pt0)
    {
        st = cat89_mor_retain(lc->cat, lc->l0);
        if (st != CAT89_OK)
        {
            return st;
        }
        *out_mor = lc->l0;
        return CAT89_OK;
    }
    if (shape_obj == lc->pt1)
    {
        st = cat89_mor_retain(lc->cat, lc->l1);
        if (st != CAT89_OK)
        {
            return st;
        }
        *out_mor = lc->l1;
        return CAT89_OK;
    }
    return CAT89_INVALID;
}

static void test_equalizer_found(void)
{
    cat89_category *cat;
    cat89_enum *enumeration;
    cat89_eq *eq;
    const cat89_obj *E;
    const cat89_obj *Z;
    const cat89_obj *pt0;
    const cat89_obj *pt1;
    cat89_diagram *diagram;
    cat89_cone *cone;
    cat89_limit *limit;
    cat89_cone *candidate;
    cat89_mor *f;
    cat89_mor *g;
    cat89_mor *e;
    cat89_mor *z;
    cat89_mor *mz;
    cat89_mor *d;
    cat89_mor *leg;
    cat89_mor *u;
    cat89_cone_ops cops;
    struct legctx lc;
    const cat89_obj *dm;
    const cat89_obj *cd;
    cat89_status st;

    cat = NULL;
    enumeration = NULL;
    eq = NULL;
    limit = NULL;
    pt0 = NULL;
    pt1 = NULL;
    T_STATUS(cat89_fixture_build(CAT89_FIX_EQUALIZER, &cat, &eq, &enumeration),
             CAT89_OK);
    E = obj_index(enumeration, 0);
    Z = obj_index(enumeration, 3);
    T_ASSERT(E != NULL && Z != NULL);

    f = NULL;
    g = NULL;
    e = NULL;
    z = NULL;
    mz = NULL;
    d = NULL;
    T_STATUS(grab_mor(cat, enumeration, 5, &f), CAT89_OK);
    T_STATUS(grab_mor(cat, enumeration, 6, &g), CAT89_OK);
    T_STATUS(grab_mor(cat, enumeration, 4, &e), CAT89_OK);
    T_STATUS(grab_mor(cat, enumeration, 7, &z), CAT89_OK);
    T_STATUS(grab_mor(cat, enumeration, 10, &mz), CAT89_OK);
    T_STATUS(grab_mor(cat, enumeration, 8, &d), CAT89_OK);
    T_ASSERT(f && g && e && z && mz && d);

    st = cat89_find_equalizer(cat, enumeration, eq, f, g, NULL, &limit, &pt0,
                              &pt1);
    T_STATUS(st, CAT89_OK);
    T_ASSERT(limit != NULL);
    T_ASSERT(pt0 != NULL && pt1 != NULL);

    cone = cat89_limit_cone(limit);
    T_ASSERT(cat89_obj_same(cat, cat89_cone_apex(cone), E));

    leg = NULL;
    T_STATUS(cat89_cone_leg(cone, pt0, &leg), CAT89_OK);
    cat89_dom(cat, leg, &dm);
    cat89_cod(cat, leg, &cd);
    T_ASSERT(cat89_obj_same(cat, dm, E));
    {
        int eqv = 0;
        T_STATUS(cat89_mor_equal(eq, leg, e, &eqv), CAT89_OK);
        T_EQ_UL(eqv, 1);
    }
    cat89_mor_release(cat, leg);

    /* candidate cone over the returned diagram: apex Z, legs z:Z->X,
     * mz:Z->Y */
    diagram = cat89_cone_diagram(cone);
    lc.cat = cat;
    lc.pt0 = pt0;
    lc.pt1 = pt1;
    lc.l0 = z;
    lc.l1 = mz;
    cops.leg = cand_leg;
    cops.destroy = NULL;
    candidate = NULL;
    T_STATUS(cat89_cone_new(diagram, Z, &cops, &lc, NULL, &candidate),
             CAT89_OK);

    u = NULL;
    T_STATUS(cat89_limit_factor(limit, candidate, &u), CAT89_OK);
    T_ASSERT(u != NULL);
    cat89_dom(cat, u, &dm);
    cat89_cod(cat, u, &cd);
    T_ASSERT(cat89_obj_same(cat, dm, Z));
    T_ASSERT(cat89_obj_same(cat, cd, E));
    {
        int eqv = 0;
        T_STATUS(cat89_mor_equal(eq, u, d, &eqv), CAT89_OK);
        T_EQ_UL(eqv, 1);
    }
    cat89_mor_release(cat, u);

    cat89_cone_release(candidate);
    cat89_mor_release(cat, f);
    cat89_mor_release(cat, g);
    cat89_mor_release(cat, e);
    cat89_mor_release(cat, z);
    cat89_mor_release(cat, mz);
    cat89_mor_release(cat, d);
    cat89_limit_release(limit);
    cat89_enum_release(enumeration);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

static void test_equalizer_absent(void)
{
    cat89_category *cat;
    cat89_enum *enumeration;
    cat89_eq *eq;
    cat89_mor *f;
    cat89_mor *g;
    cat89_limit *limit;
    cat89_status st;

    cat = NULL;
    enumeration = NULL;
    eq = NULL;
    limit = NULL;
    T_STATUS(
        cat89_fixture_build(CAT89_FIX_COEQUALIZER, &cat, &eq, &enumeration),
        CAT89_OK);
    f = NULL;
    g = NULL;
    T_STATUS(grab_mor(cat, enumeration, 4, &f), CAT89_OK);
    T_STATUS(grab_mor(cat, enumeration, 5, &g), CAT89_OK);
    T_ASSERT(f && g);
    st = cat89_find_equalizer(cat, enumeration, eq, f, g, NULL, &limit, NULL,
                              NULL);
    T_STATUS(st, CAT89_NOT_FOUND);
    T_ASSERT(limit == NULL);
    cat89_mor_release(cat, f);
    cat89_mor_release(cat, g);
    cat89_enum_release(enumeration);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

static void test_equalizer_invalid(void)
{
    cat89_category *cat;
    cat89_enum *enumeration;
    cat89_eq *eq;
    cat89_mor *f;
    cat89_mor *g;
    cat89_limit *limit;
    cat89_status st;

    cat = NULL;
    enumeration = NULL;
    eq = NULL;
    limit = NULL;
    T_STATUS(cat89_fixture_build(CAT89_FIX_EQUALIZER, &cat, &eq, &enumeration),
             CAT89_OK);
    f = NULL;
    g = NULL;
    T_STATUS(grab_mor(cat, enumeration, 5, &f), CAT89_OK);
    T_STATUS(grab_mor(cat, enumeration, 6, &g), CAT89_OK);
    st = cat89_find_equalizer(cat, enumeration, NULL, f, g, NULL, &limit, NULL,
                              NULL);
    T_STATUS(st, CAT89_INVALID);
    T_ASSERT(limit == NULL);
    cat89_mor_release(cat, f);
    cat89_mor_release(cat, g);
    cat89_enum_release(enumeration);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    test_equalizer_found();
    test_equalizer_absent();
    test_equalizer_invalid();
    return T_END() ? 0 : 1;
}
