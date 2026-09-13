/* cat89_test_provenance_api.c - raw provenance across public API families.
 *
 * Poison equality/enumeration capabilities count invocations so a provenance
 * rejection can be proven to precede any effect callback. Counted functor,
 * natural-transformation and cone ops do the same for mapping/leg callbacks. */
#include <cat89/cat89.h>
#include <cat89_mock_backend.h>
#include <finite_categories.h>
#include <test.h>

int cat89_test_failures = 0;

/* --------------------------------------------------- poison capabilities */

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

/* --------------------------------------------------- counted callbacks */

struct count_fun
{
    unsigned long map_obj_calls;
    unsigned long map_mor_calls;
};

static cat89_status cf_map_obj(void *ctx, const cat89_obj *obj,
                               const cat89_obj **out_obj)
{
    struct count_fun *c = ctx;
    c->map_obj_calls = c->map_obj_calls + 1;
    *out_obj = obj;
    return CAT89_OK;
}

static cat89_status cf_map_mor(void *ctx, cat89_mor *mor, cat89_mor **out_mor)
{
    struct count_fun *c = ctx;
    c->map_mor_calls = c->map_mor_calls + 1;
    *out_mor = mor;
    return CAT89_OK;
}

static const cat89_functor_ops count_fun_ops = {cf_map_obj, cf_map_mor, NULL};

struct count_nat
{
    unsigned long comp_calls;
};

static cat89_status cn_component(void *ctx, const cat89_obj *obj,
                                 cat89_mor **out_mor)
{
    struct count_nat *c = ctx;
    c->comp_calls = c->comp_calls + 1;
    (void)obj;
    *out_mor = NULL;
    return CAT89_OK;
}

static const cat89_nat_ops count_nat_ops = {cn_component, NULL};

struct count_leg
{
    unsigned long leg_calls;
};

static cat89_status cl_leg(void *ctx, const cat89_obj *shape_obj,
                           cat89_mor **out_mor)
{
    struct count_leg *c = ctx;
    c->leg_calls = c->leg_calls + 1;
    (void)shape_obj;
    *out_mor = NULL;
    return CAT89_OK;
}

static const cat89_cone_ops count_leg_ops = {cl_leg, NULL};

/* -------------------------------------------------------- RP: equality */

static void test_obj_equal_foreign(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    cat89_eq *eq;
    struct poison_eq p;
    const cat89_obj *local;
    const cat89_obj *foreign;
    int equal;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    eq = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    p.calls = 0;
    T_STATUS(cat89_eq_new(c1, &poison_eq_ops, &p, NULL, &eq), CAT89_OK);
    local = cat89_mock_obj(m1, CAT89_MOCK_A);
    foreign = cat89_mock_obj(m2, CAT89_MOCK_A);
    equal = 7;
    T_STATUS(cat89_obj_equal(eq, local, foreign, &equal), CAT89_INVALID);
    T_EQ_UL(equal, 0);
    T_EQ_UL(p.calls, 0);
    equal = 7;
    T_STATUS(cat89_obj_equal(eq, foreign, local, &equal), CAT89_INVALID);
    T_EQ_UL(equal, 0);
    T_EQ_UL(p.calls, 0);
    cat89_eq_release(eq);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

static void test_mor_equal_foreign(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    cat89_eq *eq;
    struct poison_eq p;
    cat89_mor *local;
    cat89_mor *foreign;
    int equal;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    eq = NULL;
    local = NULL;
    foreign = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    p.calls = 0;
    T_STATUS(cat89_eq_new(c1, &poison_eq_ops, &p, NULL, &eq), CAT89_OK);
    T_STATUS(cat89_mock_mor(m1, CAT89_MOCK_A, CAT89_MOCK_B, &local), CAT89_OK);
    T_STATUS(cat89_mock_mor(m2, CAT89_MOCK_A, CAT89_MOCK_B, &foreign),
             CAT89_OK);
    equal = 7;
    T_STATUS(cat89_mor_equal(eq, local, foreign, &equal), CAT89_INVALID);
    T_EQ_UL(equal, 0);
    T_EQ_UL(p.calls, 0);
    equal = 7;
    T_STATUS(cat89_mor_equal(eq, foreign, local, &equal), CAT89_INVALID);
    T_EQ_UL(equal, 0);
    T_EQ_UL(p.calls, 0);
    cat89_mor_release(c2, foreign);
    cat89_mor_release(c1, local);
    cat89_eq_release(eq);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

/* -------------------------------------------------- RP: hom enumeration */

static void test_hom_open_foreign(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    cat89_enum *en;
    struct poison_enum p;
    const cat89_obj *local;
    const cat89_obj *foreign;
    cat89_mor_iter *it;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    en = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    p.obj_open = 0;
    p.mor_open = 0;
    p.hom_open = 0;
    T_STATUS(cat89_enum_new(c1, &poison_enum_ops, &p, NULL, &en), CAT89_OK);
    local = cat89_mock_obj(m1, CAT89_MOCK_A);
    foreign = cat89_mock_obj(m2, CAT89_MOCK_A);
    it = (cat89_mor_iter *)(void *)&it;
    T_STATUS(cat89_hom_iter_open(en, foreign, local, &it), CAT89_INVALID);
    T_ASSERT(it == NULL);
    T_EQ_UL(p.hom_open, 0);
    it = (cat89_mor_iter *)(void *)&it;
    T_STATUS(cat89_hom_iter_open(en, local, foreign, &it), CAT89_INVALID);
    T_ASSERT(it == NULL);
    T_EQ_UL(p.hom_open, 0);
    cat89_enum_release(en);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

/* ------------------------------------------------- RP: functor/nat maps */

static void test_functor_map_foreign(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    cat89_functor *f;
    struct count_fun cf;
    const cat89_obj *foreign_obj;
    cat89_mor *foreign_mor;
    const cat89_obj *out_obj;
    cat89_mor *out_mor;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    f = NULL;
    foreign_mor = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    cf.map_obj_calls = 0;
    cf.map_mor_calls = 0;
    T_STATUS(cat89_functor_new(c1, c1, &count_fun_ops, &cf, NULL, &f),
             CAT89_OK);
    foreign_obj = cat89_mock_obj(m2, CAT89_MOCK_B);
    T_STATUS(cat89_mock_mor(m2, CAT89_MOCK_A, CAT89_MOCK_B, &foreign_mor),
             CAT89_OK);
    out_obj = (const cat89_obj *)(const void *)&out_obj;
    T_STATUS(cat89_functor_map_obj(f, foreign_obj, &out_obj), CAT89_INVALID);
    T_ASSERT(out_obj == NULL);
    T_EQ_UL(cf.map_obj_calls, 0);
    out_mor = (cat89_mor *)(void *)&out_mor;
    T_STATUS(cat89_functor_map_mor(f, foreign_mor, &out_mor), CAT89_INVALID);
    T_ASSERT(out_mor == NULL);
    T_EQ_UL(cf.map_mor_calls, 0);
    cat89_mor_release(c2, foreign_mor);
    cat89_functor_release(f);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

static void test_nat_component_foreign(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    cat89_functor *f;
    cat89_functor *g;
    cat89_nat *nat;
    struct count_nat cn;
    const cat89_obj *foreign;
    cat89_mor *out;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    f = NULL;
    g = NULL;
    nat = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    T_STATUS(cat89_functor_identity(c1, NULL, &f), CAT89_OK);
    T_STATUS(cat89_functor_identity(c1, NULL, &g), CAT89_OK);
    cn.comp_calls = 0;
    T_STATUS(cat89_nat_new(f, g, &count_nat_ops, &cn, NULL, &nat), CAT89_OK);
    foreign = cat89_mock_obj(m2, CAT89_MOCK_A);
    out = (cat89_mor *)(void *)&out;
    T_STATUS(cat89_nat_component(nat, foreign, &out), CAT89_INVALID);
    T_ASSERT(out == NULL);
    T_EQ_UL(cn.comp_calls, 0);
    cat89_nat_release(nat);
    cat89_functor_release(g);
    cat89_functor_release(f);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

/* ------------------------------------------------- RS: cone/cocone/iso */

static void test_cone_foreign_apex(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    cat89_functor *d;
    cat89_cone *cone;
    struct count_leg cl;
    const cat89_obj *foreign;
    cat89_status st;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    d = NULL;
    cone = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    T_STATUS(cat89_functor_identity(c1, NULL, &d), CAT89_OK);
    foreign = cat89_mock_obj(m2, CAT89_MOCK_A);
    cl.leg_calls = 0;
    cone = (cat89_cone *)(void *)&cone;
    st = cat89_cone_new(d, foreign, &count_leg_ops, &cl, NULL, &cone);
    T_STATUS(st, CAT89_INVALID);
    T_ASSERT(cone == NULL);
    T_EQ_UL(cl.leg_calls, 0);
    cat89_functor_release(d);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

static void test_cocone_foreign_apex(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    cat89_functor *d;
    cat89_cocone *cocone;
    struct count_leg cl;
    const cat89_obj *foreign;
    cat89_status st;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    d = NULL;
    cocone = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    T_STATUS(cat89_functor_identity(c1, NULL, &d), CAT89_OK);
    foreign = cat89_mock_obj(m2, CAT89_MOCK_A);
    cl.leg_calls = 0;
    cocone = (cat89_cocone *)(void *)&cocone;
    st = cat89_cocone_new(d, foreign, &count_leg_ops, &cl, NULL, &cocone);
    T_STATUS(st, CAT89_INVALID);
    T_ASSERT(cocone == NULL);
    T_EQ_UL(cl.leg_calls, 0);
    cat89_functor_release(d);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

static void test_cone_leg_foreign_shape(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    cat89_functor *d;
    cat89_cone *cone;
    struct count_leg cl;
    const cat89_obj *apex;
    const cat89_obj *foreign;
    cat89_mor *out;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    d = NULL;
    cone = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    T_STATUS(cat89_functor_identity(c1, NULL, &d), CAT89_OK);
    apex = cat89_mock_obj(m1, CAT89_MOCK_A);
    cl.leg_calls = 0;
    T_STATUS(cat89_cone_new(d, apex, &count_leg_ops, &cl, NULL, &cone),
             CAT89_OK);
    foreign = cat89_mock_obj(m2, CAT89_MOCK_A);
    out = (cat89_mor *)(void *)&out;
    T_STATUS(cat89_cone_leg(cone, foreign, &out), CAT89_INVALID);
    T_ASSERT(out == NULL);
    T_EQ_UL(cl.leg_calls, 0);
    cat89_cone_release(cone);
    cat89_functor_release(d);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

static void test_cocone_leg_foreign_shape(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    cat89_functor *d;
    cat89_cocone *cocone;
    struct count_leg cl;
    const cat89_obj *apex;
    const cat89_obj *foreign;
    cat89_mor *out;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    d = NULL;
    cocone = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    T_STATUS(cat89_functor_identity(c1, NULL, &d), CAT89_OK);
    apex = cat89_mock_obj(m1, CAT89_MOCK_A);
    cl.leg_calls = 0;
    T_STATUS(cat89_cocone_new(d, apex, &count_leg_ops, &cl, NULL, &cocone),
             CAT89_OK);
    foreign = cat89_mock_obj(m2, CAT89_MOCK_A);
    out = (cat89_mor *)(void *)&out;
    T_STATUS(cat89_cocone_leg(cocone, foreign, &out), CAT89_INVALID);
    T_ASSERT(out == NULL);
    T_EQ_UL(cl.leg_calls, 0);
    cat89_cocone_release(cocone);
    cat89_functor_release(d);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

static void test_iso_split_foreign(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    cat89_mor *local;
    cat89_mor *foreign;
    cat89_iso *iso;
    cat89_split_mono *mono;
    cat89_split_epi *epi;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    local = NULL;
    foreign = NULL;
    iso = NULL;
    mono = NULL;
    epi = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    T_STATUS(cat89_mock_mor(m1, CAT89_MOCK_A, CAT89_MOCK_B, &local), CAT89_OK);
    T_STATUS(cat89_mock_mor(m2, CAT89_MOCK_A, CAT89_MOCK_B, &foreign),
             CAT89_OK);
    cat89_mock_reset_calls(m1);
    T_STATUS(cat89_iso_new(c1, local, foreign, NULL, &iso), CAT89_INVALID);
    T_ASSERT(iso == NULL);
    T_STATUS(cat89_iso_new(c1, foreign, local, NULL, &iso), CAT89_INVALID);
    T_ASSERT(iso == NULL);
    T_STATUS(cat89_split_mono_new(c1, local, foreign, NULL, &mono),
             CAT89_INVALID);
    T_ASSERT(mono == NULL);
    T_STATUS(cat89_split_mono_new(c1, foreign, local, NULL, &mono),
             CAT89_INVALID);
    T_ASSERT(mono == NULL);
    T_STATUS(cat89_split_epi_new(c1, local, foreign, NULL, &epi),
             CAT89_INVALID);
    T_ASSERT(epi == NULL);
    T_STATUS(cat89_split_epi_new(c1, foreign, local, NULL, &epi),
             CAT89_INVALID);
    T_ASSERT(epi == NULL);
    T_EQ_UL(cat89_mock_retain_calls(m1), 0);
    cat89_mor_release(c2, foreign);
    cat89_mor_release(c1, local);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

/* ------------------------------------------------------- LC: law checks */

static void test_law_checks_foreign(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    cat89_eq *eq;
    struct poison_eq p;
    cat89_mor *local;
    cat89_mor *foreign;
    int valid;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    eq = NULL;
    local = NULL;
    foreign = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    p.calls = 0;
    T_STATUS(cat89_eq_new(c1, &poison_eq_ops, &p, NULL, &eq), CAT89_OK);
    T_STATUS(cat89_mock_mor(m1, CAT89_MOCK_A, CAT89_MOCK_B, &local), CAT89_OK);
    T_STATUS(cat89_mock_mor(m2, CAT89_MOCK_A, CAT89_MOCK_B, &foreign),
             CAT89_OK);

    valid = 7;
    T_STATUS(cat89_check_left_identity(c1, eq, foreign, &valid), CAT89_INVALID);
    T_EQ_UL(valid, 0);
    valid = 7;
    T_STATUS(cat89_check_right_identity(c1, eq, foreign, &valid),
             CAT89_INVALID);
    T_EQ_UL(valid, 0);
    valid = 7;
    T_STATUS(cat89_check_associativity(c1, eq, foreign, local, local, &valid),
             CAT89_INVALID);
    T_EQ_UL(valid, 0);
    valid = 7;
    T_STATUS(cat89_check_associativity(c1, eq, local, foreign, local, &valid),
             CAT89_INVALID);
    T_EQ_UL(valid, 0);
    valid = 7;
    T_STATUS(cat89_check_associativity(c1, eq, local, local, foreign, &valid),
             CAT89_INVALID);
    T_EQ_UL(valid, 0);
    T_EQ_UL(p.calls, 0);
    cat89_mor_release(c2, foreign);
    cat89_mor_release(c1, local);
    cat89_eq_release(eq);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

static void test_functor_checks_foreign(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    cat89_functor *f;
    cat89_eq *eq;
    struct count_fun cf;
    struct poison_eq p;
    cat89_mor *local;
    cat89_mor *foreign;
    const cat89_obj *foreign_obj;
    int valid;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    f = NULL;
    eq = NULL;
    local = NULL;
    foreign = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    cf.map_obj_calls = 0;
    cf.map_mor_calls = 0;
    T_STATUS(cat89_functor_new(c1, c1, &count_fun_ops, &cf, NULL, &f),
             CAT89_OK);
    p.calls = 0;
    T_STATUS(cat89_eq_new(c1, &poison_eq_ops, &p, NULL, &eq), CAT89_OK);
    T_STATUS(cat89_mock_mor(m1, CAT89_MOCK_A, CAT89_MOCK_B, &local), CAT89_OK);
    T_STATUS(cat89_mock_mor(m2, CAT89_MOCK_A, CAT89_MOCK_B, &foreign),
             CAT89_OK);
    foreign_obj = cat89_mock_obj(m2, CAT89_MOCK_A);

    valid = 7;
    T_STATUS(cat89_check_functor_identity(f, eq, foreign_obj, &valid),
             CAT89_INVALID);
    T_EQ_UL(valid, 0);
    valid = 7;
    T_STATUS(cat89_check_functor_composition(f, eq, foreign, local, &valid),
             CAT89_INVALID);
    T_EQ_UL(valid, 0);
    valid = 7;
    T_STATUS(cat89_check_functor_composition(f, eq, local, foreign, &valid),
             CAT89_INVALID);
    T_EQ_UL(valid, 0);
    T_EQ_UL(cf.map_obj_calls, 0);
    T_EQ_UL(cf.map_mor_calls, 0);
    cat89_mor_release(c2, foreign);
    cat89_mor_release(c1, local);
    cat89_eq_release(eq);
    cat89_functor_release(f);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

static void test_naturality_foreign(void)
{
    cat89_category *c1;
    cat89_category *c2;
    cat89_mock *m1;
    cat89_mock *m2;
    cat89_functor *f;
    cat89_functor *g;
    cat89_nat *nat;
    cat89_eq *eq;
    struct count_nat cn;
    struct poison_eq p;
    cat89_mor *foreign;
    int valid;

    c1 = NULL;
    c2 = NULL;
    m1 = NULL;
    m2 = NULL;
    f = NULL;
    g = NULL;
    nat = NULL;
    eq = NULL;
    foreign = NULL;
    T_STATUS(cat89_mock_new(NULL, &c1, &m1), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &c2, &m2), CAT89_OK);
    T_STATUS(cat89_functor_identity(c1, NULL, &f), CAT89_OK);
    T_STATUS(cat89_functor_identity(c1, NULL, &g), CAT89_OK);
    cn.comp_calls = 0;
    T_STATUS(cat89_nat_new(f, g, &count_nat_ops, &cn, NULL, &nat), CAT89_OK);
    p.calls = 0;
    T_STATUS(cat89_eq_new(c1, &poison_eq_ops, &p, NULL, &eq), CAT89_OK);
    T_STATUS(cat89_mock_mor(m2, CAT89_MOCK_A, CAT89_MOCK_B, &foreign),
             CAT89_OK);
    valid = 7;
    T_STATUS(cat89_check_naturality(nat, eq, foreign, &valid), CAT89_INVALID);
    T_EQ_UL(valid, 0);
    T_EQ_UL(cn.comp_calls, 0);
    cat89_mor_release(c2, foreign);
    cat89_eq_release(eq);
    cat89_nat_release(nat);
    cat89_functor_release(g);
    cat89_functor_release(f);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_mock_free(m2);
    cat89_mock_free(m1);
}

/* --------------------------------------------------- FR: finder inputs */

static void test_finder_foreign_objects(void)
{
    cat89_category *cat;
    cat89_eq *eq;
    cat89_enum *en;
    cat89_category *mc;
    cat89_mock *mock;
    const cat89_obj *foreign;
    cat89_limit *lim;
    cat89_colimit *colim;
    cat89_mor *out;

    cat = NULL;
    eq = NULL;
    en = NULL;
    mc = NULL;
    mock = NULL;
    lim = NULL;
    colim = NULL;
    out = NULL;
    T_STATUS(cat89_fixture_build(CAT89_FIX_BINPROD, &cat, &eq, &en), CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &mc, &mock), CAT89_OK);
    foreign = cat89_mock_obj(mock, CAT89_MOCK_A);

    lim = (cat89_limit *)(void *)&lim;
    T_STATUS(cat89_find_binary_product(cat, en, eq, foreign, foreign, NULL,
                                       &lim, NULL, NULL),
             CAT89_INVALID);
    T_ASSERT(lim == NULL);
    colim = (cat89_colimit *)(void *)&colim;
    T_STATUS(cat89_find_binary_coproduct(cat, en, eq, foreign, foreign, NULL,
                                         &colim, NULL, NULL),
             CAT89_INVALID);
    T_ASSERT(colim == NULL);

    /* terminal/initial arrows with a foreign object */
    out = (cat89_mor *)(void *)&out;
    T_STATUS(cat89_terminal_arrow(cat, en, foreign, foreign, &out),
             CAT89_INVALID);
    T_ASSERT(out == NULL);
    out = (cat89_mor *)(void *)&out;
    T_STATUS(cat89_initial_arrow(cat, en, foreign, foreign, &out),
             CAT89_INVALID);
    T_ASSERT(out == NULL);

    cat89_category_release(mc);
    cat89_mock_free(mock);
    cat89_enum_release(en);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

static void test_finder_foreign_morphisms(void)
{
    cat89_category *cat;
    cat89_eq *eq;
    cat89_enum *en;
    cat89_category *mc;
    cat89_mock *mock;
    cat89_mor *foreign;
    cat89_limit *lim;
    cat89_colimit *colim;

    cat = NULL;
    eq = NULL;
    en = NULL;
    mc = NULL;
    mock = NULL;
    foreign = NULL;
    lim = NULL;
    colim = NULL;
    T_STATUS(cat89_fixture_build(CAT89_FIX_EQUALIZER, &cat, &eq, &en),
             CAT89_OK);
    T_STATUS(cat89_mock_new(NULL, &mc, &mock), CAT89_OK);
    T_STATUS(cat89_mock_mor(mock, CAT89_MOCK_A, CAT89_MOCK_B, &foreign),
             CAT89_OK);

    lim = (cat89_limit *)(void *)&lim;
    T_STATUS(cat89_find_equalizer(cat, en, eq, foreign, foreign, NULL, &lim,
                                  NULL, NULL),
             CAT89_INVALID);
    T_ASSERT(lim == NULL);
    colim = (cat89_colimit *)(void *)&colim;
    T_STATUS(cat89_find_coequalizer(cat, en, eq, foreign, foreign, NULL, &colim,
                                    NULL, NULL),
             CAT89_INVALID);
    T_ASSERT(colim == NULL);
    lim = (cat89_limit *)(void *)&lim;
    T_STATUS(cat89_find_pullback(cat, en, eq, foreign, foreign, NULL, &lim,
                                 NULL, NULL, NULL),
             CAT89_INVALID);
    T_ASSERT(lim == NULL);
    colim = (cat89_colimit *)(void *)&colim;
    T_STATUS(cat89_find_pushout(cat, en, eq, foreign, foreign, NULL, &colim,
                                NULL, NULL, NULL),
             CAT89_INVALID);
    T_ASSERT(colim == NULL);

    cat89_mor_release(mc, foreign);
    cat89_category_release(mc);
    cat89_mock_free(mock);
    cat89_enum_release(en);
    cat89_eq_release(eq);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    test_obj_equal_foreign();
    test_mor_equal_foreign();
    test_hom_open_foreign();
    test_functor_map_foreign();
    test_nat_component_foreign();
    test_cone_foreign_apex();
    test_cocone_foreign_apex();
    test_cone_leg_foreign_shape();
    test_cocone_leg_foreign_shape();
    test_iso_split_foreign();
    test_law_checks_foreign();
    test_functor_checks_foreign();
    test_naturality_foreign();
    test_finder_foreign_objects();
    test_finder_foreign_morphisms();
    return T_END() ? 0 : 1;
}
