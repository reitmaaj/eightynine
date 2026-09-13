/* cat89_test_thin.c - thin category over a 3-element chain preorder. */
#include <cat89/cat89.h>
#include <cat89/thin.h>
#include <test.h>

int cat89_test_failures = 0;

static int po_same(void *ctx, const void *a, const void *b)
{
    const int *x = a;
    const int *y = b;
    (void)ctx;
    return *x == *y;
}

static cat89_status po_leq(void *ctx, const void *a, const void *b,
                           int *out_leq)
{
    const int *x = a;
    const int *y = b;
    (void)ctx;
    *out_leq = *x <= *y ? 1 : 0;
    return CAT89_OK;
}

static void test_chain(void)
{
    cat89_category *cat = NULL;
    cat89_preorder_ops ops;
    int va = 0;
    int vb = 1;
    int vc = 2;
    const cat89_obj *a = NULL;
    const cat89_obj *b = NULL;
    const cat89_obj *c = NULL;
    cat89_mor *m_ab = NULL;
    cat89_mor *m_bc = NULL;
    cat89_mor *m_ac = NULL;
    cat89_mor *m_ba = NULL;
    cat89_mor *id_a = NULL;
    const cat89_obj *d = NULL;
    const cat89_obj *cc = NULL;

    ops.same = po_same;
    ops.leq = po_leq;
    T_STATUS(cat89_thin_category_new(&ops, NULL, NULL, &cat), CAT89_OK);
    T_ASSERT(cat != NULL);

    T_STATUS(cat89_thin_obj(cat, &va, &a), CAT89_OK);
    T_STATUS(cat89_thin_obj(cat, &vb, &b), CAT89_OK);
    T_STATUS(cat89_thin_obj(cat, &vc, &c), CAT89_OK);

    /* identity on A: dom = cod = A. */
    T_STATUS(cat89_identity(cat, a, &id_a), CAT89_OK);
    T_STATUS(cat89_dom(cat, id_a, &d), CAT89_OK);
    T_STATUS(cat89_cod(cat, id_a, &cc), CAT89_OK);
    T_ASSERT(cat89_obj_same(cat, d, a));
    T_ASSERT(cat89_obj_same(cat, cc, a));
    cat89_mor_release(cat, id_a);

    /* A <= B so there is a morphism A -> B; B <= A is false. */
    T_STATUS(cat89_thin_mor(cat, a, b, &m_ab), CAT89_OK);
    T_ASSERT(m_ab != NULL);
    T_STATUS(cat89_dom(cat, m_ab, &d), CAT89_OK);
    T_STATUS(cat89_cod(cat, m_ab, &cc), CAT89_OK);
    T_ASSERT(cat89_obj_same(cat, d, a));
    T_ASSERT(cat89_obj_same(cat, cc, b));

    T_STATUS(cat89_thin_mor(cat, b, a, &m_ba), CAT89_NOT_FOUND);
    T_ASSERT(m_ba == NULL);

    /* composition A -> B -> C yields A -> C. */
    T_STATUS(cat89_thin_mor(cat, b, c, &m_bc), CAT89_OK);
    T_STATUS(cat89_compose(cat, m_bc, m_ab, &m_ac), CAT89_OK);
    T_ASSERT(m_ac != NULL);
    T_STATUS(cat89_dom(cat, m_ac, &d), CAT89_OK);
    T_STATUS(cat89_cod(cat, m_ac, &cc), CAT89_OK);
    T_ASSERT(cat89_obj_same(cat, d, a));
    T_ASSERT(cat89_obj_same(cat, cc, c));

    /* NULL / misuse handling. */
    {
        cat89_mor *tmp = NULL;
        const cat89_obj *o = NULL;
        T_STATUS(cat89_thin_mor(cat, NULL, c, &tmp), CAT89_INVALID);
        T_STATUS(cat89_thin_obj(NULL, &va, &o), CAT89_INVALID);
    }

    cat89_mor_release(cat, m_ab);
    cat89_mor_release(cat, m_bc);
    cat89_mor_release(cat, m_ac);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    test_chain();
    return T_END() ? 0 : 1;
}
