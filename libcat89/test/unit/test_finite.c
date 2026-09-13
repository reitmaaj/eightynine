/* cat89_test_finite.c - finite backend: builder, store, category, eq, enum. */
#include <cat89/cat89.h>
#include <cat89/finite.h>
#include <fault_alloc.h>
#include <test.h>

int cat89_test_failures = 0;

/* Monoid on one object: morphisms 0=e (identity), 1=a, a o a = a. */
static cat89_status build_monoid(cat89_category **out_cat, cat89_eq **out_eq,
                                 cat89_enum **out_enum)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id obj = 0;
    cat89_finite_mor_id e = 0;
    cat89_finite_mor_id a = 0;
    cat89_status st;

    st = cat89_finite_builder_new(NULL, &b);
    if (st != CAT89_OK)
    {
        return st;
    }
    cat89_finite_add_object(b, &obj);
    cat89_finite_add_morphism(b, obj, obj, &e);
    cat89_finite_add_morphism(b, obj, obj, &a);
    cat89_finite_set_identity(b, obj, e);
    cat89_finite_set_composition(b, e, e, e);
    cat89_finite_set_composition(b, e, a, a);
    cat89_finite_set_composition(b, a, e, a);
    cat89_finite_set_composition(b, a, a, a);
    st = cat89_finite_build(b, out_cat, out_eq, out_enum);
    cat89_finite_builder_release(b);
    return st;
}

static const cat89_obj *first_object(cat89_enum *enumeration,
                                     cat89_category *cat)
{
    cat89_obj_iter *it = NULL;
    const cat89_obj *obj = NULL;
    int done = 0;
    cat89_status st;

    st = cat89_obj_iter_open(enumeration, &it);
    if (st != CAT89_OK)
    {
        return NULL;
    }
    st = cat89_obj_iter_next(enumeration, it, &obj, &done);
    (void)cat;
    cat89_obj_iter_close(enumeration, it);
    if (st != CAT89_OK || done)
    {
        return NULL;
    }
    return obj;
}

static void test_monoid_compose_and_eq(void)
{
    cat89_category *cat = NULL;
    cat89_eq *eq = NULL;
    cat89_enum *enumeration = NULL;
    const cat89_obj *aobj = NULL;
    cat89_mor *e_handle = NULL;
    cat89_mor *a_handle = NULL;
    cat89_mor *mor = NULL;
    cat89_mor *first = NULL;
    cat89_mor *second = NULL;
    cat89_mor *composed = NULL;
    cat89_mor_iter *it = NULL;
    int done = 0;
    int equal = -1;
    int got_a = 0;

    T_STATUS(build_monoid(&cat, &eq, &enumeration), CAT89_OK);
    T_ASSERT(cat != NULL);
    T_ASSERT(eq != NULL);
    T_ASSERT(enumeration != NULL);

    aobj = first_object(enumeration, cat);
    T_ASSERT(aobj != NULL);

    /* identity on the single object is the unit e. */
    T_STATUS(cat89_identity(cat, aobj, &e_handle), CAT89_OK);

    /* enumerate both morphisms; distinguish e from a by equality to e. */
    T_STATUS(cat89_mor_iter_open(enumeration, &it), CAT89_OK);
    done = 0;
    got_a = 0;
    while (!done)
    {
        T_STATUS(cat89_mor_iter_next(enumeration, it, &mor, &done), CAT89_OK);
        if (done)
        {
            break;
        }
        if (first == NULL)
        {
            first = mor;
        }
        else
        {
            second = mor;
        }
    }
    cat89_mor_iter_close(enumeration, it);
    T_ASSERT(first != NULL);
    T_ASSERT(second != NULL);

    T_STATUS(cat89_mor_equal(eq, e_handle, first, &equal), CAT89_OK);
    if (equal)
    {
        a_handle = second;
    }
    else
    {
        a_handle = first;
    }
    T_ASSERT(a_handle != NULL);

    /* e o e = e : compose the identity with itself stays the identity. */
    T_STATUS(cat89_compose(cat, e_handle, e_handle, &composed), CAT89_OK);
    T_STATUS(cat89_mor_equal(eq, e_handle, composed, &equal), CAT89_OK);
    T_EQ_UL(equal, 1);
    cat89_mor_release(cat, composed);

    /* a o a = a : equal to the non-identity handle. */
    T_STATUS(cat89_compose(cat, a_handle, a_handle, &composed), CAT89_OK);
    T_STATUS(cat89_mor_equal(eq, a_handle, composed, &equal), CAT89_OK);
    T_EQ_UL(equal, 1);
    cat89_mor_release(cat, composed);

    /* e and a are not equal. */
    T_STATUS(cat89_mor_equal(eq, e_handle, a_handle, &equal), CAT89_OK);
    T_EQ_UL(equal, 0);

    /* dom/cod of both morphisms is the single object. */
    {
        const cat89_obj *d = NULL;
        const cat89_obj *c = NULL;
        T_STATUS(cat89_dom(cat, a_handle, &d), CAT89_OK);
        T_STATUS(cat89_cod(cat, a_handle, &c), CAT89_OK);
        T_ASSERT(cat89_obj_same(cat, d, aobj));
        T_ASSERT(cat89_obj_same(cat, c, aobj));
    }

    (void)got_a;
    cat89_mor_release(cat, e_handle);
    cat89_mor_release(cat, first);
    cat89_mor_release(cat, second);
    cat89_eq_release(eq);
    cat89_enum_release(enumeration);
    cat89_category_release(cat);
}

/* Discrete two-object category: object enumeration, cardinality, domain. */
static cat89_status build_d2(cat89_category **out_cat, cat89_eq **out_eq,
                             cat89_enum **out_enum)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id a = 0;
    cat89_finite_obj_id c = 0;
    cat89_finite_mor_id ia = 0;
    cat89_finite_mor_id ic = 0;
    cat89_status st;

    st = cat89_finite_builder_new(NULL, &b);
    if (st != CAT89_OK)
    {
        return st;
    }
    cat89_finite_add_object(b, &a);
    cat89_finite_add_object(b, &c);
    cat89_finite_add_morphism(b, a, a, &ia);
    cat89_finite_add_morphism(b, c, c, &ic);
    cat89_finite_set_identity(b, a, ia);
    cat89_finite_set_identity(b, c, ic);
    cat89_finite_set_composition(b, ia, ia, ia);
    cat89_finite_set_composition(b, ic, ic, ic);
    st = cat89_finite_build(b, out_cat, out_eq, out_enum);
    cat89_finite_builder_release(b);
    return st;
}

static void test_d2_enumeration_and_domain(void)
{
    cat89_category *cat = NULL;
    cat89_eq *eq = NULL;
    cat89_enum *enumeration = NULL;
    cat89_obj_iter *oit = NULL;
    const cat89_obj *o1 = NULL;
    const cat89_obj *o2 = NULL;
    const cat89_obj *tmp = NULL;
    int done = 0;
    int obj_count = 0;
    cat89_mor *i1 = NULL;
    cat89_mor *i2 = NULL;
    cat89_mor *composed = NULL;

    T_STATUS(build_d2(&cat, &eq, &enumeration), CAT89_OK);

    T_STATUS(cat89_obj_iter_open(enumeration, &oit), CAT89_OK);
    done = 0;
    obj_count = 0;
    while (!done)
    {
        tmp = NULL;
        T_STATUS(cat89_obj_iter_next(enumeration, oit, &tmp, &done), CAT89_OK);
        if (done)
        {
            break;
        }
        if (obj_count == 0)
        {
            o1 = tmp;
        }
        else
        {
            o2 = tmp;
        }
        obj_count = obj_count + 1;
    }
    cat89_obj_iter_close(enumeration, oit);
    T_EQ_UL(obj_count, 2);
    T_ASSERT(o1 != NULL);
    T_ASSERT(o2 != NULL);
    T_ASSERT(!cat89_obj_same(cat, o1, o2));

    T_STATUS(cat89_identity(cat, o1, &i1), CAT89_OK);
    T_STATUS(cat89_identity(cat, o2, &i2), CAT89_OK);

    /* i1 o i1 = i1. */
    T_STATUS(cat89_compose(cat, i1, i1, &composed), CAT89_OK);
    cat89_mor_release(cat, composed);

    /* i2 o i1 is ill-typed (different objects) -> CAT89_DOMAIN. */
    T_STATUS(cat89_compose(cat, i2, i1, &composed), CAT89_DOMAIN);
    T_ASSERT(composed == NULL);

    cat89_mor_release(cat, i1);
    cat89_mor_release(cat, i2);
    cat89_eq_release(eq);
    cat89_enum_release(enumeration);
    cat89_category_release(cat);
}

static void test_validate_rejects_missing_identity(void)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id a = 0;
    cat89_finite_mor_id m = 0;
    cat89_status st;
    int valid = -1;

    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    cat89_finite_add_object(b, &a);
    cat89_finite_add_object(b, &a);
    cat89_finite_add_morphism(b, a, a, &m);

    /* two objects but identities only set implicitly? not set -> invalid. */
    st = cat89_finite_validate(b, &valid);
    T_STATUS(st, CAT89_OK);
    T_EQ_UL(valid, 0);

    cat89_finite_builder_release(b);
}

static void test_builder_rejects_bad_ids(void)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id a = 0;
    cat89_finite_mor_id m = 0;
    cat89_finite_obj_id oid = 999;

    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    cat89_finite_add_object(b, &a);
    T_STATUS(cat89_finite_add_morphism(b, 5, a, &m), CAT89_INVALID);
    T_STATUS(cat89_finite_set_identity(b, oid, m), CAT89_INVALID);
    T_STATUS(cat89_finite_set_composition(b, 9, 0, 0), CAT89_INVALID);
    cat89_finite_builder_release(b);
}

/* --------------------------------------------------- FV/FB: validation */

static void fv00(cat89_finite_builder *b, cat89_finite_obj_id *a,
                 cat89_finite_obj_id *bb, cat89_finite_mor_id *ia,
                 cat89_finite_mor_id *ib, cat89_finite_mor_id *f)
{
    cat89_finite_add_object(b, a);
    cat89_finite_add_object(b, bb);
    cat89_finite_add_morphism(b, *a, *a, ia);
    cat89_finite_add_morphism(b, *bb, *bb, ib);
    cat89_finite_add_morphism(b, *a, *bb, f);
    cat89_finite_set_identity(b, *a, *ia);
    cat89_finite_set_identity(b, *bb, *ib);
    cat89_finite_set_composition(b, *ia, *ia, *ia);
    cat89_finite_set_composition(b, *ib, *ib, *ib);
    cat89_finite_set_composition(b, *ib, *f, *f);
    cat89_finite_set_composition(b, *f, *ia, *f);
}

static void assert_valid(cat89_finite_builder *b, unsigned long expected)
{
    int valid = -1;
    T_STATUS(cat89_finite_validate(b, &valid), CAT89_OK);
    T_EQ_UL(valid, expected);
}

static void test_fv01_trivial_valid(void)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id o = 0;
    cat89_finite_mor_id id = 0;

    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    cat89_finite_add_object(b, &o);
    cat89_finite_add_morphism(b, o, o, &id);
    cat89_finite_set_identity(b, o, id);
    cat89_finite_set_composition(b, id, id, id);
    assert_valid(b, 1);
    cat89_finite_builder_release(b);
}

static void test_fv02_fv00_valid(void)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id a;
    cat89_finite_obj_id bb;
    cat89_finite_mor_id ia;
    cat89_finite_mor_id ib;
    cat89_finite_mor_id f;

    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    fv00(b, &a, &bb, &ia, &ib, &f);
    assert_valid(b, 1);
    cat89_finite_builder_release(b);
}

static void test_fv03_missing_identity(void)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id a;
    cat89_finite_obj_id bb;
    cat89_finite_mor_id ia;
    cat89_finite_mor_id ib;
    cat89_finite_mor_id f;

    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    fv00(b, &a, &bb, &ia, &ib, &f);
    cat89_finite_add_object(b, &a);
    assert_valid(b, 0);
    cat89_finite_builder_release(b);
}

static void test_fv04_mistyped_identity(void)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id a;
    cat89_finite_obj_id bb;
    cat89_finite_mor_id ia;
    cat89_finite_mor_id ib;
    cat89_finite_mor_id f;

    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    fv00(b, &a, &bb, &ia, &ib, &f);
    cat89_finite_set_identity(b, a, f);
    assert_valid(b, 0);
    cat89_finite_builder_release(b);
}

static void test_fv05_missing_composition(void)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id a;
    cat89_finite_obj_id bb;
    cat89_finite_mor_id ia;
    cat89_finite_mor_id ib;
    cat89_finite_mor_id f;

    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    fv00(b, &a, &bb, &ia, &ib, &f);
    /* omit iB o f = f by rebuilding the table without it */
    cat89_finite_builder_release(b);
    b = NULL;
    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    cat89_finite_add_object(b, &a);
    cat89_finite_add_object(b, &bb);
    cat89_finite_add_morphism(b, a, a, &ia);
    cat89_finite_add_morphism(b, bb, bb, &ib);
    cat89_finite_add_morphism(b, a, bb, &f);
    cat89_finite_set_identity(b, a, ia);
    cat89_finite_set_identity(b, bb, ib);
    cat89_finite_set_composition(b, ia, ia, ia);
    cat89_finite_set_composition(b, ib, ib, ib);
    cat89_finite_set_composition(b, f, ia, f);
    assert_valid(b, 0);
    cat89_finite_builder_release(b);
}

static void test_fv06_non_composable_row(void)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id a;
    cat89_finite_obj_id bb;
    cat89_finite_mor_id ia;
    cat89_finite_mor_id ib;
    cat89_finite_mor_id f;

    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    fv00(b, &a, &bb, &ia, &ib, &f);
    /* iA o iB is non-composable (cod iB = B, dom iA = A) */
    cat89_finite_set_composition(b, ia, ib, ia);
    assert_valid(b, 0);
    cat89_finite_builder_release(b);
}

static void test_fv07_wrong_result_type(void)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id a;
    cat89_finite_obj_id bb;
    cat89_finite_mor_id ia;
    cat89_finite_mor_id ib;
    cat89_finite_mor_id f;

    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    fv00(b, &a, &bb, &ia, &ib, &f);
    cat89_finite_set_composition(b, f, ia, ia);
    assert_valid(b, 0);
    cat89_finite_builder_release(b);
}

static void test_fv08_fv09_duplicate_keys(void)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id a;
    cat89_finite_obj_id bb;
    cat89_finite_mor_id ia;
    cat89_finite_mor_id ib;
    cat89_finite_mor_id f;

    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    fv00(b, &a, &bb, &ia, &ib, &f);
    cat89_finite_set_composition(b, f, ia, f);
    assert_valid(b, 0);
    cat89_finite_builder_release(b);

    b = NULL;
    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    fv00(b, &a, &bb, &ia, &ib, &f);
    cat89_finite_set_composition(b, f, ia, ia);
    assert_valid(b, 0);
    cat89_finite_builder_release(b);
}

static void build_identity_monoid(cat89_finite_builder *b,
                                  cat89_finite_obj_id *o,
                                  cat89_finite_mor_id *e,
                                  cat89_finite_mor_id *a,
                                  cat89_finite_mor_id *bb)
{
    cat89_finite_add_object(b, o);
    cat89_finite_add_morphism(b, *o, *o, e);
    cat89_finite_add_morphism(b, *o, *o, a);
    cat89_finite_add_morphism(b, *o, *o, bb);
    cat89_finite_set_identity(b, *o, *e);
    cat89_finite_set_composition(b, *e, *e, *e);
    cat89_finite_set_composition(b, *e, *a, *a);
    cat89_finite_set_composition(b, *e, *bb, *bb);
    cat89_finite_set_composition(b, *a, *e, *a);
    cat89_finite_set_composition(b, *bb, *e, *bb);
    cat89_finite_set_composition(b, *a, *a, *bb);
    cat89_finite_set_composition(b, *a, *bb, *a);
    cat89_finite_set_composition(b, *bb, *a, *bb);
    cat89_finite_set_composition(b, *bb, *bb, *a);
}

static void test_fv10_left_identity_broken(void)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id o;
    cat89_finite_mor_id e;
    cat89_finite_mor_id a;
    cat89_finite_mor_id bb;

    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    build_identity_monoid(b, &o, &e, &a, &bb);
    cat89_finite_set_composition(b, e, a, e);
    assert_valid(b, 0);
    cat89_finite_builder_release(b);
}

static void test_fv11_right_identity_broken(void)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id o;
    cat89_finite_mor_id e;
    cat89_finite_mor_id a;
    cat89_finite_mor_id bb;

    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    build_identity_monoid(b, &o, &e, &a, &bb);
    cat89_finite_set_composition(b, a, e, e);
    assert_valid(b, 0);
    cat89_finite_builder_release(b);
}

static void test_fv12_associativity_broken(void)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id o;
    cat89_finite_mor_id e;
    cat89_finite_mor_id a;
    cat89_finite_mor_id bb;

    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    build_identity_monoid(b, &o, &e, &a, &bb);
    assert_valid(b, 0);
    cat89_finite_builder_release(b);
}

static void test_fv13_pairs_exactly_once(void)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id a;
    cat89_finite_obj_id bb;
    cat89_finite_mor_id ia;
    cat89_finite_mor_id ib;
    cat89_finite_mor_id f;

    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    fv00(b, &a, &bb, &ia, &ib, &f);
    assert_valid(b, 1);
    /* a duplicate key flips the table to invalid */
    cat89_finite_set_composition(b, ib, f, f);
    assert_valid(b, 0);
    cat89_finite_builder_release(b);
}

static void test_fv14_row_references_rejected(void)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id a = 0;
    cat89_finite_mor_id m = 0;

    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    cat89_finite_add_object(b, &a);
    cat89_finite_add_morphism(b, a, a, &m);
    T_STATUS(cat89_finite_set_composition(b, m, m, 99), CAT89_INVALID);
    T_STATUS(cat89_finite_set_composition(b, m, 99, m), CAT89_INVALID);
    T_STATUS(cat89_finite_set_composition(b, 99, m, m), CAT89_INVALID);
    T_STATUS(cat89_finite_set_identity(b, 99, m), CAT89_INVALID);
    T_STATUS(cat89_finite_set_identity(b, a, 99), CAT89_INVALID);
    cat89_finite_builder_release(b);
}

/* ------------------------------------------------------ FB: build reuse */

static void test_fb01_build_does_not_consume(void)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id a;
    cat89_finite_obj_id bb;
    cat89_finite_mor_id ia;
    cat89_finite_mor_id ib;
    cat89_finite_mor_id f;
    cat89_category *cat = NULL;
    cat89_eq *eq = NULL;
    cat89_enum *en = NULL;

    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    fv00(b, &a, &bb, &ia, &ib, &f);
    T_STATUS(cat89_finite_build(b, &cat, &eq, &en), CAT89_OK);
    assert_valid(b, 1);
    cat89_enum_release(en);
    cat89_eq_release(eq);
    cat89_category_release(cat);
    cat89_finite_builder_release(b);
}

static void test_fb02_build_twice(void)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id a;
    cat89_finite_obj_id bb;
    cat89_finite_mor_id ia;
    cat89_finite_mor_id ib;
    cat89_finite_mor_id f;
    cat89_category *c1 = NULL;
    cat89_category *c2 = NULL;
    cat89_enum *en1 = NULL;
    cat89_enum *en2 = NULL;
    cat89_mor *m1 = NULL;
    cat89_mor *m2 = NULL;
    cat89_obj_iter *it = NULL;
    const cat89_obj *obj1 = NULL;
    const cat89_obj *obj2 = NULL;
    int done = 0;

    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    fv00(b, &a, &bb, &ia, &ib, &f);
    T_STATUS(cat89_finite_build(b, &c1, NULL, &en1), CAT89_OK);
    T_STATUS(cat89_finite_build(b, &c2, NULL, &en2), CAT89_OK);
    T_ASSERT(c1 != c2);
    T_ASSERT(c1 != NULL && c2 != NULL);
    T_STATUS(cat89_obj_iter_open(en1, &it), CAT89_OK);
    T_STATUS(cat89_obj_iter_next(en1, it, &obj1, &done), CAT89_OK);
    cat89_obj_iter_close(en1, it);
    T_ASSERT(obj1 != NULL);
    it = NULL;
    done = 0;
    T_STATUS(cat89_obj_iter_open(en2, &it), CAT89_OK);
    T_STATUS(cat89_obj_iter_next(en2, it, &obj2, &done), CAT89_OK);
    cat89_obj_iter_close(en2, it);
    T_ASSERT(obj2 != NULL);
    T_ASSERT(cat89_owns_obj(c2, obj1) == 0);
    T_STATUS(cat89_identity(c1, obj1, &m1), CAT89_OK);
    T_ASSERT(m1 != NULL);
    T_ASSERT(cat89_owns_mor(c1, m1) == 1);
    T_ASSERT(cat89_owns_mor(c2, m1) == 0);
    T_STATUS(cat89_identity(c2, obj2, &m2), CAT89_OK);
    T_ASSERT(m2 != NULL);
    T_ASSERT(cat89_owns_mor(c2, m2) == 1);
    T_ASSERT(cat89_owns_mor(c1, m2) == 0);
    cat89_mor_release(c2, m2);
    cat89_mor_release(c1, m1);
    cat89_enum_release(en2);
    cat89_enum_release(en1);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_finite_builder_release(b);
}

static void test_fb03_mutate_after_build(void)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id a;
    cat89_finite_obj_id bb;
    cat89_finite_mor_id ia;
    cat89_finite_mor_id ib;
    cat89_finite_mor_id f;
    cat89_finite_obj_id d = 0;
    cat89_finite_mor_id id = 0;
    cat89_category *c1 = NULL;
    cat89_category *c2 = NULL;
    cat89_eq *e1 = NULL;
    cat89_eq *e2 = NULL;
    cat89_enum *n1 = NULL;
    cat89_enum *n2 = NULL;
    cat89_obj_iter *it;
    const cat89_obj *o;
    int done;
    unsigned long count;

    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    fv00(b, &a, &bb, &ia, &ib, &f);
    T_STATUS(cat89_finite_build(b, &c1, &e1, &n1), CAT89_OK);
    cat89_finite_add_object(b, &d);
    cat89_finite_add_morphism(b, d, d, &id);
    cat89_finite_set_identity(b, d, id);
    cat89_finite_set_composition(b, id, id, id);
    assert_valid(b, 1);
    T_STATUS(cat89_finite_build(b, &c2, &e2, &n2), CAT89_OK);

    count = 0;
    it = NULL;
    T_STATUS(cat89_obj_iter_open(n1, &it), CAT89_OK);
    done = 0;
    while (!done)
    {
        o = NULL;
        T_STATUS(cat89_obj_iter_next(n1, it, &o, &done), CAT89_OK);
        if (!done)
        {
            count = count + 1;
        }
    }
    cat89_obj_iter_close(n1, it);
    T_EQ_UL(count, 2);

    count = 0;
    it = NULL;
    T_STATUS(cat89_obj_iter_open(n2, &it), CAT89_OK);
    done = 0;
    while (!done)
    {
        o = NULL;
        T_STATUS(cat89_obj_iter_next(n2, it, &o, &done), CAT89_OK);
        if (!done)
        {
            count = count + 1;
        }
    }
    cat89_obj_iter_close(n2, it);
    T_EQ_UL(count, 3);

    cat89_enum_release(n2);
    cat89_enum_release(n1);
    cat89_eq_release(e2);
    cat89_eq_release(e1);
    cat89_category_release(c2);
    cat89_category_release(c1);
    cat89_finite_builder_release(b);
}

static void test_fb04_invalid_build_keeps_builder(void)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id a;
    cat89_finite_obj_id bb;
    cat89_finite_mor_id ia;
    cat89_finite_mor_id ib;
    cat89_finite_mor_id f;
    cat89_category *cat = NULL;
    cat89_eq *eq = NULL;
    cat89_enum *en = NULL;

    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    fv00(b, &a, &bb, &ia, &ib, &f);
    /* remove the completeness by starting a fresh partial table */
    cat89_finite_builder_release(b);
    b = NULL;
    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    cat89_finite_add_object(b, &a);
    cat89_finite_add_object(b, &bb);
    cat89_finite_add_morphism(b, a, a, &ia);
    cat89_finite_add_morphism(b, bb, bb, &ib);
    cat89_finite_add_morphism(b, a, bb, &f);
    cat89_finite_set_identity(b, a, ia);
    cat89_finite_set_identity(b, bb, ib);
    cat89_finite_set_composition(b, ia, ia, ia);
    cat89_finite_set_composition(b, ib, ib, ib);
    cat89_finite_set_composition(b, f, ia, f);
    cat = (cat89_category *)(void *)&cat;
    T_STATUS(cat89_finite_build(b, &cat, &eq, &en), CAT89_INVALID);
    T_ASSERT(cat == NULL);
    assert_valid(b, 0);
    cat89_finite_set_composition(b, ib, f, f);
    assert_valid(b, 1);
    cat89_finite_build(b, &cat, &eq, &en);
    T_ASSERT(cat != NULL);
    cat89_enum_release(en);
    cat89_eq_release(eq);
    cat89_category_release(cat);
    cat89_finite_builder_release(b);
}

static void test_fb05_build_allocfail(void)
{
    cat89_finite_builder *b = NULL;
    cat89_finite_obj_id a;
    cat89_finite_obj_id bb;
    cat89_finite_mor_id ia;
    cat89_finite_mor_id ib;
    cat89_finite_mor_id f;
    struct cat89_fault_alloc fa;
    cat89_allocator alloc;
    cat89_category *cat = NULL;
    cat89_eq *eq = NULL;
    cat89_enum *en = NULL;
    cat89_status st;
    unsigned long at;
    unsigned long ok_count;
    unsigned long fail_count;

    cat89_fault_alloc_init(&fa);
    cat89_fault_alloc_use(&fa, &alloc);
    T_STATUS(cat89_finite_builder_new(&alloc, &b), CAT89_OK);
    fv00(b, &a, &bb, &ia, &ib, &f);
    ok_count = 0;
    fail_count = 0;
    for (at = 1; at <= 100; at = at + 1)
    {
        cat89_fault_alloc_fail_at(&fa, at);
        cat = NULL;
        eq = NULL;
        en = NULL;
        st = cat89_finite_build(b, &cat, &eq, &en);
        if (st == CAT89_OK)
        {
            T_ASSERT(cat != NULL);
            cat89_enum_release(en);
            cat89_eq_release(eq);
            cat89_category_release(cat);
            ok_count = ok_count + 1;
        }
        else
        {
            T_ASSERT(st == CAT89_NOMEM);
            T_ASSERT(cat == NULL);
            assert_valid(b, 1);
            fail_count = fail_count + 1;
        }
    }
    T_ASSERT(fail_count > 0);
    T_ASSERT(ok_count > 0);
    cat89_fault_alloc_disable(&fa);
    T_STATUS(cat89_finite_build(b, &cat, NULL, NULL), CAT89_OK);
    cat89_category_release(cat);
    cat89_finite_builder_release(b);
}

int main(void)
{
    T_START();
    test_monoid_compose_and_eq();
    test_d2_enumeration_and_domain();
    test_validate_rejects_missing_identity();
    test_builder_rejects_bad_ids();
    test_fv01_trivial_valid();
    test_fv02_fv00_valid();
    test_fv03_missing_identity();
    test_fv04_mistyped_identity();
    test_fv05_missing_composition();
    test_fv06_non_composable_row();
    test_fv07_wrong_result_type();
    test_fv08_fv09_duplicate_keys();
    test_fv10_left_identity_broken();
    test_fv11_right_identity_broken();
    test_fv12_associativity_broken();
    test_fv13_pairs_exactly_once();
    test_fv14_row_references_rejected();
    test_fb01_build_does_not_consume();
    test_fb02_build_twice();
    test_fb03_mutate_after_build();
    test_fb04_invalid_build_keeps_builder();
    test_fb05_build_allocfail();
    return T_END() ? 0 : 1;
}
