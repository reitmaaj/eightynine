/* cat89_test_functor.c - identity functor, composition, instance checks. */
#include "finite_categories.h"
#include <cat89/cat89.h>
#include <test.h>

int cat89_test_failures = 0;

static const cat89_obj *grab_object(cat89_enum *enumeration)
{
    cat89_obj_iter *it = NULL;
    const cat89_obj *obj = NULL;
    int done = 0;

    if (cat89_obj_iter_open(enumeration, &it) != CAT89_OK)
    {
        return NULL;
    }
    if (cat89_obj_iter_next(enumeration, it, &obj, &done) != CAT89_OK)
    {
        cat89_obj_iter_close(enumeration, it);
        return NULL;
    }
    cat89_obj_iter_close(enumeration, it);
    return obj;
}

static cat89_mor *grab_morphism(cat89_enum *enumeration)
{
    cat89_mor_iter *it = NULL;
    cat89_mor *mor = NULL;
    int done = 0;

    if (cat89_mor_iter_open(enumeration, &it) != CAT89_OK)
    {
        return NULL;
    }
    if (cat89_mor_iter_next(enumeration, it, &mor, &done) != CAT89_OK)
    {
        cat89_mor_iter_close(enumeration, it);
        return NULL;
    }
    cat89_mor_iter_close(enumeration, it);
    return mor;
}

static void test_identity_functor_preserves(void)
{
    cat89_category *cat = NULL;
    cat89_eq *eq = NULL;
    cat89_enum *enumeration = NULL;
    cat89_functor *ident = NULL;
    const cat89_obj *obj = NULL;
    const cat89_obj *mapped_obj = NULL;
    cat89_mor *mor = NULL;
    cat89_mor *mapped = NULL;
    int equal = -1;

    T_STATUS(cat89_fixture_build(CAT89_FIX_ARROW, &cat, &eq, &enumeration),
             CAT89_OK);

    T_STATUS(cat89_functor_identity(cat, NULL, &ident), CAT89_OK);
    T_ASSERT(ident != NULL);
    T_ASSERT(cat89_functor_source(ident) == cat);
    T_ASSERT(cat89_functor_target(ident) == cat);

    obj = grab_object(enumeration);
    T_ASSERT(obj != NULL);
    T_STATUS(cat89_functor_map_obj(ident, obj, &mapped_obj), CAT89_OK);
    T_ASSERT(mapped_obj != NULL);
    T_ASSERT(cat89_obj_same(cat, obj, mapped_obj));

    mor = grab_morphism(enumeration);
    T_ASSERT(mor != NULL);
    T_STATUS(cat89_functor_map_mor(ident, mor, &mapped), CAT89_OK);
    T_ASSERT(mapped != NULL);
    equal = -1;
    T_STATUS(cat89_mor_equal(eq, mor, mapped, &equal), CAT89_OK);
    T_EQ_UL(equal, 1);
    cat89_mor_release(cat, mor);
    cat89_mor_release(cat, mapped);

    cat89_functor_release(ident);
    cat89_eq_release(eq);
    cat89_enum_release(enumeration);
    cat89_category_release(cat);
}

static void test_identity_self_composition_and_mismatch(void)
{
    cat89_category *cat = NULL;
    cat89_category *cat2 = NULL;
    cat89_eq *eq = NULL;
    cat89_enum *enumeration = NULL;
    cat89_functor *ident = NULL;
    cat89_functor *ident2 = NULL;
    cat89_functor *comp = NULL;
    cat89_mor *mor = NULL;
    cat89_mor *mapped = NULL;
    cat89_mor_iter *it = NULL;
    int done = 0;
    int equal = -1;

    T_STATUS(cat89_fixture_build(CAT89_FIX_ARROW, &cat, &eq, &enumeration),
             CAT89_OK);
    T_STATUS(cat89_fixture_build(CAT89_FIX_COMP2, &cat2, NULL, NULL), CAT89_OK);

    T_STATUS(cat89_functor_identity(cat, NULL, &ident), CAT89_OK);
    T_STATUS(cat89_functor_identity(cat2, NULL, &ident2), CAT89_OK);

    /* g o f with matching middle category is valid. */
    T_STATUS(cat89_functor_compose(ident, ident, NULL, &comp), CAT89_OK);
    T_ASSERT(comp != NULL);
    T_ASSERT(cat89_functor_source(comp) == cat);

    /* Mapping through the composite preserves a morphism. */
    it = NULL;
    done = 0;
    T_STATUS(cat89_mor_iter_open(enumeration, &it), CAT89_OK);
    T_STATUS(cat89_mor_iter_next(enumeration, it, &mor, &done), CAT89_OK);
    cat89_mor_iter_close(enumeration, it);
    T_ASSERT(mor != NULL);
    T_STATUS(cat89_functor_map_mor(comp, mor, &mapped), CAT89_OK);
    equal = -1;
    T_STATUS(cat89_mor_equal(eq, mor, mapped, &equal), CAT89_OK);
    T_EQ_UL(equal, 1);
    cat89_mor_release(cat, mor);
    cat89_mor_release(cat, mapped);

    /* Mismatched middle category (different category instances). */
    {
        cat89_functor *dummy = NULL;
        T_STATUS(cat89_functor_compose(ident2, ident, NULL, &dummy),
                 CAT89_INVALID);
        T_ASSERT(dummy == NULL);
        T_STATUS(cat89_functor_compose(NULL, ident, NULL, &dummy),
                 CAT89_INVALID);
        T_STATUS(cat89_functor_compose(ident, NULL, NULL, &dummy),
                 CAT89_INVALID);
    }

    cat89_functor_release(ident);
    cat89_functor_release(ident2);
    cat89_functor_release(comp);
    cat89_eq_release(eq);
    cat89_enum_release(enumeration);
    cat89_category_release(cat);
    cat89_category_release(cat2);
}

/* ------------------------------------------------- FC01-FC03: three cats */

struct map_ctx
{
    const cat89_obj *obj;
    cat89_mor *id;
    cat89_category *target;
};

static cat89_status map_const_obj(void *ctx, const cat89_obj *obj,
                                  const cat89_obj **out_obj)
{
    struct map_ctx *m = ctx;
    (void)obj;
    *out_obj = m->obj;
    return CAT89_OK;
}

static cat89_status map_const_mor(void *ctx, cat89_mor *mor,
                                  cat89_mor **out_mor)
{
    struct map_ctx *m = ctx;
    cat89_status st;

    (void)mor;
    st = cat89_mor_retain(m->target, m->id);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_mor = m->id;
    return CAT89_OK;
}

static const cat89_functor_ops map_ops = {map_const_obj, map_const_mor, NULL};

static void test_fc01_fc02_compose_three_categories(void)
{
    cat89_category *c = NULL;
    cat89_category *d = NULL;
    cat89_category *e = NULL;
    cat89_eq *eqc = NULL;
    cat89_eq *eqd = NULL;
    cat89_eq *eqe = NULL;
    cat89_enum *enc = NULL;
    cat89_enum *end = NULL;
    cat89_enum *ene = NULL;
    cat89_functor *f = NULL;
    cat89_functor *g = NULL;
    cat89_functor *gf = NULL;
    struct map_ctx mf;
    struct map_ctx mg;
    const cat89_obj *cobj = NULL;
    const cat89_obj *dobj = NULL;
    const cat89_obj *eobj = NULL;
    cat89_mor *did = NULL;
    cat89_mor *eid = NULL;
    cat89_mor *cmor = NULL;
    const cat89_obj *mapped = NULL;
    cat89_mor *mapped_mor = NULL;
    int equal = -1;

    T_STATUS(cat89_fixture_build(CAT89_FIX_TERMINAL, &c, &eqc, &enc), CAT89_OK);
    T_STATUS(cat89_fixture_build(CAT89_FIX_TERMINAL, &d, &eqd, &end), CAT89_OK);
    T_STATUS(cat89_fixture_build(CAT89_FIX_TERMINAL, &e, &eqe, &ene), CAT89_OK);
    T_ASSERT(c != d && d != e && c != e);
    cobj = grab_object(enc);
    dobj = grab_object(end);
    eobj = grab_object(ene);
    T_ASSERT(cobj != NULL && dobj != NULL && eobj != NULL);
    T_STATUS(cat89_identity(d, dobj, &did), CAT89_OK);
    T_STATUS(cat89_identity(e, eobj, &eid), CAT89_OK);
    mf.obj = dobj;
    mf.id = did;
    mf.target = d;
    mg.obj = eobj;
    mg.id = eid;
    mg.target = e;
    T_STATUS(cat89_functor_new(c, d, &map_ops, &mf, NULL, &f), CAT89_OK);
    T_STATUS(cat89_functor_new(d, e, &map_ops, &mg, NULL, &g), CAT89_OK);

    T_STATUS(cat89_functor_compose(g, f, NULL, &gf), CAT89_OK);
    T_ASSERT(gf != NULL);
    T_ASSERT(cat89_functor_source(gf) == c);
    T_ASSERT(cat89_functor_target(gf) == e);

    /* FC02: GF(x) == G(F(x)) and GF(m) == G(F(m)) in E. */
    mapped = NULL;
    T_STATUS(cat89_functor_map_obj(gf, cobj, &mapped), CAT89_OK);
    T_ASSERT(cat89_obj_same(e, mapped, eobj));
    cmor = grab_morphism(enc);
    T_ASSERT(cmor != NULL);
    mapped_mor = NULL;
    T_STATUS(cat89_functor_map_mor(gf, cmor, &mapped_mor), CAT89_OK);
    T_ASSERT(mapped_mor != NULL);
    T_STATUS(cat89_mor_equal(eqe, mapped_mor, eid, &equal), CAT89_OK);
    T_EQ_UL(equal, 1);

    cat89_mor_release(e, mapped_mor);
    cat89_mor_release(c, cmor);
    cat89_functor_release(gf);
    cat89_functor_release(g);
    cat89_functor_release(f);
    cat89_mor_release(e, eid);
    cat89_mor_release(d, did);
    cat89_enum_release(ene);
    cat89_enum_release(end);
    cat89_enum_release(enc);
    cat89_eq_release(eqe);
    cat89_eq_release(eqd);
    cat89_eq_release(eqc);
    cat89_category_release(e);
    cat89_category_release(d);
    cat89_category_release(c);
}

static void test_fc03_incompatible_middle(void)
{
    cat89_category *c = NULL;
    cat89_category *d1 = NULL;
    cat89_category *d2 = NULL;
    cat89_category *e = NULL;
    cat89_eq *eq1 = NULL;
    cat89_eq *eq2 = NULL;
    cat89_eq *eq3 = NULL;
    cat89_eq *eq4 = NULL;
    cat89_enum *en1 = NULL;
    cat89_enum *en2 = NULL;
    cat89_enum *en3 = NULL;
    cat89_enum *en4 = NULL;
    cat89_functor *f = NULL;
    cat89_functor *g = NULL;
    cat89_functor *gf = NULL;
    struct map_ctx mf;
    struct map_ctx mg;
    const cat89_obj *o2 = NULL;
    const cat89_obj *o3 = NULL;
    const cat89_obj *o4 = NULL;
    cat89_mor *id1 = NULL;
    cat89_mor *id2 = NULL;
    cat89_mor *id3 = NULL;

    T_STATUS(cat89_fixture_build(CAT89_FIX_TERMINAL, &c, &eq1, &en1), CAT89_OK);
    T_STATUS(cat89_fixture_build(CAT89_FIX_TERMINAL, &d1, &eq2, &en2),
             CAT89_OK);
    T_STATUS(cat89_fixture_build(CAT89_FIX_TERMINAL, &d2, &eq3, &en3),
             CAT89_OK);
    T_STATUS(cat89_fixture_build(CAT89_FIX_TERMINAL, &e, &eq4, &en4), CAT89_OK);
    T_ASSERT(d1 != d2);
    o2 = grab_object(en2);
    o3 = grab_object(en3);
    o4 = grab_object(en4);
    T_STATUS(cat89_identity(d1, o2, &id1), CAT89_OK);
    T_STATUS(cat89_identity(d2, o3, &id2), CAT89_OK);
    T_STATUS(cat89_identity(e, o4, &id3), CAT89_OK);
    mf.obj = o2;
    mf.id = id1;
    mf.target = d1;
    mg.obj = o4;
    mg.id = id3;
    mg.target = e;
    T_STATUS(cat89_functor_new(c, d1, &map_ops, &mf, NULL, &f), CAT89_OK);
    T_STATUS(cat89_functor_new(d2, e, &map_ops, &mg, NULL, &g), CAT89_OK);
    gf = (cat89_functor *)(void *)&gf;
    T_STATUS(cat89_functor_compose(g, f, NULL, &gf), CAT89_INVALID);
    T_ASSERT(gf == NULL);
    cat89_functor_release(g);
    cat89_functor_release(f);
    cat89_mor_release(e, id3);
    cat89_mor_release(d2, id2);
    cat89_mor_release(d1, id1);
    cat89_enum_release(en4);
    cat89_enum_release(en3);
    cat89_enum_release(en2);
    cat89_enum_release(en1);
    cat89_eq_release(eq4);
    cat89_eq_release(eq3);
    cat89_eq_release(eq2);
    cat89_eq_release(eq1);
    cat89_category_release(e);
    cat89_category_release(d2);
    cat89_category_release(d1);
    cat89_category_release(c);
}

int main(void)
{
    T_START();
    test_identity_functor_preserves();
    test_identity_self_composition_and_mismatch();
    test_fc01_fc02_compose_three_categories();
    test_fc03_incompatible_middle();
    return T_END() ? 0 : 1;
}
