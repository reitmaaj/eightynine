/* cat89_test_nat_vertical.c - vertical natural-transformation composition. */
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

static cat89_status component_id(void *ctx, const cat89_obj *obj,
                                 cat89_mor **out_mor)
{
    cat89_category *category = ctx;

    return cat89_identity(category, obj, out_mor);
}

static void test_vertical(void)
{
    cat89_category *cat = NULL;
    cat89_eq *eq = NULL;
    cat89_enum *enumeration = NULL;
    cat89_functor *f1 = NULL;
    cat89_functor *f2 = NULL;
    cat89_functor *f3 = NULL;
    cat89_nat *alpha = NULL;
    cat89_nat *beta = NULL;
    cat89_nat *vertical = NULL;
    cat89_nat *bad = NULL;
    cat89_nat_ops ops;
    const cat89_obj *obj = NULL;
    cat89_mor *comp = NULL;
    const cat89_obj *d = NULL;
    const cat89_obj *cc = NULL;
    int valid = -1;

    T_STATUS(cat89_fixture_build(CAT89_FIX_COMP2, &cat, &eq, &enumeration),
             CAT89_OK);
    obj = grab_object(enumeration);
    T_ASSERT(obj != NULL);

    T_STATUS(cat89_functor_identity(cat, NULL, &f1), CAT89_OK);
    T_STATUS(cat89_functor_identity(cat, NULL, &f2), CAT89_OK);
    T_STATUS(cat89_functor_identity(cat, NULL, &f3), CAT89_OK);

    ops.component = component_id;
    ops.destroy = NULL;

    /* alpha : f1 => f2, beta : f2 => f3 (identity components). */
    T_STATUS(cat89_nat_new(f1, f2, &ops, cat, NULL, &alpha), CAT89_OK);
    T_STATUS(cat89_nat_new(f2, f3, &ops, cat, NULL, &beta), CAT89_OK);
    T_ASSERT(cat89_nat_source(alpha) == f1);
    T_ASSERT(cat89_nat_target(beta) == f3);

    T_STATUS(cat89_nat_compose_vertical(beta, alpha, NULL, &vertical),
             CAT89_OK);
    T_ASSERT(vertical != NULL);
    T_ASSERT(cat89_nat_source(vertical) == f1);
    T_ASSERT(cat89_nat_target(vertical) == f3);

    /* component at obj: beta_A o alpha_A = id o id = id. */
    T_STATUS(cat89_nat_component(vertical, obj, &comp), CAT89_OK);
    T_ASSERT(comp != NULL);
    T_STATUS(cat89_dom(cat, comp, &d), CAT89_OK);
    T_STATUS(cat89_cod(cat, comp, &cc), CAT89_OK);
    T_ASSERT(cat89_obj_same(cat, d, obj));
    T_ASSERT(cat89_obj_same(cat, cc, obj));
    valid = -1;
    T_STATUS(cat89_check_right_identity(cat, eq, comp, &valid), CAT89_OK);
    T_EQ_UL(valid, 1);
    cat89_mor_release(cat, comp);

    /* instance mismatch is rejected. */
    T_STATUS(cat89_nat_compose_vertical(alpha, beta, NULL, &bad),
             CAT89_INVALID);
    T_ASSERT(bad == NULL);

    cat89_nat_release(alpha);
    cat89_nat_release(beta);
    cat89_nat_release(vertical);
    cat89_functor_release(f1);
    cat89_functor_release(f2);
    cat89_functor_release(f3);
    cat89_eq_release(eq);
    cat89_enum_release(enumeration);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    test_vertical();
    return T_END() ? 0 : 1;
}
