/* cat89_test_capability_provenance.c - typed capability provenance.
 *
 * Every API that receives both a category-bearing structure and a category-
 * bearing capability must reject mismatched category instances with
 * CAT89_INVALID before any equality/enumeration callback. Poison capabilities
 * count invocations. */
#include <cat89/cat89.h>
#include <cat89_mock_backend.h>
#include <finite_categories.h>
#include <test.h>

int cat89_test_failures = 0;

/* ------------------------------------------------------- poison helpers */

struct poison_eq
{
    unsigned long calls;
};

static cat89_status peq_obj(void *ctx, const cat89_obj *a, const cat89_obj *b,
                            int *out_equal)
{
    struct poison_eq *p = ctx;
    (void)a;
    (void)b;
    p->calls = p->calls + 1;
    *out_equal = 0;
    return CAT89_CALLBACK;
}

static cat89_status peq_mor(void *ctx, const cat89_mor *f, const cat89_mor *g,
                            int *out_equal)
{
    struct poison_eq *p = ctx;
    (void)f;
    (void)g;
    p->calls = p->calls + 1;
    *out_equal = 0;
    return CAT89_CALLBACK;
}

static const cat89_eq_ops poison_eq_ops = {peq_obj, peq_mor, NULL};

struct poison_enum
{
    unsigned long obj_open;
    unsigned long mor_open;
    unsigned long hom_open;
};

static cat89_status penum_obj_open(void *ctx, cat89_obj_iter **out_iter)
{
    struct poison_enum *p = ctx;
    p->obj_open = p->obj_open + 1;
    *out_iter = NULL;
    return CAT89_CALLBACK;
}

static cat89_status penum_mor_open(void *ctx, cat89_mor_iter **out_iter)
{
    struct poison_enum *p = ctx;
    p->mor_open = p->mor_open + 1;
    *out_iter = NULL;
    return CAT89_CALLBACK;
}

static cat89_status penum_hom_open(void *ctx, const cat89_obj *dom,
                                   const cat89_obj *cod,
                                   cat89_mor_iter **out_iter)
{
    struct poison_enum *p = ctx;
    (void)dom;
    (void)cod;
    p->hom_open = p->hom_open + 1;
    *out_iter = NULL;
    return CAT89_CALLBACK;
}

static const cat89_enum_ops poison_enum_ops = {penum_obj_open, NULL, NULL,
                                               penum_mor_open, NULL, NULL,
                                               penum_hom_open, NULL};

static cat89_status simple_component(void *ctx, const cat89_obj *obj,
                                     cat89_mor **out_mor)
{
    cat89_category *cat = ctx;
    return cat89_identity(cat, obj, out_mor);
}

static const cat89_nat_ops simple_nat_ops = {simple_component, NULL};

/* ------------------------------------------------ TC01/02 slice eq owner */

static void test_slice_coslice_eq_owner(void)
{
    cat89_category *base;
    cat89_eq *eq;
    cat89_enum *en;
    cat89_category *other;
    cat89_mock *mock;
    cat89_eq *wrong;
    const cat89_obj *apex;
    cat89_category *slice;
    cat89_category *coslice;

    base = NULL;
    eq = NULL;
    en = NULL;
    other = NULL;
    mock = NULL;
    wrong = NULL;
    apex = NULL;
    slice = NULL;
    coslice = NULL;
    T_STATUS(cat89_fixture_build(CAT89_FIX_TERMINAL, &base, &eq, &en),
             CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &other, &mock), CAT89_OK);
    T_STATUS(cat89_eq_new(other, &poison_eq_ops, NULL, NULL, &wrong), CAT89_OK);
    {
        cat89_obj_iter *it = NULL;
        int done = 0;
        T_STATUS(cat89_obj_iter_open(en, &it), CAT89_OK);
        T_STATUS(cat89_obj_iter_next(en, it, &apex, &done), CAT89_OK);
        cat89_obj_iter_close(en, it);
    }
    slice = (cat89_category *)(void *)&slice;
    T_STATUS(cat89_slice_category_new(base, wrong, apex, NULL, &slice),
             CAT89_INVALID);
    T_ASSERT(slice == NULL);
    coslice = (cat89_category *)(void *)&coslice;
    T_STATUS(cat89_coslice_category_new(base, wrong, apex, NULL, &coslice),
             CAT89_INVALID);
    T_ASSERT(coslice == NULL);
    cat89_eq_release(wrong);
    cat89_category_release(other);
    cat89_mock_free(mock);
    cat89_enum_release(en);
    cat89_eq_release(eq);
    cat89_category_release(base);
}

/* ------------------------------------------- TC03-05 local law checks */

static void test_law_check_eq_owner(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    cat89_eq *eq2;
    struct poison_eq p;
    cat89_mor *f;
    int valid;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    eq2 = NULL;
    f = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    p.calls = 0;
    T_STATUS(cat89_eq_new(c2, &poison_eq_ops, &p, NULL, &eq2), CAT89_OK);
    T_STATUS(cat89_mock_mor(m1, CAT89_MOCK_A, CAT89_MOCK_B, &f), CAT89_OK);
    valid = 7;
    T_STATUS(cat89_check_left_identity(c1, eq2, f, &valid), CAT89_INVALID);
    T_EQ_UL(valid, 0);
    valid = 7;
    T_STATUS(cat89_check_right_identity(c1, eq2, f, &valid), CAT89_INVALID);
    T_EQ_UL(valid, 0);
    valid = 7;
    T_STATUS(cat89_check_associativity(c1, eq2, f, f, f, &valid),
             CAT89_INVALID);
    T_EQ_UL(valid, 0);
    T_EQ_UL(p.calls, 0);
    cat89_mor_release(c1, f);
    cat89_eq_release(eq2);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

/* ------------------------------------- TC06-08 iso/split eq ownership */

static void test_iso_split_eq_owner(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    cat89_eq *eq2;
    struct poison_eq p;
    cat89_mor *f;
    cat89_mor *g;
    cat89_iso *iso;
    cat89_split_mono *mono;
    cat89_split_epi *epi;
    int valid;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    eq2 = NULL;
    f = NULL;
    g = NULL;
    iso = NULL;
    mono = NULL;
    epi = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    p.calls = 0;
    T_STATUS(cat89_eq_new(c2, &poison_eq_ops, &p, NULL, &eq2), CAT89_OK);
    T_STATUS(cat89_mock_mor(m1, CAT89_MOCK_A, CAT89_MOCK_B, &f), CAT89_OK);
    T_STATUS(cat89_mock_mor(m1, CAT89_MOCK_B, CAT89_MOCK_A, &g), CAT89_OK);
    T_STATUS(cat89_iso_new(c1, f, g, NULL, &iso), CAT89_OK);
    T_STATUS(cat89_split_mono_new(c1, f, g, NULL, &mono), CAT89_OK);
    T_STATUS(cat89_split_epi_new(c1, f, g, NULL, &epi), CAT89_OK);

    valid = 7;
    T_STATUS(cat89_check_iso(iso, eq2, &valid), CAT89_INVALID);
    T_EQ_UL(valid, 0);
    valid = 7;
    T_STATUS(cat89_check_split_mono(mono, eq2, &valid), CAT89_INVALID);
    T_EQ_UL(valid, 0);
    valid = 7;
    T_STATUS(cat89_check_split_epi(epi, eq2, &valid), CAT89_INVALID);
    T_EQ_UL(valid, 0);
    T_EQ_UL(p.calls, 0);

    cat89_split_epi_release(epi);
    cat89_split_mono_release(mono);
    cat89_iso_release(iso);
    cat89_mor_release(c1, g);
    cat89_mor_release(c1, f);
    cat89_eq_release(eq2);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

/* ------------------------------- TC09-11 functor/naturality eq owner */

static void test_functor_nat_eq_owner(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    cat89_eq *eq2;
    struct poison_eq p;
    cat89_functor *f;
    cat89_functor *g;
    cat89_nat *nat;
    cat89_mor *mor;
    int valid;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    eq2 = NULL;
    f = NULL;
    g = NULL;
    nat = NULL;
    mor = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    p.calls = 0;
    T_STATUS(cat89_eq_new(c2, &poison_eq_ops, &p, NULL, &eq2), CAT89_OK);
    T_STATUS(cat89_functor_identity(c1, NULL, &f), CAT89_OK);
    T_STATUS(cat89_functor_identity(c1, NULL, &g), CAT89_OK);
    T_STATUS(cat89_nat_new(f, g, &simple_nat_ops, c1, NULL, &nat), CAT89_OK);
    T_STATUS(cat89_mock_mor(m1, CAT89_MOCK_A, CAT89_MOCK_B, &mor), CAT89_OK);
    valid = 7;
    T_STATUS(cat89_check_functor_identity(
                 f, eq2, cat89_mock_obj(m1, CAT89_MOCK_A), &valid),
             CAT89_INVALID);
    T_EQ_UL(valid, 0);
    valid = 7;
    T_STATUS(cat89_check_functor_composition(f, eq2, mor, mor, &valid),
             CAT89_INVALID);
    T_EQ_UL(valid, 0);
    valid = 7;
    T_STATUS(cat89_check_naturality(nat, eq2, mor, &valid), CAT89_INVALID);
    T_EQ_UL(valid, 0);
    T_EQ_UL(p.calls, 0);
    cat89_nat_release(nat);
    cat89_mor_release(c1, mor);
    cat89_functor_release(g);
    cat89_functor_release(f);
    cat89_eq_release(eq2);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

/* --------------------------------------- TC12-15 cone/cocone checks */

struct cone_ctx
{
    cat89_category *cat;
    const cat89_obj *obj;
};

static cat89_status cone_leg(void *ctx, const cat89_obj *shape_obj,
                             cat89_mor **out_mor)
{
    struct cone_ctx *c = ctx;
    (void)shape_obj;
    return cat89_identity(c->cat, c->obj, out_mor);
}

static cat89_status cone_map_obj(void *ctx, const cat89_obj *obj,
                                 const cat89_obj **out_obj)
{
    struct cone_ctx *c = ctx;
    (void)obj;
    *out_obj = c->obj;
    return CAT89_OK;
}

static cat89_status cone_map_mor(void *ctx, cat89_mor *mor, cat89_mor **out_mor)
{
    struct cone_ctx *c = ctx;
    (void)mor;
    return cat89_identity(c->cat, c->obj, out_mor);
}

static const cat89_cone_ops cone_ops = {cone_leg, NULL};
static const cat89_functor_ops cone_fun_ops = {cone_map_obj, cone_map_mor,
                                               NULL};

static void test_cone_check_provenance(void)
{
    cat89_category *cat;
    cat89_eq *eq;
    cat89_enum *en;
    cat89_category *shape;
    cat89_eq *seq;
    cat89_enum *sen;
    cat89_functor *diagram;
    cat89_cone *cone;
    cat89_cocone *cocone;
    struct cone_ctx cc;
    cat89_category *other;
    cat89_mock *mock;
    cat89_eq *wrong_eq;
    cat89_enum *wrong_en;
    struct poison_eq pe;
    struct poison_enum pn;
    const cat89_obj *apex;
    int valid;

    cat = NULL;
    eq = NULL;
    en = NULL;
    shape = NULL;
    seq = NULL;
    sen = NULL;
    diagram = NULL;
    cone = NULL;
    cocone = NULL;
    other = NULL;
    mock = NULL;
    wrong_eq = NULL;
    wrong_en = NULL;
    apex = NULL;
    T_STATUS(cat89_fixture_build(CAT89_FIX_TERMINAL, &cat, &eq, &en), CAT89_OK);
    T_STATUS(cat89_shape_category_new(CAT89_SHAPE_DISCRETE2, NULL, &shape, &seq,
                                      &sen),
             CAT89_OK);
    {
        cat89_obj_iter *it = NULL;
        int done = 0;
        T_STATUS(cat89_obj_iter_open(en, &it), CAT89_OK);
        T_STATUS(cat89_obj_iter_next(en, it, &apex, &done), CAT89_OK);
        cat89_obj_iter_close(en, it);
    }
    cc.cat = cat;
    cc.obj = apex;
    T_STATUS(cat89_functor_new(shape, cat, &cone_fun_ops, &cc, NULL, &diagram),
             CAT89_OK);
    T_STATUS(cat89_cone_new(diagram, apex, &cone_ops, &cc, NULL, &cone),
             CAT89_OK);
    T_STATUS(cat89_cocone_new(diagram, apex, &cone_ops, &cc, NULL, &cocone),
             CAT89_OK);

    T_STATUS(cat89_mock_new(NULL, &other, &mock), CAT89_OK);
    pe.calls = 0;
    pn.obj_open = 0;
    pn.mor_open = 0;
    pn.hom_open = 0;
    T_STATUS(cat89_eq_new(other, &poison_eq_ops, &pe, NULL, &wrong_eq),
             CAT89_OK);
    T_STATUS(cat89_enum_new(other, &poison_enum_ops, &pn, NULL, &wrong_en),
             CAT89_OK);

    /* wrong equality */
    valid = 7;
    T_STATUS(cat89_check_cone(cone, wrong_eq, sen, &valid), CAT89_INVALID);
    T_EQ_UL(valid, 0);
    valid = 7;
    T_STATUS(cat89_check_cocone(cocone, wrong_eq, sen, &valid), CAT89_INVALID);
    T_EQ_UL(valid, 0);
    {
        cat89_check_result res;
        res.checked = 9;
        res.failed = 9;
        res.status = CAT89_OK;
        T_STATUS(cat89_check_cone_exhaustive(cone, wrong_eq, sen, &res),
                 CAT89_INVALID);
        res.checked = 9;
        res.failed = 9;
        T_STATUS(cat89_check_cocone_exhaustive(cocone, wrong_eq, sen, &res),
                 CAT89_INVALID);
    }
    T_EQ_UL(pe.calls, 0);

    /* wrong shape enumeration */
    valid = 7;
    T_STATUS(cat89_check_cone(cone, eq, wrong_en, &valid), CAT89_INVALID);
    T_EQ_UL(valid, 0);
    valid = 7;
    T_STATUS(cat89_check_cocone(cocone, eq, wrong_en, &valid), CAT89_INVALID);
    T_EQ_UL(valid, 0);
    {
        cat89_check_result res;
        res.checked = 9;
        res.failed = 9;
        res.status = CAT89_OK;
        T_STATUS(cat89_check_cone_exhaustive(cone, eq, wrong_en, &res),
                 CAT89_INVALID);
        res.checked = 9;
        res.failed = 9;
        T_STATUS(cat89_check_cocone_exhaustive(cocone, eq, wrong_en, &res),
                 CAT89_INVALID);
    }
    T_EQ_UL(pn.obj_open, 0);
    T_EQ_UL(pn.mor_open, 0);
    T_EQ_UL(pn.hom_open, 0);

    cat89_enum_release(wrong_en);
    cat89_eq_release(wrong_eq);
    cat89_category_release(other);
    cat89_mock_free(mock);
    cat89_cocone_release(cocone);
    cat89_cone_release(cone);
    cat89_functor_release(diagram);
    cat89_enum_release(sen);
    cat89_eq_release(seq);
    cat89_category_release(shape);
    cat89_enum_release(en);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

/* --------------------------------- TC16-19 category exhaustive owners */

static void test_exhaustive_capability_owner(void)
{
    cat89_category *cat;
    cat89_eq *eq;
    cat89_enum *en;
    cat89_category *other;
    cat89_mock *mock;
    cat89_eq *wrong_eq;
    cat89_enum *wrong_en;
    struct poison_eq pe;
    struct poison_enum pn;
    cat89_check_result res;

    cat = NULL;
    eq = NULL;
    en = NULL;
    other = NULL;
    mock = NULL;
    wrong_eq = NULL;
    wrong_en = NULL;
    T_STATUS(cat89_fixture_build(CAT89_FIX_TERMINAL, &cat, &eq, &en), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &other, &mock), CAT89_OK);
    pe.calls = 0;
    pn.obj_open = 0;
    pn.mor_open = 0;
    pn.hom_open = 0;
    T_STATUS(cat89_eq_new(other, &poison_eq_ops, &pe, NULL, &wrong_eq),
             CAT89_OK);
    T_STATUS(cat89_enum_new(other, &poison_enum_ops, &pn, NULL, &wrong_en),
             CAT89_OK);

    res.checked = 9;
    res.failed = 9;
    T_STATUS(cat89_check_category_exhaustive(cat, wrong_eq, en, &res),
             CAT89_INVALID);
    T_STATUS(cat89_check_iso_exhaustive(cat, wrong_eq, en, &res),
             CAT89_INVALID);
    T_STATUS(cat89_check_split_mono_exhaustive(cat, wrong_eq, en, &res),
             CAT89_INVALID);
    T_STATUS(cat89_check_split_epi_exhaustive(cat, wrong_eq, en, &res),
             CAT89_INVALID);
    T_EQ_UL(pe.calls, 0);

    T_STATUS(cat89_check_category_exhaustive(cat, eq, wrong_en, &res),
             CAT89_INVALID);
    T_STATUS(cat89_check_iso_exhaustive(cat, eq, wrong_en, &res),
             CAT89_INVALID);
    T_STATUS(cat89_check_split_mono_exhaustive(cat, eq, wrong_en, &res),
             CAT89_INVALID);
    T_STATUS(cat89_check_split_epi_exhaustive(cat, eq, wrong_en, &res),
             CAT89_INVALID);
    T_EQ_UL(pn.obj_open, 0);
    T_EQ_UL(pn.mor_open, 0);

    cat89_enum_release(wrong_en);
    cat89_eq_release(wrong_eq);
    cat89_category_release(other);
    cat89_mock_free(mock);
    cat89_enum_release(en);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

/* ------------------------------------ TC20/21 functor exhaustive owners */

static void test_functor_exhaustive_owner(void)
{
    cat89_category *cat;
    cat89_eq *eq;
    cat89_enum *en;
    cat89_category *other;
    cat89_mock *mock;
    cat89_eq *wrong_eq;
    cat89_enum *wrong_en;
    struct poison_eq pe;
    struct poison_enum pn;
    cat89_functor *fid;
    cat89_functor *gid;
    cat89_nat *nat;
    cat89_nat_ops nops;
    cat89_check_result res;

    cat = NULL;
    eq = NULL;
    en = NULL;
    other = NULL;
    mock = NULL;
    wrong_eq = NULL;
    wrong_en = NULL;
    fid = NULL;
    gid = NULL;
    nat = NULL;
    T_STATUS(cat89_fixture_build(CAT89_FIX_TERMINAL, &cat, &eq, &en), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &other, &mock), CAT89_OK);
    pe.calls = 0;
    pn.obj_open = 0;
    pn.mor_open = 0;
    pn.hom_open = 0;
    T_STATUS(cat89_eq_new(other, &poison_eq_ops, &pe, NULL, &wrong_eq),
             CAT89_OK);
    T_STATUS(cat89_enum_new(other, &poison_enum_ops, &pn, NULL, &wrong_en),
             CAT89_OK);
    T_STATUS(cat89_functor_identity(cat, NULL, &fid), CAT89_OK);
    T_STATUS(cat89_functor_identity(cat, NULL, &gid), CAT89_OK);
    nops.component = NULL;
    T_STATUS(cat89_nat_new(fid, gid, &nops, NULL, NULL, &nat), CAT89_INVALID);

    res.checked = 9;
    res.failed = 9;
    T_STATUS(cat89_check_functor_exhaustive(fid, wrong_eq, en, &res),
             CAT89_INVALID);
    T_STATUS(cat89_check_functor_exhaustive(fid, eq, wrong_en, &res),
             CAT89_INVALID);
    T_EQ_UL(pe.calls, 0);
    T_EQ_UL(pn.obj_open, 0);

    cat89_enum_release(wrong_en);
    cat89_eq_release(wrong_eq);
    cat89_category_release(other);
    cat89_mock_free(mock);
    cat89_functor_release(gid);
    cat89_functor_release(fid);
    cat89_enum_release(en);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

/* ------------------------------------------- TC22-25 terminal/initial */

static void test_finder_enum_owner(void)
{
    cat89_category *cat;
    cat89_eq *eq;
    cat89_enum *en;
    cat89_category *other;
    cat89_mock *mock;
    cat89_enum *wrong_en;
    struct poison_enum pn;
    const cat89_obj *o;
    cat89_mor *out;

    cat = NULL;
    eq = NULL;
    en = NULL;
    other = NULL;
    mock = NULL;
    wrong_en = NULL;
    o = NULL;
    out = NULL;
    T_STATUS(cat89_fixture_build(CAT89_FIX_TERMINAL, &cat, &eq, &en), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &other, &mock), CAT89_OK);
    pn.obj_open = 0;
    pn.mor_open = 0;
    pn.hom_open = 0;
    T_STATUS(cat89_enum_new(other, &poison_enum_ops, &pn, NULL, &wrong_en),
             CAT89_OK);
    {
        cat89_obj_iter *it = NULL;
        int done = 0;
        T_STATUS(cat89_obj_iter_open(en, &it), CAT89_OK);
        T_STATUS(cat89_obj_iter_next(en, it, &o, &done), CAT89_OK);
        cat89_obj_iter_close(en, it);
    }
    out = (cat89_mor *)(void *)&out;
    {
        const cat89_obj *out_obj = NULL;
        T_STATUS(cat89_find_terminal(cat, wrong_en, &out_obj), CAT89_INVALID);
        T_ASSERT(out_obj == NULL);
        T_STATUS(cat89_find_initial(cat, wrong_en, &out_obj), CAT89_INVALID);
        T_ASSERT(out_obj == NULL);
    }
    T_STATUS(cat89_terminal_arrow(cat, wrong_en, o, o, &out), CAT89_INVALID);
    T_ASSERT(out == NULL);
    T_STATUS(cat89_initial_arrow(cat, wrong_en, o, o, &out), CAT89_INVALID);
    T_ASSERT(out == NULL);
    T_EQ_UL(pn.obj_open, 0);
    T_EQ_UL(pn.mor_open, 0);
    cat89_enum_release(wrong_en);
    cat89_category_release(other);
    cat89_mock_free(mock);
    cat89_enum_release(en);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

/* ---------------------------------------- TC26-31 findlim capability */

static void test_findlim_capability_owner(void)
{
    cat89_category *cat;
    cat89_eq *eq;
    cat89_enum *en;
    cat89_category *other;
    cat89_mock *mock;
    cat89_eq *wrong_eq;
    cat89_enum *wrong_en;
    struct poison_eq pe;
    struct poison_enum pn;
    const cat89_obj *o;
    cat89_mor *m;
    cat89_limit *lim;
    cat89_colimit *colim;

    cat = NULL;
    eq = NULL;
    en = NULL;
    other = NULL;
    mock = NULL;
    wrong_eq = NULL;
    wrong_en = NULL;
    o = NULL;
    m = NULL;
    lim = NULL;
    colim = NULL;
    T_STATUS(cat89_fixture_build(CAT89_FIX_BINPROD, &cat, &eq, &en), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &other, &mock), CAT89_OK);
    pe.calls = 0;
    pn.obj_open = 0;
    pn.mor_open = 0;
    pn.hom_open = 0;
    T_STATUS(cat89_eq_new(other, &poison_eq_ops, &pe, NULL, &wrong_eq),
             CAT89_OK);
    T_STATUS(cat89_enum_new(other, &poison_enum_ops, &pn, NULL, &wrong_en),
             CAT89_OK);
    {
        cat89_obj_iter *it = NULL;
        int done = 0;
        T_STATUS(cat89_obj_iter_open(en, &it), CAT89_OK);
        T_STATUS(cat89_obj_iter_next(en, it, &o, &done), CAT89_OK);
        cat89_obj_iter_close(en, it);
    }
    {
        cat89_mor_iter *it = NULL;
        int done = 0;
        T_STATUS(cat89_mor_iter_open(en, &it), CAT89_OK);
        T_STATUS(cat89_mor_iter_next(en, it, &m, &done), CAT89_OK);
        cat89_mor_iter_close(en, it);
    }

    lim = (cat89_limit *)(void *)&lim;
    T_STATUS(cat89_find_binary_product(cat, wrong_en, eq, o, o, NULL, &lim,
                                       NULL, NULL),
             CAT89_INVALID);
    T_ASSERT(lim == NULL);
    colim = (cat89_colimit *)(void *)&colim;
    T_STATUS(cat89_find_binary_coproduct(cat, wrong_en, eq, o, o, NULL, &colim,
                                         NULL, NULL),
             CAT89_INVALID);
    T_ASSERT(colim == NULL);
    lim = (cat89_limit *)(void *)&lim;
    T_STATUS(
        cat89_find_equalizer(cat, wrong_en, eq, m, m, NULL, &lim, NULL, NULL),
        CAT89_INVALID);
    T_ASSERT(lim == NULL);
    colim = (cat89_colimit *)(void *)&colim;
    T_STATUS(cat89_find_coequalizer(cat, wrong_en, eq, m, m, NULL, &colim, NULL,
                                    NULL),
             CAT89_INVALID);
    T_ASSERT(colim == NULL);
    lim = (cat89_limit *)(void *)&lim;
    T_STATUS(cat89_find_pullback(cat, wrong_en, eq, m, m, NULL, &lim, NULL,
                                 NULL, NULL),
             CAT89_INVALID);
    T_ASSERT(lim == NULL);
    colim = (cat89_colimit *)(void *)&colim;
    T_STATUS(cat89_find_pushout(cat, wrong_en, eq, m, m, NULL, &colim, NULL,
                                NULL, NULL),
             CAT89_INVALID);
    T_ASSERT(colim == NULL);
    T_EQ_UL(pn.obj_open, 0);
    T_EQ_UL(pn.mor_open, 0);

    lim = (cat89_limit *)(void *)&lim;
    T_STATUS(cat89_find_binary_product(cat, en, wrong_eq, o, o, NULL, &lim,
                                       NULL, NULL),
             CAT89_INVALID);
    T_ASSERT(lim == NULL);
    colim = (cat89_colimit *)(void *)&colim;
    T_STATUS(cat89_find_binary_coproduct(cat, en, wrong_eq, o, o, NULL, &colim,
                                         NULL, NULL),
             CAT89_INVALID);
    T_ASSERT(colim == NULL);
    lim = (cat89_limit *)(void *)&lim;
    T_STATUS(
        cat89_find_equalizer(cat, en, wrong_eq, m, m, NULL, &lim, NULL, NULL),
        CAT89_INVALID);
    T_ASSERT(lim == NULL);
    colim = (cat89_colimit *)(void *)&colim;
    T_STATUS(cat89_find_coequalizer(cat, en, wrong_eq, m, m, NULL, &colim, NULL,
                                    NULL),
             CAT89_INVALID);
    T_ASSERT(colim == NULL);
    lim = (cat89_limit *)(void *)&lim;
    T_STATUS(cat89_find_pullback(cat, en, wrong_eq, m, m, NULL, &lim, NULL,
                                 NULL, NULL),
             CAT89_INVALID);
    T_ASSERT(lim == NULL);
    colim = (cat89_colimit *)(void *)&colim;
    T_STATUS(cat89_find_pushout(cat, en, wrong_eq, m, m, NULL, &colim, NULL,
                                NULL, NULL),
             CAT89_INVALID);
    T_ASSERT(colim == NULL);
    T_EQ_UL(pe.calls, 0);

    cat89_mor_release(cat, m);
    cat89_enum_release(wrong_en);
    cat89_eq_release(wrong_eq);
    cat89_category_release(other);
    cat89_mock_free(mock);
    cat89_enum_release(en);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    test_slice_coslice_eq_owner();
    test_law_check_eq_owner();
    test_iso_split_eq_owner();
    test_functor_nat_eq_owner();
    test_cone_check_provenance();
    test_exhaustive_capability_owner();
    test_functor_exhaustive_owner();
    test_finder_enum_owner();
    test_findlim_capability_owner();
    return T_END() ? 0 : 1;
}
