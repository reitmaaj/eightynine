/* cat89_test_exhaustive_families.c - exhaustive derived law-family sweeps (W5).
 *
 * Exercises the library exhaustive sweep functions (iso / split mono / split
 * epi / functor / naturality / cone / cocone) over the small FULL reference
 * fixtures, where identity and associativity hold and the law counts are exact
 * and well-defined, plus the CAT89_NOT_SUPPORTED guard when a required
 * capability (equality or an enumeration) is absent, and a must-reject case
 * (a non-commuting cone reports failing cells). Test code is not green-owned.
 */
#include <stdlib.h>

#include <cat89/cat89.h>
#include <finite_categories.h>
#include <test.h>

int cat89_test_failures = 0;

struct amb
{
    cat89_category *cat;
    cat89_eq *eq;
    cat89_enum *en;
    cat89_mor **mors;
    unsigned long n;
};

static unsigned long collect_mors(cat89_category *cat, cat89_enum *en,
                                  cat89_mor ***out)
{
    cat89_mor_iter *it;
    cat89_mor *mor;
    cat89_mor **arr;
    int done;
    unsigned long n;

    it = NULL;
    if (cat89_mor_iter_open(en, &it) != CAT89_OK)
    {
        return 0;
    }
    done = 0;
    n = 0;
    while (!done)
    {
        mor = NULL;
        cat89_mor_iter_next(en, it, &mor, &done);
        if (!done)
        {
            cat89_mor_release(cat, mor);
            n = n + 1;
        }
    }
    cat89_mor_iter_close(en, it);
    arr = (cat89_mor **)malloc(n * sizeof(cat89_mor *));
    if (arr == NULL)
    {
        return 0;
    }
    it = NULL;
    if (cat89_mor_iter_open(en, &it) != CAT89_OK)
    {
        free(arr);
        return 0;
    }
    done = 0;
    n = 0;
    while (!done)
    {
        mor = NULL;
        cat89_mor_iter_next(en, it, &mor, &done);
        if (!done)
        {
            arr[n] = mor;
            n = n + 1;
        }
    }
    cat89_mor_iter_close(en, it);
    *out = arr;
    return n;
}

static void amb_open(enum cat89_fixture_kind kind, struct amb *a)
{
    a->cat = NULL;
    a->eq = NULL;
    a->en = NULL;
    a->mors = NULL;
    a->n = 0;
    T_STATUS(cat89_fixture_build(kind, &a->cat, &a->eq, &a->en), CAT89_OK);
    a->n = collect_mors(a->cat, a->en, &a->mors);
    T_ASSERT(a->n > 0);
}

static void amb_close(struct amb *a)
{
    unsigned long i;

    for (i = 0; i < a->n; i = i + 1)
    {
        cat89_mor_release(a->cat, a->mors[i]);
    }
    free(a->mors);
    cat89_enum_release(a->en);
    cat89_eq_release(a->eq);
    cat89_category_release(a->cat);
}

static void expect_zero_failed(cat89_status st)
{
    T_ASSERT(st == CAT89_OK);
}

static void test_iso_sweep(void)
{
    struct amb a;
    cat89_check_result r;

    /* C2_GROUPOID: genuine isos are (e,e) and (s,s); 2 pairs x 2 cases = 4. */
    amb_open(CAT89_FIX_C2_GROUPOID, &a);
    r.checked = 0;
    r.failed = 0;
    r.status = CAT89_OK;
    expect_zero_failed(cat89_check_iso_exhaustive(a.cat, a.eq, a.en, &r));
    T_EQ_UL(r.checked, 4);
    T_EQ_UL(r.failed, 0);
    amb_close(&a);

    /* TERMINAL: single identity, one iso pair => 2 cases. */
    amb_open(CAT89_FIX_TERMINAL, &a);
    r.checked = 0;
    r.failed = 0;
    r.status = CAT89_OK;
    expect_zero_failed(cat89_check_iso_exhaustive(a.cat, a.eq, a.en, &r));
    T_EQ_UL(r.checked, 2);
    T_EQ_UL(r.failed, 0);
    amb_close(&a);
}

static void test_split_sweeps(void)
{
    struct amb a;
    cat89_check_result r;

    /* C2_GROUPOID: split (section, retraction) pairs are (e,e) and (s,s); the
     * mixed pairs fail r o s == 1 and are not genuine splits. */
    amb_open(CAT89_FIX_C2_GROUPOID, &a);
    r.checked = 0;
    r.failed = 0;
    r.status = CAT89_OK;
    expect_zero_failed(
        cat89_check_split_mono_exhaustive(a.cat, a.eq, a.en, &r));
    T_EQ_UL(r.checked, 2);
    T_EQ_UL(r.failed, 0);
    r.checked = 0;
    r.failed = 0;
    r.status = CAT89_OK;
    expect_zero_failed(cat89_check_split_epi_exhaustive(a.cat, a.eq, a.en, &r));
    T_EQ_UL(r.checked, 2);
    T_EQ_UL(r.failed, 0);
    amb_close(&a);
}

static void test_functor_sweep(void)
{
    struct amb a;
    cat89_functor *fid;
    cat89_check_result r;

    /* Identity functor over DISCRETE2: 2 objects + 2 composable pairs. */
    amb_open(CAT89_FIX_DISCRETE2, &a);
    fid = NULL;
    T_STATUS(cat89_functor_identity(a.cat, NULL, &fid), CAT89_OK);
    r.checked = 0;
    r.failed = 0;
    r.status = CAT89_OK;
    expect_zero_failed(cat89_check_functor_exhaustive(fid, a.eq, a.en, &r));
    T_EQ_UL(r.checked, 4);
    T_EQ_UL(r.failed, 0);
    cat89_functor_release(fid);
    amb_close(&a);
}

static void test_naturality_sweep(void)
{
    struct amb a;
    cat89_functor *fid;
    cat89_nat *nid;
    cat89_check_result r;

    amb_open(CAT89_FIX_DISCRETE2, &a);
    fid = NULL;
    nid = NULL;
    T_STATUS(cat89_functor_identity(a.cat, NULL, &fid), CAT89_OK);
    T_STATUS(cat89_nat_identity(fid, NULL, &nid), CAT89_OK);
    r.checked = 0;
    r.failed = 0;
    r.status = CAT89_OK;
    expect_zero_failed(cat89_check_naturality_exhaustive(nid, a.eq, a.en, &r));
    T_EQ_UL(r.checked, 2);
    T_EQ_UL(r.failed, 0);
    cat89_nat_release(nid);
    cat89_functor_release(fid);
    amb_close(&a);
}

static cat89_status leg_id(void *ctx, const cat89_obj *obj, cat89_mor **out)
{
    return cat89_identity((cat89_category *)ctx, obj, out);
}

static const cat89_cone_ops cone_ops = {leg_id, NULL};

static const cat89_obj *first_obj(const struct amb *a)
{
    const cat89_obj *obj;

    obj = NULL;
    if (cat89_dom(a->cat, a->mors[0], &obj) != CAT89_OK)
    {
        return NULL;
    }
    return obj;
}

static void test_cone_sweeps(void)
{
    struct amb a;
    cat89_functor *dia;
    cat89_cone *cone;
    cat89_cocone *cocone;
    const cat89_obj *apex;
    cat89_check_result r;

    /* TERMINAL: the identity-functor cone over the single object commutes on
     * its only shape morph (the identity). One cell, no failures. */
    amb_open(CAT89_FIX_TERMINAL, &a);
    dia = NULL;
    cone = NULL;
    cocone = NULL;
    T_STATUS(cat89_functor_identity(a.cat, NULL, &dia), CAT89_OK);
    apex = first_obj(&a);
    T_ASSERT(apex != NULL);
    T_STATUS(cat89_cone_new(dia, apex, &cone_ops, a.cat, NULL, &cone),
             CAT89_OK);
    T_STATUS(cat89_cocone_new(dia, apex, &cone_ops, a.cat, NULL, &cocone),
             CAT89_OK);
    r.checked = 0;
    r.failed = 0;
    r.status = CAT89_OK;
    expect_zero_failed(cat89_check_cone_exhaustive(cone, a.eq, a.en, &r));
    T_EQ_UL(r.checked, 1);
    T_EQ_UL(r.failed, 0);
    r.checked = 0;
    r.failed = 0;
    r.status = CAT89_OK;
    expect_zero_failed(cat89_check_cocone_exhaustive(cocone, a.eq, a.en, &r));
    T_EQ_UL(r.checked, 1);
    T_EQ_UL(r.failed, 0);
    cat89_cocone_release(cocone);
    cat89_cone_release(cone);
    cat89_functor_release(dia);
    amb_close(&a);
}

static void test_cone_must_reject(void)
{
    struct amb a;
    cat89_functor *dia;
    cat89_cone *cone;
    const cat89_obj *apex;
    cat89_check_result r;

    /* C2_GROUPOID with an identity apex/leg cone: the shape morph s does NOT
     * commute (D(s) o leg != leg), so the cone sweep reports a failing cell. */
    amb_open(CAT89_FIX_C2_GROUPOID, &a);
    dia = NULL;
    cone = NULL;
    T_STATUS(cat89_functor_identity(a.cat, NULL, &dia), CAT89_OK);
    apex = first_obj(&a);
    T_ASSERT(apex != NULL);
    T_STATUS(cat89_cone_new(dia, apex, &cone_ops, a.cat, NULL, &cone),
             CAT89_OK);
    r.checked = 0;
    r.failed = 0;
    r.status = CAT89_OK;
    expect_zero_failed(cat89_check_cone_exhaustive(cone, a.eq, a.en, &r));
    T_EQ_UL(r.checked, 2);
    T_ASSERT(r.failed > 0);
    cat89_cone_release(cone);
    cat89_functor_release(dia);
    amb_close(&a);
}

static void test_not_supported(void)
{
    struct amb a;
    cat89_check_result r;

    r.checked = 0;
    r.failed = 0;
    r.status = CAT89_OK;
    amb_open(CAT89_FIX_TERMINAL, &a);
    /* A null enumeration or equality is a missing capability: NOT_SUPPORTED. */
    T_STATUS(cat89_check_iso_exhaustive(a.cat, a.eq, NULL, &r),
             CAT89_NOT_SUPPORTED);
    T_STATUS(cat89_check_iso_exhaustive(a.cat, NULL, a.en, &r),
             CAT89_NOT_SUPPORTED);
    T_STATUS(cat89_check_split_mono_exhaustive(a.cat, a.eq, NULL, &r),
             CAT89_NOT_SUPPORTED);
    T_STATUS(cat89_check_split_epi_exhaustive(a.cat, NULL, a.en, &r),
             CAT89_NOT_SUPPORTED);
    T_STATUS(cat89_check_iso_exhaustive(a.cat, a.eq, a.en, NULL),
             CAT89_INVALID);
    amb_close(&a);
}

int main(void)
{
    T_START();
    test_iso_sweep();
    test_split_sweeps();
    test_functor_sweep();
    test_naturality_sweep();
    test_cone_sweeps();
    test_cone_must_reject();
    test_not_supported();
    return T_END() ? 0 : 1;
}
