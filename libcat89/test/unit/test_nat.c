/* cat89_test_nat.c - natural transformations core + identity nat. */
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

/* component = identity morphism on the (single-object) arrow? arrow has two
 * objects, but this nat's component just returns id_A which has the right
 * type F(A)=A -> G(A)=A only when we pick the identity nat. For a generic
 * component test over a parallel pair F=G=id we verify typing. */
static cat89_status component_id(void *ctx, const cat89_obj *obj,
                                 cat89_mor **out_mor)
{
    cat89_category *category = ctx;

    return cat89_identity(category, obj, out_mor);
}

static void test_nat_between_identity_functors(void)
{
    cat89_category *cat = NULL;
    cat89_eq *eq = NULL;
    cat89_enum *enumeration = NULL;
    cat89_functor *f = NULL;
    cat89_functor *g = NULL;
    cat89_nat *nat = NULL;
    const cat89_obj *obj = NULL;
    cat89_mor *comp = NULL;
    const cat89_obj *d = NULL;
    const cat89_obj *c = NULL;
    int valid = -1;

    T_STATUS(cat89_fixture_build(CAT89_FIX_ARROW, &cat, &eq, &enumeration),
             CAT89_OK);
    T_STATUS(cat89_functor_identity(cat, NULL, &f), CAT89_OK);
    T_STATUS(cat89_functor_identity(cat, NULL, &g), CAT89_OK);

    /* F and G are parallel (both id_C). */
    {
        cat89_nat_ops ops;
        ops.component = component_id;
        ops.destroy = NULL;
        T_STATUS(cat89_nat_new(f, g, &ops, cat, NULL, &nat), CAT89_OK);
    }
    T_ASSERT(nat != NULL);
    T_ASSERT(cat89_nat_source(nat) == f);
    T_ASSERT(cat89_nat_target(nat) == g);

    obj = grab_object(enumeration);
    T_ASSERT(obj != NULL);
    T_STATUS(cat89_nat_component(nat, obj, &comp), CAT89_OK);
    T_ASSERT(comp != NULL);
    T_STATUS(cat89_dom(cat, comp, &d), CAT89_OK);
    T_STATUS(cat89_cod(cat, comp, &c), CAT89_OK);
    T_ASSERT(cat89_obj_same(cat, d, obj));
    T_ASSERT(cat89_obj_same(cat, c, obj));
    cat89_mor_release(cat, comp);

    /* local identity checker confirms the component is the identity. */
    T_STATUS(cat89_nat_component(nat, obj, &comp), CAT89_OK);
    valid = -1;
    T_STATUS(cat89_check_left_identity(cat, eq, comp, &valid), CAT89_OK);
    T_EQ_UL(valid, 1);
    cat89_mor_release(cat, comp);

    /* identity natural transformation 1_F. */
    cat89_nat_release(nat);
    nat = NULL;
    T_STATUS(cat89_nat_identity(f, NULL, &nat), CAT89_OK);
    T_ASSERT(nat != NULL);
    T_STATUS(cat89_nat_component(nat, obj, &comp), CAT89_OK);
    valid = -1;
    T_STATUS(cat89_check_right_identity(cat, eq, comp, &valid), CAT89_OK);
    T_EQ_UL(valid, 1);
    cat89_mor_release(cat, comp);

    /* instance-mismatched functors are rejected. */
    {
        cat89_category *other = NULL;
        cat89_functor *h = NULL;
        cat89_nat_ops ops;
        cat89_nat *bad = NULL;
        cat89_status st;

        T_STATUS(cat89_fixture_build(CAT89_FIX_TERMINAL, &other, NULL, NULL),
                 CAT89_OK);
        T_STATUS(cat89_functor_identity(other, NULL, &h), CAT89_OK);
        ops.component = component_id;
        ops.destroy = NULL;
        st = cat89_nat_new(f, h, &ops, cat, NULL, &bad);
        T_STATUS(st, CAT89_INVALID);
        T_ASSERT(bad == NULL);
        cat89_functor_release(h);
        cat89_category_release(other);
    }

    cat89_nat_release(nat);
    cat89_functor_release(f);
    cat89_functor_release(g);
    cat89_eq_release(eq);
    cat89_enum_release(enumeration);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    test_nat_between_identity_functors();
    return T_END() ? 0 : 1;
}
