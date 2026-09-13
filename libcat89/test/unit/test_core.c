/* cat89_test_core.c - Stage 1 category-core unit tests. */
#include <cat89/cat89.h>
#include <cat89_mock_backend.h>
#include <test.h>

int cat89_test_failures = 0;

static void test_constructor_rejects_missing_callbacks(void)
{
    cat89_category *cat = (cat89_category *)(void *)0x1;
    cat89_status st;

    st = cat89_category_new(NULL, NULL, NULL, &cat);
    T_STATUS(st, CAT89_INVALID);
    T_ASSERT(cat == NULL);

    st = cat89_category_new(NULL, NULL, NULL, NULL);
    T_STATUS(st, CAT89_INVALID);
}

static void test_dom_cod(void)
{
    cat89_category *cat = NULL;
    cat89_mock *mock = NULL;
    cat89_mor *f = NULL;
    const cat89_obj *d = NULL;
    const cat89_obj *c = NULL;
    const cat89_obj *a;
    const cat89_obj *b;

    T_STATUS(cat89_mock_new(NULL, &cat, &mock), CAT89_OK);
    a = cat89_mock_obj(mock, CAT89_MOCK_A);
    b = cat89_mock_obj(mock, CAT89_MOCK_B);
    T_STATUS(cat89_mock_mor(mock, CAT89_MOCK_A, CAT89_MOCK_B, &f), CAT89_OK);

    T_STATUS(cat89_dom(cat, f, &d), CAT89_OK);
    T_STATUS(cat89_cod(cat, f, &c), CAT89_OK);
    T_ASSERT(cat89_obj_same(cat, d, a));
    T_ASSERT(cat89_obj_same(cat, c, b));
    T_ASSERT(!cat89_obj_same(cat, a, b));

    T_STATUS(cat89_dom(NULL, f, &d), CAT89_INVALID);
    T_ASSERT(d == NULL);
    T_STATUS(cat89_dom(cat, NULL, &d), CAT89_INVALID);
    T_STATUS(cat89_dom(cat, f, NULL), CAT89_INVALID);

    cat89_mor_release(cat, f);
    cat89_category_release(cat);
    cat89_mock_free(mock);
}

static void test_identity(void)
{
    cat89_category *cat = NULL;
    cat89_mock *mock = NULL;
    cat89_mor *id = NULL;
    cat89_mor *dummy = NULL;
    const cat89_obj *d = NULL;
    const cat89_obj *c = NULL;
    const cat89_obj *a;

    T_STATUS(cat89_mock_new(NULL, &cat, &mock), CAT89_OK);
    a = cat89_mock_obj(mock, CAT89_MOCK_A);
    T_STATUS(cat89_identity(cat, a, &id), CAT89_OK);
    T_ASSERT(id != NULL);
    T_STATUS(cat89_dom(cat, id, &d), CAT89_OK);
    T_STATUS(cat89_cod(cat, id, &c), CAT89_OK);
    T_ASSERT(cat89_obj_same(cat, d, a));
    T_ASSERT(cat89_obj_same(cat, c, a));
    cat89_mor_release(cat, id);
    id = NULL;

    T_STATUS(cat89_identity(NULL, a, &dummy), CAT89_INVALID);
    T_STATUS(cat89_identity(cat, NULL, &dummy), CAT89_INVALID);
    T_STATUS(cat89_identity(cat, a, NULL), CAT89_INVALID);

    cat89_category_release(cat);
    cat89_mock_free(mock);
}

static void test_compose_order_and_typing(void)
{
    cat89_category *cat = NULL;
    cat89_mock *mock = NULL;
    cat89_mor *f = NULL;
    cat89_mor *g = NULL;
    cat89_mor *h = NULL;
    const cat89_obj *a;
    const cat89_obj *c;
    const cat89_obj *d = NULL;
    const cat89_obj *cc = NULL;

    T_STATUS(cat89_mock_new(NULL, &cat, &mock), CAT89_OK);
    a = cat89_mock_obj(mock, CAT89_MOCK_A);
    c = cat89_mock_obj(mock, CAT89_MOCK_C);
    T_STATUS(cat89_mock_mor(mock, CAT89_MOCK_A, CAT89_MOCK_B, &f), CAT89_OK);
    T_STATUS(cat89_mock_mor(mock, CAT89_MOCK_B, CAT89_MOCK_C, &g), CAT89_OK);

    /* g o f is well typed; the generic wrapper calls backend compose(g, f). */
    T_STATUS(cat89_compose(cat, g, f, &h), CAT89_OK);
    T_ASSERT(h != NULL);
    T_STATUS(cat89_dom(cat, h, &d), CAT89_OK);
    T_STATUS(cat89_cod(cat, h, &cc), CAT89_OK);
    T_ASSERT(cat89_obj_same(cat, d, a));
    T_ASSERT(cat89_obj_same(cat, cc, c));

    cat89_mor_release(cat, h);
    cat89_mor_release(cat, g);
    cat89_mor_release(cat, f);
    cat89_category_release(cat);
    cat89_mock_free(mock);
}

static void test_compose_rejects_domain_mismatch(void)
{
    cat89_category *cat = NULL;
    cat89_mock *mock = NULL;
    cat89_mor *f = NULL;
    cat89_mor *q = NULL;
    cat89_mor *h = NULL;
    unsigned long before;

    T_STATUS(cat89_mock_new(NULL, &cat, &mock), CAT89_OK);
    /* f: A->B, q: A->C: cod(f)=B, dom(q)=A, mismatch. */
    T_STATUS(cat89_mock_mor(mock, CAT89_MOCK_A, CAT89_MOCK_B, &f), CAT89_OK);
    T_STATUS(cat89_mock_mor(mock, CAT89_MOCK_A, CAT89_MOCK_C, &q), CAT89_OK);

    h = (cat89_mor *)(void *)0x1;
    before = cat89_mock_compose_calls(mock);
    T_STATUS(cat89_compose(cat, q, f, &h), CAT89_DOMAIN);
    T_ASSERT(h == NULL);
    /* Backend compose must not be invoked when the wrapper prevalidates. */
    T_EQ_UL(cat89_mock_compose_calls(mock), before);

    T_STATUS(cat89_compose(NULL, q, f, &h), CAT89_INVALID);
    T_STATUS(cat89_compose(cat, NULL, f, &h), CAT89_INVALID);
    T_STATUS(cat89_compose(cat, q, NULL, &h), CAT89_INVALID);

    cat89_mor_release(cat, f);
    cat89_mor_release(cat, q);
    cat89_category_release(cat);
    cat89_mock_free(mock);
}

static void test_status_propagation(void)
{
    cat89_category *cat = NULL;
    cat89_mock *mock = NULL;
    cat89_mor *f = NULL;
    cat89_mor *g = NULL;
    cat89_mor *h = NULL;
    const cat89_obj *d = NULL;

    T_STATUS(cat89_mock_new(NULL, &cat, &mock), CAT89_OK);
    T_STATUS(cat89_mock_mor(mock, CAT89_MOCK_A, CAT89_MOCK_B, &f), CAT89_OK);
    T_STATUS(cat89_mock_mor(mock, CAT89_MOCK_B, CAT89_MOCK_C, &g), CAT89_OK);

    cat89_mock_fail_compose(mock, 0, CAT89_NOMEM);
    T_STATUS(cat89_compose(cat, g, f, &h), CAT89_NOMEM);
    T_ASSERT(h == NULL);
    cat89_mock_fail_compose(mock, 0, CAT89_CALLBACK);
    T_STATUS(cat89_compose(cat, g, f, &h), CAT89_CALLBACK);

    cat89_mock_fail_dom(mock, 0, CAT89_NOMEM);
    d = (const cat89_obj *)(void *)0x1;
    T_STATUS(cat89_dom(cat, f, &d), CAT89_NOMEM);
    T_ASSERT(d == NULL);
    cat89_mock_fail_dom(mock, 0, CAT89_OK);

    cat89_mock_fail_cod(mock, 0, CAT89_CALLBACK);
    T_STATUS(cat89_compose(cat, g, f, &h), CAT89_CALLBACK);

    cat89_mor_release(cat, f);
    cat89_mor_release(cat, g);
    cat89_category_release(cat);
    cat89_mock_free(mock);
}

static void test_null_handling_and_release(void)
{
    cat89_category *cat = NULL;
    cat89_mock *mock = NULL;
    cat89_mor *f = NULL;

    T_STATUS(cat89_mock_new(NULL, &cat, &mock), CAT89_OK);
    T_STATUS(cat89_mock_mor(mock, CAT89_MOCK_A, CAT89_MOCK_B, &f), CAT89_OK);

    cat89_mor_release(cat, NULL);
    cat89_category_release(NULL);
    cat89_mor_release(cat, f);
    cat89_category_release(cat);
    T_EQ_UL(cat89_mock_destroy_calls(mock), 1);
    cat89_mock_free(mock);
}

static void test_lifetime_destroy_after_last_release(void)
{
    cat89_category *cat = NULL;
    cat89_category *cat2 = NULL;
    cat89_mock *mock = NULL;
    cat89_mock *mock2 = NULL;
    unsigned long before;

    T_STATUS(cat89_mock_new(NULL, &cat, &mock), CAT89_OK);
    before = cat89_mock_destroy_calls(mock);
    T_STATUS(cat89_category_retain(cat), CAT89_OK);
    T_STATUS(cat89_category_retain(cat), CAT89_OK);
    cat89_category_release(cat);
    cat89_category_release(cat);
    T_EQ_UL(cat89_mock_destroy_calls(mock), before);
    cat89_category_release(cat);
    T_EQ_UL(cat89_mock_destroy_calls(mock), before + 1);
    cat89_mock_free(mock);
    mock = NULL;

    T_STATUS(cat89_mock_new(NULL, &cat2, &mock2), CAT89_OK);
    cat89_category_release(cat2);
    cat89_mock_free(mock2);
}

static void test_category_retain_null(void)
{
    T_STATUS(cat89_category_retain(NULL), CAT89_INVALID);
}

int main(void)
{
    T_START();
    test_constructor_rejects_missing_callbacks();
    test_dom_cod();
    test_identity();
    test_compose_order_and_typing();
    test_compose_rejects_domain_mismatch();
    test_status_propagation();
    test_null_handling_and_release();
    test_lifetime_destroy_after_last_release();
    test_category_retain_null();
    return T_END() ? 0 : 1;
}
