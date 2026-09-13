/* cat89_test_independence.c - CAT-I5/I6/I7 capability independence.
 *
 * The mock backend offers object/morphism handles but NO eq or enum capability
 * and makes no law claim (morphisms need not "execute" in any sense beyond
 * structural dom/cod/identity). Core, opposite, and functor operations must
 * work on it with no capability at all; only capability-demanding checkers must
 * refuse with CAT89_NOT_SUPPORTED rather than silently succeed. */
#include <stdlib.h>

#include <cat89/cat89.h>
#include <cat89_mock_backend.h>
#include <test.h>

int cat89_test_failures = 0;

static void test_core_works_without_caps(void)
{
    cat89_category *cat;
    cat89_mock *mock;
    const cat89_obj *A;
    const cat89_obj *B;
    const cat89_obj *dm;
    const cat89_obj *cm;
    cat89_mor *id;
    cat89_mor *mor;

    cat = NULL;
    mock = NULL;
    T_STATUS(cat89_mock_new(NULL, &cat, &mock), CAT89_OK);
    A = cat89_mock_obj(mock, CAT89_MOCK_A);
    B = cat89_mock_obj(mock, CAT89_MOCK_B);
    T_ASSERT(A != NULL && B != NULL);

    id = NULL;
    T_STATUS(cat89_identity(cat, A, &id), CAT89_OK);
    T_ASSERT(id != NULL);
    dm = NULL;
    cm = NULL;
    cat89_dom(cat, id, &dm);
    cat89_cod(cat, id, &cm);
    T_ASSERT(dm == A && cm == A);
    cat89_mor_release(cat, id);

    mor = NULL;
    T_STATUS(cat89_mock_mor(mock, CAT89_MOCK_A, CAT89_MOCK_B, &mor), CAT89_OK);
    cat89_dom(cat, mor, &dm);
    cat89_cod(cat, mor, &cm);
    T_ASSERT(dm == A && cm == B);

    cat89_mor_release(cat, mor);
    cat89_category_release(cat);
    cat89_mock_free(mock);
}

static void test_opposite_works_without_caps(void)
{
    cat89_category *cat;
    cat89_mock *mock;
    cat89_category *opp;
    const cat89_obj *dm;
    const cat89_obj *cm;
    cat89_mor *mor;
    cat89_mor *op_mor;

    cat = NULL;
    mock = NULL;
    opp = NULL;
    T_STATUS(cat89_mock_new(NULL, &cat, &mock), CAT89_OK);
    T_STATUS(cat89_opposite_new(cat, NULL, &opp), CAT89_OK);
    T_ASSERT(opp != NULL);

    mor = NULL;
    T_STATUS(cat89_mock_mor(mock, CAT89_MOCK_A, CAT89_MOCK_B, &mor), CAT89_OK);
    op_mor = NULL;
    T_STATUS(cat89_opposite_mor(opp, mor, &op_mor), CAT89_OK);
    T_ASSERT(op_mor != NULL);
    /* opposite reverses the arrow: op dom = B, op cod = A */
    dm = NULL;
    cm = NULL;
    cat89_dom(opp, op_mor, &dm);
    cat89_cod(opp, op_mor, &cm);
    T_ASSERT(dm == cat89_mock_obj(mock, CAT89_MOCK_B));
    T_ASSERT(cm == cat89_mock_obj(mock, CAT89_MOCK_A));

    cat89_mor_release(opp, op_mor);
    cat89_mor_release(cat, mor);
    cat89_category_release(opp);
    cat89_category_release(cat);
    cat89_mock_free(mock);
}

static void test_functor_works_without_caps(void)
{
    cat89_category *cat;
    cat89_mock *mock;
    cat89_functor *fid;
    const cat89_obj *A;
    const cat89_obj *ma;
    cat89_mor *mor;
    cat89_mor *mm;

    cat = NULL;
    mock = NULL;
    fid = NULL;
    T_STATUS(cat89_mock_new(NULL, &cat, &mock), CAT89_OK);
    A = cat89_mock_obj(mock, CAT89_MOCK_A);

    T_STATUS(cat89_functor_identity(cat, NULL, &fid), CAT89_OK);
    T_ASSERT(fid != NULL);
    ma = NULL;
    T_STATUS(cat89_functor_map_obj(fid, A, &ma), CAT89_OK);
    T_ASSERT(ma == A);

    mor = NULL;
    T_STATUS(cat89_mock_mor(mock, CAT89_MOCK_A, CAT89_MOCK_B, &mor), CAT89_OK);
    mm = NULL;
    T_STATUS(cat89_functor_map_mor(fid, mor, &mm), CAT89_OK);
    T_ASSERT(mm != NULL);
    cat89_mor_release(cat, mm);
    cat89_mor_release(cat, mor);

    cat89_functor_release(fid);
    cat89_category_release(cat);
    cat89_mock_free(mock);
}

static void test_capability_demanding_checker_refuses(void)
{
    cat89_category *cat;
    cat89_mock *mock;
    cat89_check_result res;

    cat = NULL;
    mock = NULL;
    T_STATUS(cat89_mock_new(NULL, &cat, &mock), CAT89_OK);

    /* No eq and no enumeration -> exhaustive checker must refuse, not pass. */
    res.checked = 0;
    res.failed = 0;
    T_STATUS(cat89_check_category_exhaustive(cat, NULL, NULL, &res),
             CAT89_NOT_SUPPORTED);

    cat89_category_release(cat);
    cat89_mock_free(mock);
}

int main(void)
{
    T_START();
    test_core_works_without_caps();
    test_opposite_works_without_caps();
    test_functor_works_without_caps();
    test_capability_demanding_checker_refuses();
    return T_END() ? 0 : 1;
}
