/* cat89_test_cone.c - cone/cocone over the C2 groupoid as its own diagram. */
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

static void test_cone(void)
{
    cat89_category *cat = NULL;
    cat89_enum *enumeration = NULL;
    cat89_eq *eq = NULL;
    cat89_functor *diagram = NULL;
    cat89_cone *cone = NULL;
    cat89_cocone *cocone = NULL;
    cat89_cone_ops ops;
    const cat89_obj *obj = NULL;
    const cat89_obj *apex = NULL;
    cat89_mor *leg = NULL;
    const cat89_obj *d = NULL;
    const cat89_obj *cc = NULL;
    int valid = -1;

    T_STATUS(
        cat89_fixture_build(CAT89_FIX_C2_GROUPOID, &cat, &eq, &enumeration),
        CAT89_OK);
    obj = grab_object(enumeration);
    T_ASSERT(obj != NULL);
    apex = obj;

    /* diagram: identity on C2 (shape = ambient). */
    T_STATUS(cat89_functor_identity(cat, NULL, &diagram), CAT89_OK);

    ops.leg = leg_identity;
    ops.destroy = NULL;
    T_STATUS(cat89_cone_new(diagram, apex, &ops, cat, NULL, &cone), CAT89_OK);
    T_ASSERT(cone != NULL);
    T_ASSERT(cat89_cone_diagram(cone) == diagram);
    T_ASSERT(cat89_cone_apex(cone) == apex);
    T_ASSERT(cat89_cone_category(cone) == cat);

    /* leg at the single shape object: apex -> D(obj), typed identity. */
    T_STATUS(cat89_cone_leg(cone, obj, &leg), CAT89_OK);
    T_ASSERT(leg != NULL);
    T_STATUS(cat89_dom(cat, leg, &d), CAT89_OK);
    T_STATUS(cat89_cod(cat, leg, &cc), CAT89_OK);
    T_ASSERT(cat89_obj_same(cat, d, apex));
    T_ASSERT(cat89_obj_same(cat, cc, obj));
    valid = -1;
    T_STATUS(cat89_check_right_identity(cat, eq, leg, &valid), CAT89_OK);
    T_EQ_UL(valid, 1);
    cat89_mor_release(cat, leg);

    /* cocone over the same data. */
    T_STATUS(cat89_cocone_new(diagram, apex, &ops, cat, NULL, &cocone),
             CAT89_OK);
    T_ASSERT(cocone != NULL);
    T_STATUS(cat89_cocone_leg(cocone, obj, &leg), CAT89_OK);
    T_ASSERT(leg != NULL);
    cat89_mor_release(cat, leg);

    cat89_cone_release(cone);
    cat89_cocone_release(cocone);
    cat89_functor_release(diagram);
    cat89_eq_release(eq);
    cat89_enum_release(enumeration);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    test_cone();
    return T_END() ? 0 : 1;
}
