/* cat89_test_representation_independence.c - RI01-RI08.
 *
 * The alias backend provides distinct object pointers that denote the same
 * categorical object and morphisms whose stored endpoints use alternate
 * representatives. Generic algorithms must use cat89_obj_same, never C pointer
 * identity. */
#include <alias_backend.h>
#include <cat89/cat89.h>
#include <test.h>

int cat89_test_failures = 0;

static void test_ri01_alias_objects(void)
{
    cat89_category *cat;
    cat89_alias *alias;
    const cat89_obj *a0;
    const cat89_obj *a1;

    cat = NULL;
    alias = NULL;
    T_STATUS(cat89_alias_new(NULL, &cat, &alias), CAT89_OK);
    a0 = cat89_alias_obj(alias, CAT89_ALIAS_A0);
    a1 = cat89_alias_obj(alias, CAT89_ALIAS_A1);
    T_ASSERT(a0 != a1);
    T_ASSERT(cat89_owns_obj(cat, a0) == 1);
    T_ASSERT(cat89_owns_obj(cat, a1) == 1);
    T_ASSERT(cat89_obj_same(cat, a0, a1) == 1);
    T_ASSERT(cat89_obj_same(cat, a0, cat89_alias_obj(alias, CAT89_ALIAS_B0)) ==
             0);
    cat89_category_release(cat);
    cat89_alias_free(alias);
}

static void test_ri02_compose_aliases(void)
{
    cat89_category *cat;
    cat89_alias *alias;
    cat89_eq *eq;
    cat89_mor *f;
    cat89_mor *g;
    cat89_mor *h;
    cat89_mor *gf;
    int equal;

    cat = NULL;
    alias = NULL;
    eq = NULL;
    f = NULL;
    g = NULL;
    h = NULL;
    gf = NULL;
    T_STATUS(cat89_alias_new(NULL, &cat, &alias), CAT89_OK);
    T_STATUS(cat89_alias_eq(alias, &eq), CAT89_OK);
    T_STATUS(cat89_alias_mor(alias, CAT89_ALIAS_F, &f), CAT89_OK);
    T_STATUS(cat89_alias_mor(alias, CAT89_ALIAS_G, &g), CAT89_OK);
    T_STATUS(cat89_alias_mor(alias, CAT89_ALIAS_H, &h), CAT89_OK);
    T_STATUS(cat89_compose(cat, g, f, &gf), CAT89_OK);
    T_ASSERT(gf != NULL);
    equal = 0;
    T_STATUS(cat89_mor_equal(eq, gf, h, &equal), CAT89_OK);
    T_EQ_UL(equal, 1);
    cat89_mor_release(cat, gf);
    cat89_mor_release(cat, h);
    cat89_mor_release(cat, g);
    cat89_mor_release(cat, f);
    cat89_eq_release(eq);
    cat89_category_release(cat);
    cat89_alias_free(alias);
}

static void test_ri03_unique_arrow_finder(void)
{
    cat89_category *cat;
    cat89_alias *alias;
    cat89_eq *eq;
    cat89_enum *en;
    cat89_mor *g;
    cat89_mor *found;
    int equal;

    cat = NULL;
    alias = NULL;
    eq = NULL;
    en = NULL;
    g = NULL;
    found = NULL;
    T_STATUS(cat89_alias_new(NULL, &cat, &alias), CAT89_OK);
    T_STATUS(cat89_alias_eq(alias, &eq), CAT89_OK);
    T_STATUS(cat89_alias_enum(alias, &en), CAT89_OK);
    T_STATUS(cat89_alias_mor(alias, CAT89_ALIAS_G, &g), CAT89_OK);
    /* query with B0 -> C0; the unique arrow g is stored B0 -> C1 */
    T_STATUS(
        cat89_terminal_arrow(cat, en, cat89_alias_obj(alias, CAT89_ALIAS_C0),
                             cat89_alias_obj(alias, CAT89_ALIAS_B0), &found),
        CAT89_OK);
    T_ASSERT(found != NULL);
    equal = 0;
    T_STATUS(cat89_mor_equal(eq, found, g, &equal), CAT89_OK);
    T_EQ_UL(equal, 1);
    cat89_mor_release(cat, found);
    cat89_mor_release(cat, g);
    cat89_enum_release(en);
    cat89_eq_release(eq);
    cat89_category_release(cat);
    cat89_alias_free(alias);
}

static void test_ri04_ri05_terminal_initial(void)
{
    cat89_category *cat;
    cat89_alias *alias;
    cat89_eq *eq;
    cat89_enum *en;
    cat89_mor *h;
    cat89_mor *f;
    cat89_mor *found;
    int equal;

    cat = NULL;
    alias = NULL;
    eq = NULL;
    en = NULL;
    h = NULL;
    f = NULL;
    found = NULL;
    T_STATUS(cat89_alias_new(NULL, &cat, &alias), CAT89_OK);
    T_STATUS(cat89_alias_eq(alias, &eq), CAT89_OK);
    T_STATUS(cat89_alias_enum(alias, &en), CAT89_OK);
    T_STATUS(cat89_alias_mor(alias, CAT89_ALIAS_H, &h), CAT89_OK);
    T_STATUS(cat89_alias_mor(alias, CAT89_ALIAS_F, &f), CAT89_OK);

    /* terminal arrow A0 -> C1 (unique h stored A1 -> C1) */
    T_STATUS(
        cat89_terminal_arrow(cat, en, cat89_alias_obj(alias, CAT89_ALIAS_C1),
                             cat89_alias_obj(alias, CAT89_ALIAS_A0), &found),
        CAT89_OK);
    T_ASSERT(found != NULL);
    equal = 0;
    T_STATUS(cat89_mor_equal(eq, found, h, &equal), CAT89_OK);
    T_EQ_UL(equal, 1);
    cat89_mor_release(cat, found);
    found = NULL;

    /* initial arrow A0 -> B0 (unique f stored A1 -> B1) */
    T_STATUS(
        cat89_initial_arrow(cat, en, cat89_alias_obj(alias, CAT89_ALIAS_A0),
                            cat89_alias_obj(alias, CAT89_ALIAS_B0), &found),
        CAT89_OK);
    T_ASSERT(found != NULL);
    equal = 0;
    T_STATUS(cat89_mor_equal(eq, found, f, &equal), CAT89_OK);
    T_EQ_UL(equal, 1);

    cat89_mor_release(cat, found);
    cat89_mor_release(cat, f);
    cat89_mor_release(cat, h);
    cat89_enum_release(en);
    cat89_eq_release(eq);
    cat89_category_release(cat);
    cat89_alias_free(alias);
}

static void test_ri06_exhaustive_aliases(void)
{
    cat89_category *cat;
    cat89_alias *alias;
    cat89_eq *eq;
    cat89_enum *en;
    cat89_check_result res;

    cat = NULL;
    alias = NULL;
    eq = NULL;
    en = NULL;
    T_STATUS(cat89_alias_new(NULL, &cat, &alias), CAT89_OK);
    T_STATUS(cat89_alias_eq(alias, &eq), CAT89_OK);
    T_STATUS(cat89_alias_enum(alias, &en), CAT89_OK);
    res.checked = 0;
    res.failed = 0;
    res.status = CAT89_CALLBACK;
    T_STATUS(cat89_check_category_exhaustive(cat, eq, en, &res), CAT89_OK);
    T_EQ_UL(res.failed, 0);
    T_ASSERT(res.checked > 0);
    T_STATUS(res.status, CAT89_OK);
    cat89_enum_release(en);
    cat89_eq_release(eq);
    cat89_category_release(cat);
    cat89_alias_free(alias);
}

static void test_ri07_functor_exhaustive_aliases(void)
{
    cat89_category *cat;
    cat89_alias *alias;
    cat89_eq *eq;
    cat89_enum *en;
    cat89_functor *fid;
    cat89_check_result res;

    cat = NULL;
    alias = NULL;
    eq = NULL;
    en = NULL;
    fid = NULL;
    T_STATUS(cat89_alias_new(NULL, &cat, &alias), CAT89_OK);
    T_STATUS(cat89_alias_eq(alias, &eq), CAT89_OK);
    T_STATUS(cat89_alias_enum(alias, &en), CAT89_OK);
    T_STATUS(cat89_functor_identity(cat, NULL, &fid), CAT89_OK);
    res.checked = 0;
    res.failed = 0;
    res.status = CAT89_CALLBACK;
    T_STATUS(cat89_check_functor_exhaustive(fid, eq, en, &res), CAT89_OK);
    T_EQ_UL(res.failed, 0);
    T_ASSERT(res.checked > 0);
    cat89_functor_release(fid);
    cat89_enum_release(en);
    cat89_eq_release(eq);
    cat89_category_release(cat);
    cat89_alias_free(alias);
}

static void test_ri08_findlim_aliases(void)
{
    cat89_category *cat;
    cat89_alias *alias;
    cat89_eq *eq;
    cat89_enum *en;
    cat89_limit *limit;
    cat89_cone *cone;
    const cat89_obj *pa;
    const cat89_obj *pb;
    const cat89_obj *apex;
    const cat89_obj *dm;
    const cat89_obj *cm;
    cat89_mor *la;
    cat89_mor *lb;

    cat = NULL;
    alias = NULL;
    eq = NULL;
    en = NULL;
    limit = NULL;
    la = NULL;
    lb = NULL;
    T_STATUS(cat89_alias_new(NULL, &cat, &alias), CAT89_OK);
    T_STATUS(cat89_alias_eq(alias, &eq), CAT89_OK);
    T_STATUS(cat89_alias_enum(alias, &en), CAT89_OK);
    T_STATUS(cat89_find_binary_product(cat, en, eq,
                                       cat89_alias_obj(alias, CAT89_ALIAS_A0),
                                       cat89_alias_obj(alias, CAT89_ALIAS_B0),
                                       NULL, &limit, &pa, &pb),
             CAT89_OK);
    T_ASSERT(limit != NULL);
    T_ASSERT(pa != NULL);
    T_ASSERT(pb != NULL);
    cone = cat89_limit_cone(limit);
    apex = cat89_cone_apex(cone);
    T_ASSERT(cat89_owns_obj(cat, apex) == 1);
    T_STATUS(cat89_cone_leg(cone, pa, &la), CAT89_OK);
    T_STATUS(cat89_cone_leg(cone, pb, &lb), CAT89_OK);
    dm = NULL;
    cm = NULL;
    T_STATUS(cat89_dom(cat, la, &dm), CAT89_OK);
    T_STATUS(cat89_cod(cat, la, &cm), CAT89_OK);
    T_ASSERT(cat89_obj_same(cat, dm, apex) == 1);
    T_ASSERT(cat89_obj_same(cat, cm, cat89_alias_obj(alias, CAT89_ALIAS_A0)) ==
             1);
    dm = NULL;
    cm = NULL;
    T_STATUS(cat89_cod(cat, lb, &cm), CAT89_OK);
    T_ASSERT(cat89_obj_same(cat, cm, cat89_alias_obj(alias, CAT89_ALIAS_B0)) ==
             1);
    cat89_mor_release(cat, lb);
    cat89_mor_release(cat, la);
    cat89_limit_release(limit);
    cat89_enum_release(en);
    cat89_eq_release(eq);
    cat89_category_release(cat);
    cat89_alias_free(alias);
}

int main(void)
{
    T_START();
    test_ri01_alias_objects();
    test_ri02_compose_aliases();
    test_ri03_unique_arrow_finder();
    test_ri04_ri05_terminal_initial();
    test_ri06_exhaustive_aliases();
    test_ri07_functor_exhaustive_aliases();
    test_ri08_findlim_aliases();
    return T_END() ? 0 : 1;
}
