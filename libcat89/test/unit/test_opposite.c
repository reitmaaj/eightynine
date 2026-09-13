/* cat89_test_opposite.c - opposite category over the walking composable pair.
 */
#include <cat89/cat89.h>
#include <cat89/opposite.h>
#include <finite_categories.h>
#include <test.h>

int cat89_test_failures = 0;

/* comp2 morphism indices: 0=idA 1=idB 2=idC 3=f 4=g 5=h. */
static unsigned long grab_morphisms(cat89_category *cat,
                                    cat89_enum *enumeration, cat89_mor **out,
                                    unsigned long cap)
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

static void test_opposite_comp2(void)
{
    cat89_category *base = NULL;
    cat89_eq *eq = NULL;
    cat89_enum *enumeration = NULL;
    cat89_category *op = NULL;
    cat89_mor *mors[6];
    cat89_mor *f;
    cat89_mor *g;
    cat89_mor *h;
    cat89_mor *f_op = NULL;
    cat89_mor *g_op = NULL;
    cat89_mor *h_op = NULL;
    cat89_mor *composed = NULL;
    const cat89_obj *a = NULL;
    const cat89_obj *b = NULL;
    const cat89_obj *codf = NULL;
    const cat89_obj *d = NULL;
    const cat89_obj *cc = NULL;
    const cat89_mor *under = NULL;
    int equal = -1;
    unsigned long n;

    T_STATUS(cat89_fixture_build(CAT89_FIX_COMP2, &base, &eq, &enumeration),
             CAT89_OK);
    n = grab_morphisms(base, enumeration, mors, 6);
    T_EQ_UL(n, 6);
    f = mors[3];
    g = mors[4];
    h = mors[5];

    T_STATUS(cat89_opposite_new(base, NULL, &op), CAT89_OK);
    T_ASSERT(op != NULL);

    T_STATUS(cat89_opposite_mor(op, f, &f_op), CAT89_OK);
    T_STATUS(cat89_opposite_mor(op, g, &g_op), CAT89_OK);
    T_STATUS(cat89_opposite_mor(op, h, &h_op), CAT89_OK);

    /* objects are reversed: dom_op(f_op)=cod_base(f), cod_op(f_op)=dom_base(f).
     */
    cat89_dom(base, f, &a);
    cat89_cod(base, f, &b);
    T_STATUS(cat89_dom(op, f_op, &d), CAT89_OK);
    T_STATUS(cat89_cod(op, f_op, &cc), CAT89_OK);
    T_ASSERT(cat89_obj_same(op, d, b));
    T_ASSERT(cat89_obj_same(op, cc, a));

    /* op composition: f_op o g_op has underlying base morphism equal to h. */
    T_STATUS(cat89_compose(op, f_op, g_op, &composed), CAT89_OK);
    T_ASSERT(composed != NULL);
    under = cat89_opposite_base_mor(op, composed);
    T_ASSERT(under != NULL);
    equal = -1;
    T_STATUS(cat89_mor_equal(eq, h, under, &equal), CAT89_OK);
    T_EQ_UL(equal, 1);

    /* dom/cod of the composite in op: dom = cod_base(h), cod = dom_base(h). */
    cat89_cod(base, h, &codf);
    T_STATUS(cat89_dom(op, composed, &d), CAT89_OK);
    T_STATUS(cat89_cod(op, composed, &cc), CAT89_OK);
    T_ASSERT(cat89_obj_same(op, d, codf));
    T_ASSERT(cat89_obj_same(op, cc, a));

    /* identity in op on object A (base dom of f). */
    {
        cat89_mor *idop = NULL;
        cat89_mor *idbase = NULL;
        T_STATUS(cat89_identity(op, a, &idop), CAT89_OK);
        T_STATUS(cat89_identity(base, a, &idbase), CAT89_OK);
        under = cat89_opposite_base_mor(op, idop);
        equal = -1;
        T_STATUS(cat89_mor_equal(eq, idbase, under, &equal), CAT89_OK);
        T_EQ_UL(equal, 1);
        cat89_mor_release(op, idop);
        cat89_mor_release(base, idbase);
    }

    cat89_mor_release(op, f_op);
    cat89_mor_release(op, g_op);
    cat89_mor_release(op, h_op);
    cat89_mor_release(op, composed);
    for (n = 0; n < 6; n = n + 1)
    {
        cat89_mor_release(base, mors[n]);
    }
    cat89_category_release(op);
    cat89_eq_release(eq);
    cat89_enum_release(enumeration);
    cat89_category_release(base);
}

int main(void)
{
    T_START();
    test_opposite_comp2();
    return T_END() ? 0 : 1;
}
