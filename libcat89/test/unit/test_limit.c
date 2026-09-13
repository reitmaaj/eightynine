/* cat89_test_limit.c - limit/colimit with identity mediating factor (C2). */
#include <cat89/cat89.h>
#include <finite_categories.h>
#include <test.h>

int cat89_test_failures = 0;

static const cat89_obj *grab_object(cat89_enum *enumeration)
{
    cat89_obj_iter *it = NULL;
    const cat89_obj *obj = NULL;
    int done = 0;

    if (cat89_obj_iter_open(enumeration, &it) != CAT89_OK)
    {
        return NULL;
    }
    if (cat89_obj_iter_next(enumeration, it, &obj, &done) != CAT89_OK)
    {
        cat89_obj_iter_close(enumeration, it);
        return NULL;
    }
    cat89_obj_iter_close(enumeration, it);
    return obj;
}

static cat89_status leg_identity(void *ctx, const cat89_obj *shape_obj,
                                 cat89_mor **out_mor)
{
    cat89_category *category = ctx;

    return cat89_identity(category, shape_obj, out_mor);
}

static cat89_status factor_cone(void *ctx, const cat89_cone *candidate,
                                cat89_mor **out_mor)
{
    cat89_category *category = ctx;

    return cat89_identity(category, cat89_cone_apex(candidate), out_mor);
}

static cat89_status factor_cocone(void *ctx, const cat89_cocone *candidate,
                                  cat89_mor **out_mor)
{
    cat89_category *category = ctx;

    return cat89_identity(category, cat89_cocone_apex(candidate), out_mor);
}

static void test_limit(void)
{
    cat89_category *cat = NULL;
    cat89_enum *enumeration = NULL;
    cat89_eq *eq = NULL;
    cat89_functor *diagram = NULL;
    cat89_cone *cone = NULL;
    cat89_cocone *cocone = NULL;
    cat89_limit *limit = NULL;
    cat89_colimit *colimit = NULL;
    cat89_cone_ops cops;
    cat89_limit_ops lops;
    cat89_colimit_ops dops;
    const cat89_obj *obj = NULL;
    cat89_mor *leg = NULL;
    cat89_mor *med = NULL;
    const cat89_obj *d = NULL;
    const cat89_obj *cc = NULL;

    T_STATUS(
        cat89_fixture_build(CAT89_FIX_C2_GROUPOID, &cat, &eq, &enumeration),
        CAT89_OK);
    obj = grab_object(enumeration);
    T_ASSERT(obj != NULL);

    T_STATUS(cat89_functor_identity(cat, NULL, &diagram), CAT89_OK);

    cops.leg = leg_identity;
    cops.destroy = NULL;
    T_STATUS(cat89_cone_new(diagram, obj, &cops, cat, NULL, &cone), CAT89_OK);

    lops.factor = factor_cone;
    lops.destroy = NULL;
    T_STATUS(cat89_limit_new(cone, &lops, cat, NULL, &limit), CAT89_OK);
    T_ASSERT(limit != NULL);
    T_ASSERT(cat89_limit_cone(limit) == cone);
    T_ASSERT(cat89_limit_category(limit) == cat);

    /* a candidate cone over the same diagram (apex = the single object). */
    {
        cat89_cone *candidate = NULL;
        T_STATUS(cat89_cone_new(diagram, obj, &cops, cat, NULL, &candidate),
                 CAT89_OK);
        med = NULL;
        T_STATUS(cat89_limit_factor(limit, candidate, &med), CAT89_OK);
        T_ASSERT(med != NULL);
        cat89_dom(cat, med, &d);
        cat89_cod(cat, med, &cc);
        T_ASSERT(cat89_obj_same(cat, d, obj));
        T_ASSERT(cat89_obj_same(cat, cc, obj));
        cat89_mor_release(cat, med);
        cat89_cone_release(candidate);
    }

    /* colimit dual. */
    T_STATUS(cat89_cocone_new(diagram, obj, &cops, cat, NULL, &cocone),
             CAT89_OK);
    dops.factor = factor_cocone;
    dops.destroy = NULL;
    T_STATUS(cat89_colimit_new(cocone, &dops, cat, NULL, &colimit), CAT89_OK);
    T_ASSERT(colimit != NULL);
    {
        cat89_cocone *candidate = NULL;
        T_STATUS(cat89_cocone_new(diagram, obj, &cops, cat, NULL, &candidate),
                 CAT89_OK);
        med = NULL;
        T_STATUS(cat89_colimit_factor(colimit, candidate, &med), CAT89_OK);
        T_ASSERT(med != NULL);
        cat89_mor_release(cat, med);
        cat89_cocone_release(candidate);
    }

    (void)leg;
    cat89_colimit_release(colimit);
    cat89_cocone_release(cocone);
    cat89_limit_release(limit);
    cat89_cone_release(cone);
    cat89_functor_release(diagram);
    cat89_eq_release(eq);
    cat89_enum_release(enumeration);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    test_limit();
    return T_END() ? 0 : 1;
}
