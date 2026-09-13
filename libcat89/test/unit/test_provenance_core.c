/* cat89_test_provenance_core.c - raw-handle provenance in the core.
 *
 * Uses the counted mock backend (F01): rejection must happen before any
 * representation callback, observable through per-op counters. Also covers the
 * mandatory-owns-vtable contract (OW01/OW02) and exact 0/1 normalization. */
#include <cat89/cat89.h>
#include <cat89_mock_backend.h>
#include <test.h>

int cat89_test_failures = 0;

/* ------------------------------------------------- OW01/OW02 vtable contract
 */

static cat89_status dummy_dom(void *ctx, const cat89_mor *mor,
                              const cat89_obj **out_obj)
{
    (void)ctx;
    (void)mor;
    (void)out_obj;
    return CAT89_OK;
}

static cat89_status dummy_cod(void *ctx, const cat89_mor *mor,
                              const cat89_obj **out_obj)
{
    (void)ctx;
    (void)mor;
    (void)out_obj;
    return CAT89_OK;
}

static cat89_status dummy_identity(void *ctx, const cat89_obj *obj,
                                   cat89_mor **out_mor)
{
    (void)ctx;
    (void)obj;
    (void)out_mor;
    return CAT89_OK;
}

static cat89_status dummy_compose(void *ctx, const cat89_mor *g,
                                  const cat89_mor *f, cat89_mor **out_mor)
{
    (void)ctx;
    (void)g;
    (void)f;
    (void)out_mor;
    return CAT89_OK;
}

static int dummy_obj_same(void *ctx, const cat89_obj *a, const cat89_obj *b)
{
    (void)ctx;
    (void)a;
    (void)b;
    return 0;
}

static cat89_status dummy_retain(void *ctx, cat89_mor *mor)
{
    (void)ctx;
    (void)mor;
    return CAT89_OK;
}

static void dummy_release(void *ctx, cat89_mor *mor)
{
    (void)ctx;
    (void)mor;
}

static int dummy_owns_obj(void *ctx, const cat89_obj *obj)
{
    (void)ctx;
    (void)obj;
    return 0;
}

static int dummy_owns_mor(void *ctx, const cat89_mor *mor)
{
    (void)ctx;
    (void)mor;
    return 0;
}

static void dummy_destroy(void *ctx)
{
    (void)ctx;
}

static void ops_fill(cat89_category_ops *ops)
{
    ops->dom = dummy_dom;
    ops->cod = dummy_cod;
    ops->identity = dummy_identity;
    ops->compose = dummy_compose;
    ops->obj_same = dummy_obj_same;
    ops->mor_retain = dummy_retain;
    ops->mor_release = dummy_release;
    ops->owns_obj = dummy_owns_obj;
    ops->owns_mor = dummy_owns_mor;
    ops->category_destroy = dummy_destroy;
}

static void test_owns_obj_mandatory(void)
{
    cat89_category_ops ops;
    cat89_category *cat;

    cat = NULL;
    ops_fill(&ops);
    ops.owns_obj = NULL;
    T_STATUS(cat89_category_new(&ops, NULL, NULL, &cat), CAT89_INVALID);
    T_ASSERT(cat == NULL);
}

static void test_owns_mor_mandatory(void)
{
    cat89_category_ops ops;
    cat89_category *cat;

    cat = NULL;
    ops_fill(&ops);
    ops.owns_mor = NULL;
    T_STATUS(cat89_category_new(&ops, NULL, NULL, &cat), CAT89_INVALID);
    T_ASSERT(cat == NULL);
}

/* ---------------------------------------------------------- core gating */

static void test_dom_foreign(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    cat89_mor *foreign;
    const cat89_obj *out;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    foreign = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    T_STATUS(cat89_mock_mor(m2, CAT89_MOCK_A, CAT89_MOCK_B, &foreign),
             CAT89_OK);
    cat89_mock_reset_calls(m1);
    out = (const cat89_obj *)(const void *)&out;
    T_STATUS(cat89_dom(c1, foreign, &out), CAT89_INVALID);
    T_ASSERT(out == NULL);
    T_ASSERT(cat89_mock_owns_mor_calls(m1) >= 1);
    T_EQ_UL(cat89_mock_dom_calls(m1), 0);
    cat89_mor_release(c2, foreign);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

static void test_cod_foreign(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    cat89_mor *foreign;
    const cat89_obj *out;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    foreign = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    T_STATUS(cat89_mock_mor(m2, CAT89_MOCK_A, CAT89_MOCK_B, &foreign),
             CAT89_OK);
    cat89_mock_reset_calls(m1);
    out = (const cat89_obj *)(const void *)&out;
    T_STATUS(cat89_cod(c1, foreign, &out), CAT89_INVALID);
    T_ASSERT(out == NULL);
    T_EQ_UL(cat89_mock_cod_calls(m1), 0);
    cat89_mor_release(c2, foreign);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

static void test_identity_foreign(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    const cat89_obj *foreign;
    cat89_mor *out;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    foreign = cat89_mock_obj(m2, CAT89_MOCK_B);
    cat89_mock_reset_calls(m1);
    out = (cat89_mor *)(void *)&out;
    T_STATUS(cat89_identity(c1, foreign, &out), CAT89_INVALID);
    T_ASSERT(out == NULL);
    T_EQ_UL(cat89_mock_identity_calls(m1), 0);
    T_ASSERT(cat89_mock_owns_obj_calls(m1) >= 1);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

static void test_compose_foreign_f(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    cat89_mor *local_g;
    cat89_mor *foreign_f;
    cat89_mor *out;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    local_g = NULL;
    foreign_f = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    T_STATUS(cat89_mock_mor(m1, CAT89_MOCK_B, CAT89_MOCK_C, &local_g),
             CAT89_OK);
    T_STATUS(cat89_mock_mor(m2, CAT89_MOCK_A, CAT89_MOCK_B, &foreign_f),
             CAT89_OK);
    cat89_mock_reset_calls(m1);
    out = (cat89_mor *)(void *)&out;
    T_STATUS(cat89_compose(c1, local_g, foreign_f, &out), CAT89_INVALID);
    T_ASSERT(out == NULL);
    T_EQ_UL(cat89_mock_dom_calls(m1), 0);
    T_EQ_UL(cat89_mock_cod_calls(m1), 0);
    T_EQ_UL(cat89_mock_obj_same_calls(m1), 0);
    T_EQ_UL(cat89_mock_compose_calls(m1), 0);
    T_ASSERT(cat89_mock_owns_mor_calls(m1) >= 1);
    cat89_mor_release(c2, foreign_f);
    cat89_mor_release(c1, local_g);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

static void test_compose_foreign_g(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    cat89_mor *foreign_g;
    cat89_mor *local_f;
    cat89_mor *out;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    foreign_g = NULL;
    local_f = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    T_STATUS(cat89_mock_mor(m2, CAT89_MOCK_B, CAT89_MOCK_C, &foreign_g),
             CAT89_OK);
    T_STATUS(cat89_mock_mor(m1, CAT89_MOCK_A, CAT89_MOCK_B, &local_f),
             CAT89_OK);
    cat89_mock_reset_calls(m1);
    out = (cat89_mor *)(void *)&out;
    T_STATUS(cat89_compose(c1, foreign_g, local_f, &out), CAT89_INVALID);
    T_ASSERT(out == NULL);
    T_EQ_UL(cat89_mock_dom_calls(m1), 0);
    T_EQ_UL(cat89_mock_cod_calls(m1), 0);
    T_EQ_UL(cat89_mock_obj_same_calls(m1), 0);
    T_EQ_UL(cat89_mock_compose_calls(m1), 0);
    cat89_mor_release(c2, foreign_g);
    cat89_mor_release(c1, local_f);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

static void test_compose_both_foreign(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    cat89_mor *f;
    cat89_mor *g;
    cat89_mor *out;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    f = NULL;
    g = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    T_STATUS(cat89_mock_mor(m2, CAT89_MOCK_A, CAT89_MOCK_B, &f), CAT89_OK);
    T_STATUS(cat89_mock_mor(m2, CAT89_MOCK_B, CAT89_MOCK_C, &g), CAT89_OK);
    cat89_mock_reset_calls(m1);
    out = (cat89_mor *)(void *)&out;
    T_STATUS(cat89_compose(c1, g, f, &out), CAT89_INVALID);
    T_ASSERT(out == NULL);
    T_EQ_UL(cat89_mock_dom_calls(m1), 0);
    T_EQ_UL(cat89_mock_cod_calls(m1), 0);
    T_EQ_UL(cat89_mock_obj_same_calls(m1), 0);
    T_EQ_UL(cat89_mock_compose_calls(m1), 0);
    cat89_mor_release(c2, g);
    cat89_mor_release(c2, f);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

static void test_obj_same_foreign(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    const cat89_obj *local;
    const cat89_obj *foreign;
    int same;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    local = cat89_mock_obj(m1, CAT89_MOCK_A);
    foreign = cat89_mock_obj(m2, CAT89_MOCK_A);
    cat89_mock_reset_calls(m1);
    same = cat89_obj_same(c1, local, foreign);
    T_ASSERT(same == 0);
    T_EQ_UL(cat89_mock_obj_same_calls(m1), 0);
    same = cat89_obj_same(c1, foreign, local);
    T_ASSERT(same == 0);
    T_EQ_UL(cat89_mock_obj_same_calls(m1), 0);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

static void test_retain_foreign(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    cat89_mor *foreign;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    foreign = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    T_STATUS(cat89_mock_mor(m2, CAT89_MOCK_A, CAT89_MOCK_B, &foreign),
             CAT89_OK);
    cat89_mock_reset_calls(m1);
    T_STATUS(cat89_mor_retain(c1, foreign), CAT89_INVALID);
    T_EQ_UL(cat89_mock_retain_calls(m1), 0);
    cat89_mor_release(c2, foreign);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

static void test_release_foreign(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    cat89_mor *foreign;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    foreign = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    T_STATUS(cat89_mock_mor(m2, CAT89_MOCK_A, CAT89_MOCK_B, &foreign),
             CAT89_OK);
    cat89_mock_reset_calls(m1);
    cat89_mor_release(c1, foreign);
    T_EQ_UL(cat89_mock_release_calls(m1), 0);
    T_EQ_UL(cat89_mock_mor_free_calls(m1), 0);
    /* the owning category still tears the handle down normally */
    cat89_mor_release(c2, foreign);
    T_EQ_UL(cat89_mock_release_calls(m2), 1);
    T_EQ_UL(cat89_mock_mor_free_calls(m2), 1);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

static void test_valid_compose(void)
{
    cat89_category *c1;
    cat89_mock *m1;
    cat89_mor *f;
    cat89_mor *g;
    cat89_mor *r;
    const cat89_obj *dm;
    const cat89_obj *cm;

    c1 = NULL;
    m1 = NULL;
    f = NULL;
    g = NULL;
    r = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_mor(m1, CAT89_MOCK_A, CAT89_MOCK_B, &f), CAT89_OK);
    T_STATUS(cat89_mock_mor(m1, CAT89_MOCK_B, CAT89_MOCK_C, &g), CAT89_OK);
    cat89_mock_reset_calls(m1);
    T_STATUS(cat89_compose(c1, g, f, &r), CAT89_OK);
    T_ASSERT(r != NULL);
    T_ASSERT(cat89_owns_mor(c1, r) == 1);
    T_ASSERT(cat89_mock_dom_calls(m1) > 0);
    T_ASSERT(cat89_mock_cod_calls(m1) > 0);
    T_ASSERT(cat89_mock_obj_same_calls(m1) > 0);
    T_ASSERT(cat89_mock_compose_calls(m1) > 0);
    dm = NULL;
    cm = NULL;
    T_STATUS(cat89_dom(c1, r, &dm), CAT89_OK);
    T_STATUS(cat89_cod(c1, r, &cm), CAT89_OK);
    T_ASSERT(dm == cat89_mock_obj(m1, CAT89_MOCK_A));
    T_ASSERT(cm == cat89_mock_obj(m1, CAT89_MOCK_C));
    cat89_mor_release(c1, r);
    cat89_mor_release(c1, g);
    cat89_mor_release(c1, f);
    cat89_category_release(c1);
    cat89_mock_free(m1);
}

static void test_null_ownership(void)
{
    cat89_category *c1;
    cat89_mock *m1;

    c1 = NULL;
    m1 = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_ASSERT(cat89_owns_obj(c1, NULL) == 0);
    T_ASSERT(cat89_owns_mor(c1, NULL) == 0);
    T_ASSERT(cat89_owns_obj(NULL, NULL) == 0);
    T_ASSERT(cat89_owns_mor(NULL, NULL) == 0);
    cat89_category_release(c1);
    cat89_mock_free(m1);
}

int main(void)
{
    T_START();
    test_owns_obj_mandatory();
    test_owns_mor_mandatory();
    test_dom_foreign();
    test_cod_foreign();
    test_identity_foreign();
    test_compose_foreign_f();
    test_compose_foreign_g();
    test_compose_both_foreign();
    test_obj_same_foreign();
    test_retain_foreign();
    test_release_foreign();
    test_valid_compose();
    test_null_ownership();
    return T_END() ? 0 : 1;
}
