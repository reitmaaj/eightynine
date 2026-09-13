/* cat89_test_check_functor.c - functor identity/composition + naturality. */
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

static unsigned long collect(cat89_category *cat, cat89_enum *enumeration,
                             cat89_mor **out, unsigned long cap)
{
    cat89_mor_iter *it = NULL;
    cat89_mor *mor = NULL;
    int done = 0;
    unsigned long n = 0;

    if (cat89_mor_iter_open(enumeration, &it) != CAT89_OK)
    {
        return 0;
    }
    done = 0;
    while (!done)
    {
        mor = NULL;
        cat89_mor_iter_next(enumeration, it, &mor, &done);
        if (done)
        {
            break;
        }
        if (n < cap)
        {
            out[n] = mor;
        }
        n = n + 1;
    }
    cat89_mor_iter_close(enumeration, it);
    (void)cat;
    return n;
}

static cat89_status component_id(void *ctx, const cat89_obj *obj,
                                 cat89_mor **out_mor)
{
    cat89_category *category = ctx;

    return cat89_identity(category, obj, out_mor);
}

static void test_functor_laws(void)
{
    cat89_category *cat = NULL;
    cat89_eq *eq = NULL;
    cat89_enum *enumeration = NULL;
    cat89_functor *ident = NULL;
    const cat89_obj *obj = NULL;
    cat89_mor *mors[6];
    cat89_mor *f = NULL;
    cat89_mor *g = NULL;
    int valid = -1;

    T_STATUS(cat89_fixture_build(CAT89_FIX_COMP2, &cat, &eq, &enumeration),
             CAT89_OK);
    collect(cat, enumeration, mors, 6);
    f = mors[3];
    g = mors[4];
    obj = grab_object(enumeration);

    T_STATUS(cat89_functor_identity(cat, NULL, &ident), CAT89_OK);

    valid = -1;
    T_STATUS(cat89_check_functor_identity(ident, eq, obj, &valid), CAT89_OK);
    T_EQ_UL(valid, 1);

    valid = -1;
    T_STATUS(cat89_check_functor_composition(ident, eq, g, f, &valid),
             CAT89_OK);
    T_EQ_UL(valid, 1);

    T_STATUS(cat89_check_functor_identity(NULL, eq, obj, &valid),
             CAT89_INVALID);
    T_STATUS(cat89_check_functor_composition(ident, NULL, g, f, &valid),
             CAT89_INVALID);

    cat89_functor_release(ident);
    {
        unsigned long i;
        for (i = 0; i < 6; i = i + 1)
        {
            cat89_mor_release(cat, mors[i]);
        }
    }
    cat89_eq_release(eq);
    cat89_enum_release(enumeration);
    cat89_category_release(cat);
}

static void test_naturality(void)
{
    cat89_category *cat = NULL;
    cat89_eq *eq = NULL;
    cat89_enum *enumeration = NULL;
    cat89_functor *f1 = NULL;
    cat89_functor *f2 = NULL;
    cat89_nat *nat = NULL;
    cat89_mor *mors[3];
    cat89_mor *arr = NULL;
    int valid = -1;
    cat89_nat_ops ops;

    T_STATUS(cat89_fixture_build(CAT89_FIX_ARROW, &cat, &eq, &enumeration),
             CAT89_OK);
    collect(cat, enumeration, mors, 3);
    arr = mors[2];

    T_STATUS(cat89_functor_identity(cat, NULL, &f1), CAT89_OK);
    T_STATUS(cat89_functor_identity(cat, NULL, &f2), CAT89_OK);
    ops.component = component_id;
    ops.destroy = NULL;
    T_STATUS(cat89_nat_new(f1, f2, &ops, cat, NULL, &nat), CAT89_OK);

    valid = -1;
    T_STATUS(cat89_check_naturality(nat, eq, arr, &valid), CAT89_OK);
    T_EQ_UL(valid, 1);

    T_STATUS(cat89_check_naturality(nat, NULL, arr, &valid), CAT89_INVALID);
    T_STATUS(cat89_check_naturality(NULL, eq, arr, &valid), CAT89_INVALID);

    cat89_nat_release(nat);
    cat89_functor_release(f1);
    cat89_functor_release(f2);
    for (valid = 0; valid < 3; valid = valid + 1)
    {
        cat89_mor_release(cat, mors[valid]);
    }
    cat89_eq_release(eq);
    cat89_enum_release(enumeration);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    test_functor_laws();
    test_naturality();
    return T_END() ? 0 : 1;
}
