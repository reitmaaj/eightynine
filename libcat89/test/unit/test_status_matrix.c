/* cat89_test_status_matrix.c - callback-failure matrix + no-status-retag
 * regression over the mock backend (W6 continuation).
 *
 * Programs each mock callback (dom/cod/compose/identity/retain) to fail with a
 * distinct cat89_status (CAT89_NOMEM and CAT89_CALLBACK) and confirms the
 * status observed by the caller through the opposite wrapper category (which
 * delegates dom/cod/identity/compose to the base backend and retain on
 * wrapping) is EXACTLY the injected status - never retagged. The regression
 * target is the no-status-retag rule: a backend compose returning CAT89_NOMEM
 * surfaced through wrapper composition must not be observed as CAT89_CALLBACK.
 * Runs clean under ASan+LSan (no live allocation left on a failing callback).
 */
#include <cat89/cat89.h>
#include <cat89_mock_backend.h>
#include <test.h>

int cat89_test_failures = 0;

static void verify_dom_verbatim(cat89_category *cat, cat89_mor *f,
                                cat89_status injected)
{
    const cat89_obj *o;
    cat89_status st;

    o = (const cat89_obj *)(void *)0x1;
    st = cat89_dom(cat, f, &o);
    T_EQ_UL(st, injected);
    T_ASSERT(o == NULL);
}

static void verify_identity_verbatim(cat89_category *cat, const cat89_obj *obj,
                                     cat89_status injected)
{
    cat89_mor *r;
    cat89_status st;

    r = (cat89_mor *)(void *)0x1;
    st = cat89_identity(cat, obj, &r);
    T_EQ_UL(st, injected);
    T_ASSERT(r == NULL);
}

static void test_core_verbatim(void)
{
    cat89_category *cat;
    cat89_mock *mock;
    cat89_mor *f;
    const cat89_obj *a;

    cat = NULL;
    mock = NULL;
    f = NULL;
    T_STATUS(cat89_mock_new(NULL, &cat, &mock), CAT89_OK);
    T_STATUS(cat89_mock_mor(mock, CAT89_MOCK_A, CAT89_MOCK_B, &f), CAT89_OK);
    a = cat89_mock_obj(mock, CAT89_MOCK_A);

    cat89_mock_fail_dom(mock, 0, CAT89_NOMEM);
    verify_dom_verbatim(cat, f, CAT89_NOMEM);
    cat89_mock_fail_dom(mock, 0, CAT89_CALLBACK);
    verify_dom_verbatim(cat, f, CAT89_CALLBACK);
    cat89_mock_fail_dom(mock, 0, CAT89_OK);

    cat89_mock_fail_identity(mock, 0, CAT89_NOMEM);
    verify_identity_verbatim(cat, a, CAT89_NOMEM);
    cat89_mock_fail_identity(mock, 0, CAT89_CALLBACK);
    verify_identity_verbatim(cat, a, CAT89_CALLBACK);
    cat89_mock_fail_identity(mock, 0, CAT89_OK);

    cat89_mor_release(cat, f);
    cat89_category_release(cat);
    cat89_mock_free(mock);
}

static void test_opposite_verbatim(void)
{
    cat89_category *cat;
    cat89_mock *mock;
    cat89_category *op;
    cat89_mor *u;
    cat89_mor *gm;
    cat89_mor *fm;
    cat89_mor *u_op;
    cat89_mor *gm_op;
    cat89_mor *fm_op;
    cat89_mor *r;
    const cat89_obj *o;
    cat89_status st;

    cat = NULL;
    mock = NULL;
    op = NULL;
    u = NULL;
    gm = NULL;
    fm = NULL;
    u_op = NULL;
    gm_op = NULL;
    fm_op = NULL;
    T_STATUS(cat89_mock_new(NULL, &cat, &mock), CAT89_OK);
    /* u: A->B (dom/cod/identity/retain probe), gm: B->A (cod A), fm: A->C
     * (dom A) so gm_op, fm_op compose in the opposite category. */
    T_STATUS(cat89_mock_mor(mock, CAT89_MOCK_A, CAT89_MOCK_B, &u), CAT89_OK);
    T_STATUS(cat89_mock_mor(mock, CAT89_MOCK_B, CAT89_MOCK_A, &gm), CAT89_OK);
    T_STATUS(cat89_mock_mor(mock, CAT89_MOCK_A, CAT89_MOCK_C, &fm), CAT89_OK);

    T_STATUS(cat89_opposite_new(cat, NULL, &op), CAT89_OK);
    T_STATUS(cat89_opposite_mor(op, u, &u_op), CAT89_OK);
    T_STATUS(cat89_opposite_mor(op, gm, &gm_op), CAT89_OK);
    T_STATUS(cat89_opposite_mor(op, fm, &fm_op), CAT89_OK);

    /* sanity: without fault the opposite compose succeeds (locks ordering). */
    r = NULL;
    T_STATUS(cat89_compose(op, gm_op, fm_op, &r), CAT89_OK);
    T_ASSERT(r != NULL);
    cat89_mor_release(op, r);

    /* cod of an opposite morph delegates to base dom: fail_dom surfaces. */
    cat89_mock_fail_dom(mock, 0, CAT89_NOMEM);
    o = (const cat89_obj *)(void *)0x1;
    st = cat89_cod(op, u_op, &o);
    T_EQ_UL(st, CAT89_NOMEM);
    T_ASSERT(o == NULL);
    cat89_mock_fail_dom(mock, 0, CAT89_CALLBACK);
    o = (const cat89_obj *)(void *)0x1;
    st = cat89_cod(op, u_op, &o);
    T_EQ_UL(st, CAT89_CALLBACK);
    T_ASSERT(o == NULL);
    cat89_mock_fail_dom(mock, 0, CAT89_OK);

    /* dom of an opposite morph delegates to base cod: fail_cod surfaces. */
    cat89_mock_fail_cod(mock, 0, CAT89_NOMEM);
    o = (const cat89_obj *)(void *)0x1;
    st = cat89_dom(op, u_op, &o);
    T_EQ_UL(st, CAT89_NOMEM);
    T_ASSERT(o == NULL);
    cat89_mock_fail_cod(mock, 0, CAT89_CALLBACK);
    o = (const cat89_obj *)(void *)0x1;
    st = cat89_dom(op, u_op, &o);
    T_EQ_UL(st, CAT89_CALLBACK);
    T_ASSERT(o == NULL);
    cat89_mock_fail_cod(mock, 0, CAT89_OK);

    /* identity in op delegates to base identity. */
    cat89_mock_fail_identity(mock, 0, CAT89_NOMEM);
    r = (cat89_mor *)(void *)0x1;
    st = cat89_identity(op, cat89_mock_obj(mock, CAT89_MOCK_A), &r);
    T_EQ_UL(st, CAT89_NOMEM);
    T_ASSERT(r == NULL);
    cat89_mock_fail_identity(mock, 0, CAT89_CALLBACK);
    r = (cat89_mor *)(void *)0x1;
    st = cat89_identity(op, cat89_mock_obj(mock, CAT89_MOCK_A), &r);
    T_EQ_UL(st, CAT89_CALLBACK);
    T_ASSERT(r == NULL);
    cat89_mock_fail_identity(mock, 0, CAT89_OK);

    /* compose in op delegates to base compose: NOMEM must not be retagged. */
    cat89_mock_fail_compose(mock, 0, CAT89_NOMEM);
    r = (cat89_mor *)(void *)0x1;
    st = cat89_compose(op, gm_op, fm_op, &r);
    T_EQ_UL(st, CAT89_NOMEM);
    T_ASSERT(r == NULL);
    cat89_mock_fail_compose(mock, 0, CAT89_CALLBACK);
    r = (cat89_mor *)(void *)0x1;
    st = cat89_compose(op, gm_op, fm_op, &r);
    T_EQ_UL(st, CAT89_CALLBACK);
    T_ASSERT(r == NULL);
    cat89_mock_fail_compose(mock, 0, CAT89_OK);

    /* wrapping (opposite_mor) retains the base morph: fail_retain surfaces. */
    cat89_mock_fail_retain(mock, 0, CAT89_NOMEM);
    r = (cat89_mor *)(void *)0x1;
    st = cat89_opposite_mor(op, u, &r);
    T_EQ_UL(st, CAT89_NOMEM);
    T_ASSERT(r == NULL);
    cat89_mock_fail_retain(mock, 0, CAT89_CALLBACK);
    r = (cat89_mor *)(void *)0x1;
    st = cat89_opposite_mor(op, u, &r);
    T_EQ_UL(st, CAT89_CALLBACK);
    T_ASSERT(r == NULL);
    cat89_mock_fail_retain(mock, 0, CAT89_OK);

    cat89_mor_release(op, fm_op);
    cat89_mor_release(op, gm_op);
    cat89_mor_release(op, u_op);
    cat89_category_release(op);
    cat89_mor_release(cat, fm);
    cat89_mor_release(cat, gm);
    cat89_mor_release(cat, u);
    cat89_category_release(cat);
    cat89_mock_free(mock);
}

int main(void)
{
    T_START();
    test_core_verbatim();
    test_opposite_verbatim();
    return T_END() ? 0 : 1;
}
