/* cat89_test_bugfix_outnull.c - bugfix reproduction: fallible derived binary
 * ops must null *out before argument validation (bugfix pass).
 *
 * Regression for a contract defect: cat89_iso_compose / split_mono_compose /
 * split_epi_compose / iso_invert / iso_as_split_mono / iso_as_split_epi
 * returned CAT89_INVALID on a NULL handle or a cross-category-instance
 * argument WITHOUT nulling *out, leaving the caller a stale handle it could
 * mistake for owned. Every such failure must leave *out == NULL. */
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

/* C2_GROUPOID morphs: index 1 is the self-inverse s (s o s = e). */
static void test_cross_instance_compose(void)
{
    struct amb a;
    struct amb b;
    cat89_iso *ia;
    cat89_iso *ib;
    cat89_split_mono *sa;
    cat89_split_mono *sb;
    cat89_split_epi *ea;
    cat89_split_epi *eb;
    cat89_iso *out;
    cat89_split_mono *outm;
    cat89_split_epi *oute;
    cat89_status st;

    out = (cat89_iso *)(void *)0x1;
    outm = (cat89_split_mono *)(void *)0x1;
    oute = (cat89_split_epi *)(void *)0x1;
    amb_open(CAT89_FIX_C2_GROUPOID, &a);
    amb_open(CAT89_FIX_C2_GROUPOID, &b);

    T_STATUS(cat89_iso_new(a.cat, a.mors[1], a.mors[1], NULL, &ia), CAT89_OK);
    T_STATUS(cat89_iso_new(b.cat, b.mors[1], b.mors[1], NULL, &ib), CAT89_OK);
    T_STATUS(cat89_split_mono_new(a.cat, a.mors[1], a.mors[1], NULL, &sa),
             CAT89_OK);
    T_STATUS(cat89_split_mono_new(b.cat, b.mors[1], b.mors[1], NULL, &sb),
             CAT89_OK);
    T_STATUS(cat89_split_epi_new(a.cat, a.mors[1], a.mors[1], NULL, &ea),
             CAT89_OK);
    T_STATUS(cat89_split_epi_new(b.cat, b.mors[1], b.mors[1], NULL, &eb),
             CAT89_OK);

    /* cross-category-instance compose must null *out on CAT89_INVALID. */
    out = (cat89_iso *)(void *)0x1;
    st = cat89_iso_compose(ia, ib, NULL, &out);
    T_EQ_UL(st, CAT89_INVALID);
    T_ASSERT(out == NULL);

    outm = (cat89_split_mono *)(void *)0x1;
    st = cat89_split_mono_compose(sa, sb, NULL, &outm);
    T_EQ_UL(st, CAT89_INVALID);
    T_ASSERT(outm == NULL);

    oute = (cat89_split_epi *)(void *)0x1;
    st = cat89_split_epi_compose(ea, eb, NULL, &oute);
    T_EQ_UL(st, CAT89_INVALID);
    T_ASSERT(oute == NULL);

    cat89_split_epi_release(eb);
    cat89_split_epi_release(ea);
    cat89_split_mono_release(sb);
    cat89_split_mono_release(sa);
    cat89_iso_release(ib);
    cat89_iso_release(ia);
    amb_close(&b);
    amb_close(&a);
}

static void test_null_handle(void)
{
    cat89_iso *out;
    cat89_split_mono *outm;
    cat89_split_epi *oute;
    cat89_status st;

    out = (cat89_iso *)(void *)0x1;
    st = cat89_iso_compose(NULL, NULL, NULL, &out);
    T_EQ_UL(st, CAT89_INVALID);
    T_ASSERT(out == NULL);

    out = (cat89_iso *)(void *)0x1;
    st = cat89_iso_invert(NULL, NULL, &out);
    T_EQ_UL(st, CAT89_INVALID);
    T_ASSERT(out == NULL);

    outm = (cat89_split_mono *)(void *)0x1;
    st = cat89_split_mono_compose(NULL, NULL, NULL, &outm);
    T_EQ_UL(st, CAT89_INVALID);
    T_ASSERT(outm == NULL);

    oute = (cat89_split_epi *)(void *)0x1;
    st = cat89_split_epi_compose(NULL, NULL, NULL, &oute);
    T_EQ_UL(st, CAT89_INVALID);
    T_ASSERT(oute == NULL);

    outm = (cat89_split_mono *)(void *)0x1;
    st = cat89_iso_as_split_mono(NULL, NULL, &outm);
    T_EQ_UL(st, CAT89_INVALID);
    T_ASSERT(outm == NULL);

    oute = (cat89_split_epi *)(void *)0x1;
    st = cat89_iso_as_split_epi(NULL, NULL, &oute);
    T_EQ_UL(st, CAT89_INVALID);
    T_ASSERT(oute == NULL);
}

int main(void)
{
    T_START();
    test_cross_instance_compose();
    test_null_handle();
    return T_END() ? 0 : 1;
}
