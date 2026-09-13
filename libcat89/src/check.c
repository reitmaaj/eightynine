/* cat89_check.c - explicit law checking (local witness checks). */

#include "cat89_internal.h"
#include <cat89/check.h>

static cat89_status compose_and_compare(cat89_category *category, cat89_eq *eq,
                                        const cat89_mor *a, const cat89_mor *b,
                                        const cat89_mor *witness,
                                        int *out_equal)
{
    cat89_status st;
    cat89_mor *composite;
    int equal;

    composite = NULL;
    st = cat89_compose(category, a, b, &composite);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_mor_equal(eq, composite, witness, &equal);
    cat89_mor_release(category, composite);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_equal = equal;
    return CAT89_OK;
}

static int base_args_valid(cat89_category *category, cat89_eq *eq,
                           const cat89_mor *f)
{
    if (category == NULL)
    {
        return 0;
    }
    if (eq == NULL)
    {
        return 0;
    }
    if (f == NULL)
    {
        return 0;
    }
    return 1;
}

cat89_status cat89_check_left_identity(cat89_category *category, cat89_eq *eq,
                                       const cat89_mor *f, int *out_valid)
{
    cat89_status st;
    const cat89_obj *cf;
    cat89_mor *cid;
    int equal;

    if (out_valid == NULL)
    {
        return CAT89_INVALID;
    }
    *out_valid = 0;
    if (base_args_valid(category, eq, f) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_eq_category(eq) != category)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(category, f) == 0)
    {
        return CAT89_INVALID;
    }

    cid = NULL;
    st = cat89_cod(category, f, &cf);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_identity(category, cf, &cid);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = compose_and_compare(category, eq, cid, f, f, &equal);
    cat89_mor_release(category, cid);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_valid = equal;
    return CAT89_OK;
}

cat89_status cat89_check_right_identity(cat89_category *category, cat89_eq *eq,
                                        const cat89_mor *f, int *out_valid)
{
    cat89_status st;
    const cat89_obj *df;
    cat89_mor *idf;
    int equal;

    if (out_valid == NULL)
    {
        return CAT89_INVALID;
    }
    *out_valid = 0;
    if (base_args_valid(category, eq, f) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_eq_category(eq) != category)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(category, f) == 0)
    {
        return CAT89_INVALID;
    }

    idf = NULL;
    st = cat89_dom(category, f, &df);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_identity(category, df, &idf);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = compose_and_compare(category, eq, f, idf, f, &equal);
    cat89_mor_release(category, idf);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_valid = equal;
    return CAT89_OK;
}

static cat89_status compose_owned(cat89_category *category, const cat89_mor *a,
                                  const cat89_mor *b, cat89_mor **out_mor)
{
    cat89_status st;

    *out_mor = NULL;
    st = cat89_compose(category, a, b, out_mor);
    return st;
}

static void rel(cat89_category *category, cat89_mor *mor)
{
    cat89_mor_release(category, mor);
}

static void rel2(cat89_category *category, cat89_mor *a, cat89_mor *b)
{
    cat89_mor_release(category, a);
    cat89_mor_release(category, b);
}

static void rel3(cat89_category *category, cat89_mor *a, cat89_mor *b,
                 cat89_mor *d)
{
    rel2(category, a, b);
    cat89_mor_release(category, d);
}

static void rel4(cat89_category *category, cat89_mor *a, cat89_mor *b,
                 cat89_mor *d, cat89_mor *e)
{
    rel3(category, a, b, d);
    cat89_mor_release(category, e);
}

cat89_status cat89_check_associativity(cat89_category *category, cat89_eq *eq,
                                       const cat89_mor *h, const cat89_mor *g,
                                       const cat89_mor *f, int *out_valid)
{
    cat89_status st;
    cat89_mor *hg;
    cat89_mor *lhs;
    cat89_mor *gf;
    cat89_mor *rhs;
    int equal;

    if (out_valid == NULL)
    {
        return CAT89_INVALID;
    }
    *out_valid = 0;
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (eq == NULL)
    {
        return CAT89_INVALID;
    }
    if (h == NULL)
    {
        return CAT89_INVALID;
    }
    if (g == NULL)
    {
        return CAT89_INVALID;
    }
    if (f == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_eq_category(eq) != category)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(category, h) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(category, g) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(category, f) == 0)
    {
        return CAT89_INVALID;
    }

    hg = NULL;
    lhs = NULL;
    gf = NULL;
    rhs = NULL;

    st = compose_owned(category, h, g, &hg);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = compose_owned(category, hg, f, &lhs);
    rel(category, hg);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = compose_owned(category, g, f, &gf);
    if (st != CAT89_OK)
    {
        rel(category, lhs);
        return st;
    }
    st = compose_owned(category, h, gf, &rhs);
    rel(category, gf);
    if (st != CAT89_OK)
    {
        rel(category, lhs);
        return st;
    }

    st = cat89_mor_equal(eq, lhs, rhs, &equal);
    rel(category, lhs);
    rel(category, rhs);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_valid = equal;
    return CAT89_OK;
}

static cat89_status law_identity(cat89_category *category, cat89_eq *eq,
                                 const cat89_mor *a, const cat89_mor *b,
                                 const cat89_obj *obj, int *out_equal)
{
    cat89_mor *idw;
    cat89_status st;
    int equal;

    idw = NULL;
    st = cat89_identity(category, obj, &idw);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = compose_and_compare(category, eq, a, b, idw, &equal);
    cat89_mor_release(category, idw);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_equal = equal;
    return CAT89_OK;
}

cat89_status cat89_check_iso(const cat89_iso *iso, cat89_eq *eq, int *out_valid)
{
    cat89_category *category;
    cat89_mor *forward;
    cat89_mor *inverse;
    const cat89_obj *df;
    const cat89_obj *cf;
    cat89_status st;
    int e1;
    int e2;

    if (out_valid == NULL)
    {
        return CAT89_INVALID;
    }
    *out_valid = 0;
    if (iso == NULL)
    {
        return CAT89_INVALID;
    }
    if (eq == NULL)
    {
        return CAT89_INVALID;
    }
    category = cat89_iso_category(iso);
    forward = cat89_iso_forward(iso);
    inverse = cat89_iso_inverse(iso);
    if (cat89_eq_category(eq) != category)
    {
        return CAT89_INVALID;
    }

    st = cat89_dom(category, forward, &df);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_cod(category, forward, &cf);
    if (st != CAT89_OK)
    {
        return st;
    }

    st = law_identity(category, eq, inverse, forward, df, &e1);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (e1 == 0)
    {
        return CAT89_OK;
    }
    st = law_identity(category, eq, forward, inverse, cf, &e2);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_valid = e2;
    return CAT89_OK;
}

cat89_status cat89_check_split_mono(const cat89_split_mono *split, cat89_eq *eq,
                                    int *out_valid)
{
    cat89_category *category;
    const cat89_mor *section;
    const cat89_mor *retraction;
    const cat89_obj *ds;
    cat89_status st;
    int equal;

    if (out_valid == NULL)
    {
        return CAT89_INVALID;
    }
    *out_valid = 0;
    if (split == NULL)
    {
        return CAT89_INVALID;
    }
    if (eq == NULL)
    {
        return CAT89_INVALID;
    }
    category = cat89_split_mono_category(split);
    section = cat89_split_mono_section(split);
    retraction = cat89_split_mono_retraction(split);
    if (cat89_eq_category(eq) != category)
    {
        return CAT89_INVALID;
    }

    st = cat89_dom(category, section, &ds);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = law_identity(category, eq, retraction, section, ds, &equal);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_valid = equal;
    return CAT89_OK;
}

cat89_status cat89_check_split_epi(const cat89_split_epi *split, cat89_eq *eq,
                                   int *out_valid)
{
    cat89_category *category;
    const cat89_mor *section;
    const cat89_mor *retraction;
    const cat89_obj *cs;
    cat89_status st;
    int equal;

    if (out_valid == NULL)
    {
        return CAT89_INVALID;
    }
    *out_valid = 0;
    if (split == NULL)
    {
        return CAT89_INVALID;
    }
    if (eq == NULL)
    {
        return CAT89_INVALID;
    }
    category = cat89_split_epi_category(split);
    section = cat89_split_epi_section(split);
    retraction = cat89_split_epi_retraction(split);
    if (cat89_eq_category(eq) != category)
    {
        return CAT89_INVALID;
    }

    st = cat89_cod(category, section, &cs);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = law_identity(category, eq, section, retraction, cs, &equal);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_valid = equal;
    return CAT89_OK;
}

cat89_status cat89_check_functor_identity(const cat89_functor *functor,
                                          cat89_eq *target_eq,
                                          const cat89_obj *obj, int *out_valid)
{
    cat89_category *source;
    cat89_category *target;
    const cat89_obj *fobj;
    cat89_mor *idsrc;
    cat89_mor *fidsrc;
    cat89_mor *idf;
    cat89_status st;
    int equal;

    if (out_valid == NULL)
    {
        return CAT89_INVALID;
    }
    *out_valid = 0;
    if (functor == NULL)
    {
        return CAT89_INVALID;
    }
    if (target_eq == NULL)
    {
        return CAT89_INVALID;
    }
    if (obj == NULL)
    {
        return CAT89_INVALID;
    }
    source = cat89_functor_source(functor);
    target = cat89_functor_target(functor);
    if (cat89_eq_category(target_eq) != target)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_obj(source, obj) == 0)
    {
        return CAT89_INVALID;
    }

    idsrc = NULL;
    fidsrc = NULL;
    idf = NULL;

    st = cat89_functor_map_obj(functor, obj, &fobj);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_identity(target, fobj, &idf);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_identity(source, obj, &idsrc);
    if (st != CAT89_OK)
    {
        rel(target, idf);
        return st;
    }
    st = cat89_functor_map_mor(functor, idsrc, &fidsrc);
    rel(source, idsrc);
    if (st != CAT89_OK)
    {
        rel(target, idf);
        return st;
    }
    st = cat89_mor_equal(target_eq, fidsrc, idf, &equal);
    rel(target, fidsrc);
    rel(target, idf);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_valid = equal;
    return CAT89_OK;
}

cat89_status cat89_check_functor_composition(const cat89_functor *functor,
                                             cat89_eq *target_eq, cat89_mor *g,
                                             cat89_mor *f, int *out_valid)
{
    cat89_category *source;
    cat89_category *target;
    cat89_mor *gfsrc;
    cat89_mor *flhs;
    cat89_mor *fg;
    cat89_mor *ff;
    cat89_mor *frhs;
    cat89_status st;
    int equal;

    if (out_valid == NULL)
    {
        return CAT89_INVALID;
    }
    *out_valid = 0;
    if (functor == NULL)
    {
        return CAT89_INVALID;
    }
    if (target_eq == NULL)
    {
        return CAT89_INVALID;
    }
    if (g == NULL)
    {
        return CAT89_INVALID;
    }
    if (f == NULL)
    {
        return CAT89_INVALID;
    }
    source = cat89_functor_source(functor);
    target = cat89_functor_target(functor);
    if (cat89_eq_category(target_eq) != target)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(source, g) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(source, f) == 0)
    {
        return CAT89_INVALID;
    }

    gfsrc = NULL;
    flhs = NULL;
    fg = NULL;
    ff = NULL;
    frhs = NULL;

    st = compose_owned(source, g, f, &gfsrc);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_functor_map_mor(functor, gfsrc, &flhs);
    rel(source, gfsrc);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_functor_map_mor(functor, g, &fg);
    if (st != CAT89_OK)
    {
        rel(target, flhs);
        return st;
    }
    st = cat89_functor_map_mor(functor, f, &ff);
    if (st != CAT89_OK)
    {
        rel2(target, flhs, fg);
        return st;
    }
    st = compose_owned(target, fg, ff, &frhs);
    rel2(target, fg, ff);
    if (st != CAT89_OK)
    {
        rel(target, flhs);
        return st;
    }
    st = cat89_mor_equal(target_eq, flhs, frhs, &equal);
    rel2(target, flhs, frhs);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_valid = equal;
    return CAT89_OK;
}

cat89_status cat89_check_naturality(const cat89_nat *nat, cat89_eq *target_eq,
                                    cat89_mor *f, int *out_valid)
{
    cat89_functor *sf;
    cat89_functor *tf;
    cat89_category *source;
    cat89_category *target;
    const cat89_obj *a;
    const cat89_obj *b;
    cat89_mor *eta_a;
    cat89_mor *eta_b;
    cat89_mor *ff;
    cat89_mor *gg;
    cat89_mor *lhs;
    cat89_mor *rhs;
    cat89_status st;
    int equal;

    if (out_valid == NULL)
    {
        return CAT89_INVALID;
    }
    *out_valid = 0;
    if (nat == NULL)
    {
        return CAT89_INVALID;
    }
    if (target_eq == NULL)
    {
        return CAT89_INVALID;
    }
    if (f == NULL)
    {
        return CAT89_INVALID;
    }
    sf = cat89_nat_source(nat);
    tf = cat89_nat_target(nat);
    source = cat89_functor_source(sf);
    target = cat89_functor_target(sf);
    if (cat89_eq_category(target_eq) != target)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(source, f) == 0)
    {
        return CAT89_INVALID;
    }

    eta_a = NULL;
    eta_b = NULL;
    ff = NULL;
    gg = NULL;
    lhs = NULL;
    rhs = NULL;

    st = cat89_dom(source, f, &a);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_cod(source, f, &b);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_nat_component(nat, a, &eta_a);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_nat_component(nat, b, &eta_b);
    if (st != CAT89_OK)
    {
        rel(target, eta_a);
        return st;
    }
    st = cat89_functor_map_mor(sf, f, &ff);
    if (st != CAT89_OK)
    {
        rel2(target, eta_a, eta_b);
        return st;
    }
    st = cat89_functor_map_mor(tf, f, &gg);
    if (st != CAT89_OK)
    {
        rel3(target, eta_a, eta_b, ff);
        return st;
    }
    st = compose_owned(target, gg, eta_a, &lhs);
    if (st != CAT89_OK)
    {
        rel4(target, eta_a, eta_b, ff, gg);
        return st;
    }
    st = compose_owned(target, eta_b, ff, &rhs);
    rel(target, eta_a);
    rel(target, eta_b);
    rel(target, ff);
    rel(target, gg);
    if (st != CAT89_OK)
    {
        rel(target, lhs);
        return st;
    }
    st = cat89_mor_equal(target_eq, lhs, rhs, &equal);
    rel(target, lhs);
    rel(target, rhs);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_valid = equal;
    return CAT89_OK;
}

/* ------------------------------------------ cone / cocone checks */

static cat89_status cone_compare(cat89_category *ccat, cat89_eq *eq,
                                 cat89_mor *a, cat89_mor *b, int *out_invalid)
{
    cat89_status st;
    int equal;

    st = cat89_mor_equal(eq, a, b, &equal);
    rel(ccat, a);
    rel(ccat, b);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (equal == 0)
    {
        *out_invalid = 1;
    }
    return CAT89_OK;
}

static cat89_status cone_mor_check(cat89_category *ccat, cat89_category *shape,
                                   cat89_eq *eq, cat89_diagram *diagram,
                                   const cat89_cone *cone, cat89_mor *m,
                                   int *out_invalid)
{
    const cat89_obj *j;
    const cat89_obj *k;
    cat89_mor *dm;
    cat89_mor *lj;
    cat89_mor *lk;
    cat89_mor *comp;
    cat89_status st;

    dm = NULL;
    lj = NULL;
    lk = NULL;
    comp = NULL;
    st = cat89_dom(shape, m, &j);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_cod(shape, m, &k);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_functor_map_mor(diagram, m, &dm);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_cone_leg(cone, j, &lj);
    if (st != CAT89_OK)
    {
        rel(ccat, dm);
        return st;
    }
    st = cat89_cone_leg(cone, k, &lk);
    if (st != CAT89_OK)
    {
        rel2(ccat, dm, lj);
        return st;
    }
    st = compose_owned(ccat, dm, lj, &comp);
    rel2(ccat, dm, lj);
    if (st != CAT89_OK)
    {
        rel(ccat, lk);
        return st;
    }
    st = cone_compare(ccat, eq, comp, lk, out_invalid);
    return st;
}

static cat89_status cone_step(cat89_enum *shape_enum, cat89_mor_iter *it,
                              cat89_category *shape, cat89_category *ccat,
                              cat89_eq *eq, cat89_diagram *diagram,
                              const cat89_cone *cone, int *out_done,
                              int *out_invalid)
{
    cat89_mor *m;
    cat89_status st;

    m = NULL;
    st = cat89_mor_iter_next(shape_enum, it, &m, out_done);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (*out_done)
    {
        return CAT89_OK;
    }
    st = cone_mor_check(ccat, shape, eq, diagram, cone, m, out_invalid);
    cat89_mor_release(shape, m);
    return st;
}

cat89_status cat89_check_cone(const cat89_cone *cone, cat89_eq *eq,
                              cat89_enum *shape_enum, int *out_valid)
{
    cat89_diagram *diagram;
    cat89_category *shape;
    cat89_category *ccat;
    cat89_mor_iter *it;
    cat89_status st;
    int done;
    int invalid;

    if (out_valid == NULL)
    {
        return CAT89_INVALID;
    }
    *out_valid = 0;
    if (cone == NULL)
    {
        return CAT89_INVALID;
    }
    if (eq == NULL)
    {
        return CAT89_INVALID;
    }
    if (shape_enum == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    diagram = cat89_cone_diagram(cone);
    shape = cat89_functor_source(diagram);
    ccat = cat89_cone_category(cone);
    if (cat89_eq_category(eq) != ccat)
    {
        return CAT89_INVALID;
    }
    if (cat89_enum_category(shape_enum) != shape)
    {
        return CAT89_INVALID;
    }

    it = NULL;
    st = cat89_mor_iter_open(shape_enum, &it);
    if (st != CAT89_OK)
    {
        return st;
    }
    invalid = 0;
    done = 0;
    while (!done)
    {
        st = cone_step(shape_enum, it, shape, ccat, eq, diagram, cone, &done,
                       &invalid);
        if (st != CAT89_OK)
        {
            cat89_mor_iter_close(shape_enum, it);
            return st;
        }
    }
    cat89_mor_iter_close(shape_enum, it);
    if (invalid)
    {
        return CAT89_OK;
    }
    *out_valid = 1;
    return CAT89_OK;
}

static cat89_status cocone_mor_check(cat89_category *ccat,
                                     cat89_category *shape, cat89_eq *eq,
                                     cat89_diagram *diagram,
                                     const cat89_cocone *cocone, cat89_mor *m,
                                     int *out_invalid)
{
    const cat89_obj *j;
    const cat89_obj *k;
    cat89_mor *dm;
    cat89_mor *lj;
    cat89_mor *lk;
    cat89_mor *comp;
    cat89_status st;

    dm = NULL;
    lj = NULL;
    lk = NULL;
    comp = NULL;
    st = cat89_dom(shape, m, &j);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_cod(shape, m, &k);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_functor_map_mor(diagram, m, &dm);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_cocone_leg(cocone, j, &lj);
    if (st != CAT89_OK)
    {
        rel(ccat, dm);
        return st;
    }
    st = cat89_cocone_leg(cocone, k, &lk);
    if (st != CAT89_OK)
    {
        rel2(ccat, dm, lj);
        return st;
    }
    st = compose_owned(ccat, lk, dm, &comp);
    rel2(ccat, lk, dm);
    if (st != CAT89_OK)
    {
        rel(ccat, lj);
        return st;
    }
    st = cone_compare(ccat, eq, lj, comp, out_invalid);
    return st;
}

static cat89_status cocone_step(cat89_enum *shape_enum, cat89_mor_iter *it,
                                cat89_category *shape, cat89_category *ccat,
                                cat89_eq *eq, cat89_diagram *diagram,
                                const cat89_cocone *cocone, int *out_done,
                                int *out_invalid)
{
    cat89_mor *m;
    cat89_status st;

    m = NULL;
    st = cat89_mor_iter_next(shape_enum, it, &m, out_done);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (*out_done)
    {
        return CAT89_OK;
    }
    st = cocone_mor_check(ccat, shape, eq, diagram, cocone, m, out_invalid);
    cat89_mor_release(shape, m);
    return st;
}

cat89_status cat89_check_cocone(const cat89_cocone *cocone, cat89_eq *eq,
                                cat89_enum *shape_enum, int *out_valid)
{
    cat89_diagram *diagram;
    cat89_category *shape;
    cat89_category *ccat;
    cat89_mor_iter *it;
    cat89_status st;
    int done;
    int invalid;

    if (out_valid == NULL)
    {
        return CAT89_INVALID;
    }
    *out_valid = 0;
    if (cocone == NULL)
    {
        return CAT89_INVALID;
    }
    if (eq == NULL)
    {
        return CAT89_INVALID;
    }
    if (shape_enum == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    diagram = cat89_cocone_diagram(cocone);
    shape = cat89_functor_source(diagram);
    ccat = cat89_cocone_category(cocone);
    if (cat89_eq_category(eq) != ccat)
    {
        return CAT89_INVALID;
    }
    if (cat89_enum_category(shape_enum) != shape)
    {
        return CAT89_INVALID;
    }

    it = NULL;
    st = cat89_mor_iter_open(shape_enum, &it);
    if (st != CAT89_OK)
    {
        return st;
    }
    invalid = 0;
    done = 0;
    while (!done)
    {
        st = cocone_step(shape_enum, it, shape, ccat, eq, diagram, cocone,
                         &done, &invalid);
        if (st != CAT89_OK)
        {
            cat89_mor_iter_close(shape_enum, it);
            return st;
        }
    }
    cat89_mor_iter_close(shape_enum, it);
    if (invalid)
    {
        return CAT89_OK;
    }
    *out_valid = 1;
    return CAT89_OK;
}

/* -------------------------------------------- exhaustive law checking */

struct exc
{
    cat89_category *category;
    cat89_mor **mors;
    const cat89_obj **dom;
    const cat89_obj **cod;
    unsigned long n;
};

static void exc_free(struct exc *cs)
{
    const cat89_allocator *alloc;
    unsigned long i;

    alloc = cat89_allocator_default();
    for (i = 0; i < cs->n; i = i + 1)
    {
        cat89_mor_release(cs->category, cs->mors[i]);
    }
    if (cs->mors != NULL)
    {
        cat89_free(alloc, cs->mors);
    }
    if (cs->dom != NULL)
    {
        cat89_free(alloc, (void *)cs->dom);
    }
    if (cs->cod != NULL)
    {
        cat89_free(alloc, (void *)cs->cod);
    }
    cs->mors = NULL;
    cs->dom = NULL;
    cs->cod = NULL;
    cs->n = 0;
}

static void exc_vecs_free(const cat89_allocator *alloc, cat89_vec *vm,
                          cat89_vec *vd, cat89_vec *vc)
{
    cat89_vec_free(vm, alloc);
    cat89_vec_free(vd, alloc);
    cat89_vec_free(vc, alloc);
}

static void exc_mor_row_free(cat89_category *cat, cat89_vec *vm, size_t i)
{
    cat89_mor *mor;

    mor = *(cat89_mor **)cat89_vec_at(vm, i);
    cat89_mor_release(cat, mor);
}

static void exc_mors_vec_free(cat89_category *cat, const cat89_allocator *alloc,
                              cat89_vec *vm, cat89_vec *vd, cat89_vec *vc)
{
    size_t i;

    for (i = 0; i < vm->len; i = i + 1)
    {
        exc_mor_row_free(cat, vm, i);
    }
    exc_vecs_free(alloc, vm, vd, vc);
}

static cat89_status exc_fill_step(cat89_category *cat, cat89_enum *en,
                                  cat89_mor_iter *it, cat89_vec *vm,
                                  cat89_vec *vd, cat89_vec *vc,
                                  const cat89_allocator *alloc, int *out_done)
{
    cat89_mor *mor;
    const cat89_obj *dm;
    const cat89_obj *cm;
    cat89_status st;

    mor = NULL;
    st = cat89_mor_iter_next(en, it, &mor, out_done);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (*out_done)
    {
        return CAT89_OK;
    }
    dm = NULL;
    cm = NULL;
    st = cat89_dom(cat, mor, &dm);
    if (st == CAT89_OK)
    {
        st = cat89_cod(cat, mor, &cm);
    }
    if (st == CAT89_OK)
    {
        st = cat89_vec_push(vm, alloc, &mor);
    }
    if (st == CAT89_OK)
    {
        st = cat89_vec_push(vd, alloc, &dm);
    }
    if (st == CAT89_OK)
    {
        st = cat89_vec_push(vc, alloc, &cm);
    }
    if (st != CAT89_OK)
    {
        cat89_mor_release(cat, mor);
        return st;
    }
    return CAT89_OK;
}

static void exc_fill_abort(cat89_category *cat, cat89_enum *en,
                           cat89_mor_iter *it, const cat89_allocator *alloc,
                           cat89_vec *vm, cat89_vec *vd, cat89_vec *vc)
{
    cat89_mor_iter_close(en, it);
    exc_mors_vec_free(cat, alloc, vm, vd, vc);
}

static cat89_status exc_open_fill(cat89_category *cat, cat89_enum *en,
                                  struct exc *cs)
{
    const cat89_allocator *alloc;
    cat89_mor_iter *it;
    cat89_vec vm;
    cat89_vec vd;
    cat89_vec vc;
    int done;
    cat89_status st;

    alloc = cat89_allocator_default();
    cat89_vec_init(&vm, sizeof(cat89_mor *));
    cat89_vec_init(&vd, sizeof(const cat89_obj *));
    cat89_vec_init(&vc, sizeof(const cat89_obj *));
    it = NULL;
    st = cat89_mor_iter_open(en, &it);
    if (st != CAT89_OK)
    {
        return st;
    }
    done = 0;
    while (!done)
    {
        st = exc_fill_step(cat, en, it, &vm, &vd, &vc, alloc, &done);
        if (st != CAT89_OK)
        {
            exc_fill_abort(cat, en, it, alloc, &vm, &vd, &vc);
            return st;
        }
    }
    cat89_mor_iter_close(en, it);
    cs->mors = (cat89_mor **)vm.data;
    cs->dom = (const cat89_obj **)vd.data;
    cs->cod = (const cat89_obj **)vc.data;
    cs->n = vm.len;
    return CAT89_OK;
}

static cat89_status exc_left_right(cat89_category *cat, cat89_eq *eq,
                                   const struct exc *cs, unsigned long i,
                                   cat89_check_result *res)
{
    cat89_status st;
    int valid;

    valid = 0;
    st = cat89_check_left_identity(cat, eq, cs->mors[i], &valid);
    if (st != CAT89_OK)
    {
        return st;
    }
    ++res->checked;
    if (valid == 0)
    {
        ++res->failed;
    }
    valid = 0;
    st = cat89_check_right_identity(cat, eq, cs->mors[i], &valid);
    if (st != CAT89_OK)
    {
        return st;
    }
    ++res->checked;
    if (valid == 0)
    {
        ++res->failed;
    }
    return CAT89_OK;
}

static cat89_status exc_triple(cat89_category *cat, cat89_eq *eq,
                               const struct exc *cs, unsigned long i,
                               unsigned long j, unsigned long k,
                               cat89_check_result *res)
{
    cat89_status st;
    int valid;

    if (cat89_obj_same(cat, cs->cod[k], cs->dom[j]) == 0)
    {
        return CAT89_OK;
    }
    if (cat89_obj_same(cat, cs->cod[j], cs->dom[i]) == 0)
    {
        return CAT89_OK;
    }
    valid = 0;
    st = cat89_check_associativity(cat, eq, cs->mors[i], cs->mors[j],
                                   cs->mors[k], &valid);
    if (st != CAT89_OK)
    {
        return st;
    }
    ++res->checked;
    if (valid == 0)
    {
        ++res->failed;
    }
    return CAT89_OK;
}

cat89_status cat89_check_category_exhaustive(cat89_category *category,
                                             cat89_eq *eq,
                                             cat89_enum *enumeration,
                                             cat89_check_result *result)
{
    struct exc cs;
    unsigned long i;
    unsigned long j;
    unsigned long k;
    cat89_status st;

    if (result == NULL)
    {
        return CAT89_INVALID;
    }
    result->checked = 0;
    result->failed = 0;
    result->status = CAT89_OK;
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (eq == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    if (enumeration == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    if (cat89_eq_category(eq) != category)
    {
        return CAT89_INVALID;
    }
    if (cat89_enum_category(enumeration) != category)
    {
        return CAT89_INVALID;
    }
    cs.category = category;
    cs.mors = NULL;
    cs.dom = NULL;
    cs.cod = NULL;
    cs.n = 0;
    st = exc_open_fill(category, enumeration, &cs);
    if (st != CAT89_OK)
    {
        return st;
    }
    for (i = 0; i < cs.n; i = i + 1)
    {
        st = exc_left_right(category, eq, &cs, i, result);
        if (st != CAT89_OK)
        {
            exc_free(&cs);
            return st;
        }
    }
    for (i = 0; i < cs.n; i = i + 1)
    {
        for (j = 0; j < cs.n; j = j + 1)
        {
            for (k = 0; k < cs.n; k = k + 1)
            {
                st = exc_triple(category, eq, &cs, i, j, k, result);
                if (st != CAT89_OK)
                {
                    exc_free(&cs);
                    return st;
                }
            }
        }
    }
    exc_free(&cs);
    return CAT89_OK;
}

/* -------------------- exhaustive derived law-family sweeps (W5) ------- */

/* Count the genuine isomorph pairs (forward = mors[i], inverse = mors[j])
 * present in the census, two inverse-law cases per pair. Non-inverse-structural
 * pairs and non-isomorphic reverse pairs are skipped (never a false failure).
 */
static cat89_status iso_pair_count(cat89_category *category, cat89_eq *eq,
                                   const struct exc *cs, unsigned long i,
                                   unsigned long j, cat89_check_result *res)
{
    cat89_status st;
    int e1;
    int e2;

    if (cat89_obj_same(category, cs->dom[j], cs->cod[i]) == 0)
    {
        return CAT89_OK;
    }
    if (cat89_obj_same(category, cs->cod[j], cs->dom[i]) == 0)
    {
        return CAT89_OK;
    }
    e1 = 0;
    e2 = 0;
    st = law_identity(category, eq, cs->mors[j], cs->mors[i], cs->dom[i], &e1);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = law_identity(category, eq, cs->mors[i], cs->mors[j], cs->cod[i], &e2);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (e1 == 0)
    {
        return CAT89_OK;
    }
    if (e2 == 0)
    {
        return CAT89_OK;
    }
    res->checked = res->checked + 2;
    return CAT89_OK;
}

cat89_status cat89_check_iso_exhaustive(cat89_category *category, cat89_eq *eq,
                                        cat89_enum *enumeration,
                                        cat89_check_result *result)
{
    struct exc cs;
    unsigned long i;
    unsigned long j;
    cat89_status st;

    if (result == NULL)
    {
        return CAT89_INVALID;
    }
    result->checked = 0;
    result->failed = 0;
    result->status = CAT89_OK;
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (eq == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    if (enumeration == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    if (cat89_eq_category(eq) != category)
    {
        return CAT89_INVALID;
    }
    if (cat89_enum_category(enumeration) != category)
    {
        return CAT89_INVALID;
    }
    cs.category = category;
    cs.mors = NULL;
    cs.dom = NULL;
    cs.cod = NULL;
    cs.n = 0;
    st = exc_open_fill(category, enumeration, &cs);
    if (st != CAT89_OK)
    {
        return st;
    }
    for (i = 0; i < cs.n; i = i + 1)
    {
        for (j = 0; j < cs.n; j = j + 1)
        {
            st = iso_pair_count(category, eq, &cs, i, j, result);
            if (st != CAT89_OK)
            {
                exc_free(&cs);
                return st;
            }
        }
    }
    exc_free(&cs);
    return CAT89_OK;
}

static cat89_status split_pair_count(cat89_category *category, cat89_eq *eq,
                                     const struct exc *cs, unsigned long i,
                                     unsigned long j, int epi,
                                     cat89_check_result *res)
{
    cat89_status st;
    int equal;

    if (cat89_obj_same(category, cs->dom[j], cs->cod[i]) == 0)
    {
        return CAT89_OK;
    }
    if (cat89_obj_same(category, cs->cod[j], cs->dom[i]) == 0)
    {
        return CAT89_OK;
    }
    equal = 0;
    if (epi)
    {
        st = law_identity(category, eq, cs->mors[i], cs->mors[j], cs->cod[i],
                          &equal);
    }
    else
    {
        st = law_identity(category, eq, cs->mors[j], cs->mors[i], cs->dom[i],
                          &equal);
    }
    if (st != CAT89_OK)
    {
        return st;
    }
    if (equal == 0)
    {
        return CAT89_OK;
    }
    res->checked = res->checked + 1;
    return CAT89_OK;
}

static cat89_status split_exhaustive_run(cat89_category *category, cat89_eq *eq,
                                         cat89_enum *enumeration, int epi,
                                         cat89_check_result *result)
{
    struct exc cs;
    unsigned long i;
    unsigned long j;
    cat89_status st;

    cs.category = category;
    cs.mors = NULL;
    cs.dom = NULL;
    cs.cod = NULL;
    cs.n = 0;
    st = exc_open_fill(category, enumeration, &cs);
    if (st != CAT89_OK)
    {
        return st;
    }
    for (i = 0; i < cs.n; i = i + 1)
    {
        for (j = 0; j < cs.n; j = j + 1)
        {
            st = split_pair_count(category, eq, &cs, i, j, epi, result);
            if (st != CAT89_OK)
            {
                exc_free(&cs);
                return st;
            }
        }
    }
    exc_free(&cs);
    return CAT89_OK;
}

cat89_status cat89_check_split_mono_exhaustive(cat89_category *category,
                                               cat89_eq *eq,
                                               cat89_enum *enumeration,
                                               cat89_check_result *result)
{
    cat89_status st;

    if (result == NULL)
    {
        return CAT89_INVALID;
    }
    result->checked = 0;
    result->failed = 0;
    result->status = CAT89_OK;
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (eq == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    if (enumeration == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    if (cat89_eq_category(eq) != category)
    {
        return CAT89_INVALID;
    }
    if (cat89_enum_category(enumeration) != category)
    {
        return CAT89_INVALID;
    }
    st = split_exhaustive_run(category, eq, enumeration, 0, result);
    return st;
}

cat89_status cat89_check_split_epi_exhaustive(cat89_category *category,
                                              cat89_eq *eq,
                                              cat89_enum *enumeration,
                                              cat89_check_result *result)
{
    cat89_status st;

    if (result == NULL)
    {
        return CAT89_INVALID;
    }
    result->checked = 0;
    result->failed = 0;
    result->status = CAT89_OK;
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (eq == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    if (enumeration == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    if (cat89_eq_category(eq) != category)
    {
        return CAT89_INVALID;
    }
    if (cat89_enum_category(enumeration) != category)
    {
        return CAT89_INVALID;
    }
    st = split_exhaustive_run(category, eq, enumeration, 1, result);
    return st;
}

static cat89_status foid_object(cat89_functor *functor, cat89_eq *target_eq,
                                cat89_enum *source_enum, cat89_obj_iter *it,
                                cat89_check_result *res, int *out_done)
{
    const cat89_obj *obj;
    cat89_status st;
    int valid;

    obj = NULL;
    valid = 0;
    st = cat89_obj_iter_next(source_enum, it, &obj, out_done);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (*out_done)
    {
        return CAT89_OK;
    }
    st = cat89_check_functor_identity(functor, target_eq, obj, &valid);
    if (st != CAT89_OK)
    {
        return st;
    }
    res->checked = res->checked + 1;
    if (valid == 0)
    {
        ++res->failed;
    }
    return CAT89_OK;
}

static cat89_status functor_identity_pass(cat89_functor *functor,
                                          cat89_eq *target_eq,
                                          cat89_enum *source_enum,
                                          cat89_check_result *res)
{
    cat89_obj_iter *it;
    cat89_status st;
    int done;

    it = NULL;
    st = cat89_obj_iter_open(source_enum, &it);
    if (st != CAT89_OK)
    {
        return st;
    }
    done = 0;
    while (!done)
    {
        st = foid_object(functor, target_eq, source_enum, it, res, &done);
        if (st != CAT89_OK)
        {
            cat89_obj_iter_close(source_enum, it);
            return st;
        }
    }
    cat89_obj_iter_close(source_enum, it);
    return CAT89_OK;
}

static cat89_status functor_comp_pair(cat89_functor *functor,
                                      cat89_eq *target_eq, const struct exc *cs,
                                      unsigned long i, unsigned long j,
                                      cat89_check_result *res)
{
    cat89_status st;
    int valid;

    if (cat89_obj_same(cat89_functor_source(functor), cs->cod[j], cs->dom[i]) ==
        0)
    {
        return CAT89_OK;
    }
    valid = 0;
    st = cat89_check_functor_composition(functor, target_eq, cs->mors[i],
                                         cs->mors[j], &valid);
    if (st != CAT89_OK)
    {
        return st;
    }
    res->checked = res->checked + 1;
    if (valid == 0)
    {
        ++res->failed;
    }
    return CAT89_OK;
}

static cat89_status functor_composition_pass(cat89_functor *functor,
                                             cat89_eq *target_eq,
                                             cat89_enum *source_enum,
                                             cat89_check_result *res)
{
    cat89_category *source;
    struct exc cs;
    unsigned long i;
    unsigned long j;
    cat89_status st;

    source = cat89_functor_source(functor);
    cs.category = source;
    cs.mors = NULL;
    cs.dom = NULL;
    cs.cod = NULL;
    cs.n = 0;
    st = exc_open_fill(source, source_enum, &cs);
    if (st != CAT89_OK)
    {
        return st;
    }
    for (i = 0; i < cs.n; i = i + 1)
    {
        for (j = 0; j < cs.n; j = j + 1)
        {
            st = functor_comp_pair(functor, target_eq, &cs, i, j, res);
            if (st != CAT89_OK)
            {
                exc_free(&cs);
                return st;
            }
        }
    }
    exc_free(&cs);
    return CAT89_OK;
}

cat89_status cat89_check_functor_exhaustive(cat89_functor *functor,
                                            cat89_eq *target_eq,
                                            cat89_enum *source_enum,
                                            cat89_check_result *result)
{
    cat89_status st;

    if (result == NULL)
    {
        return CAT89_INVALID;
    }
    result->checked = 0;
    result->failed = 0;
    result->status = CAT89_OK;
    if (functor == NULL)
    {
        return CAT89_INVALID;
    }
    if (target_eq == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    if (source_enum == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    if (cat89_eq_category(target_eq) != cat89_functor_target(functor))
    {
        return CAT89_INVALID;
    }
    if (cat89_enum_category(source_enum) != cat89_functor_source(functor))
    {
        return CAT89_INVALID;
    }
    st = functor_identity_pass(functor, target_eq, source_enum, result);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = functor_composition_pass(functor, target_eq, source_enum, result);
    return st;
}

static cat89_status naturality_mor(cat89_nat *nat, cat89_eq *target_eq,
                                   cat89_enum *source_enum, cat89_mor_iter *it,
                                   cat89_check_result *res, int *out_done)
{
    cat89_category *source;
    cat89_mor *m;
    cat89_status st;
    int valid;

    source = cat89_enum_category(source_enum);
    m = NULL;
    valid = 0;
    st = cat89_mor_iter_next(source_enum, it, &m, out_done);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (*out_done)
    {
        return CAT89_OK;
    }
    st = cat89_check_naturality(nat, target_eq, m, &valid);
    cat89_mor_release(source, m);
    if (st != CAT89_OK)
    {
        return st;
    }
    res->checked = res->checked + 1;
    if (valid == 0)
    {
        ++res->failed;
    }
    return CAT89_OK;
}

cat89_status cat89_check_naturality_exhaustive(cat89_nat *nat,
                                               cat89_eq *target_eq,
                                               cat89_enum *source_enum,
                                               cat89_check_result *result)
{
    cat89_mor_iter *it;
    cat89_status st;
    int done;

    if (result == NULL)
    {
        return CAT89_INVALID;
    }
    result->checked = 0;
    result->failed = 0;
    result->status = CAT89_OK;
    if (nat == NULL)
    {
        return CAT89_INVALID;
    }
    if (target_eq == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    if (source_enum == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    if (cat89_eq_category(target_eq) !=
        cat89_functor_target(cat89_nat_source(nat)))
    {
        return CAT89_INVALID;
    }
    if (cat89_enum_category(source_enum) !=
        cat89_functor_source(cat89_nat_source(nat)))
    {
        return CAT89_INVALID;
    }
    it = NULL;
    st = cat89_mor_iter_open(source_enum, &it);
    if (st != CAT89_OK)
    {
        return st;
    }
    done = 0;
    while (!done)
    {
        st = naturality_mor(nat, target_eq, source_enum, it, result, &done);
        if (st != CAT89_OK)
        {
            cat89_mor_iter_close(source_enum, it);
            return st;
        }
    }
    cat89_mor_iter_close(source_enum, it);
    return CAT89_OK;
}

static cat89_status cone_cell(const cat89_cone *cone, cat89_eq *eq,
                              cat89_enum *shape_enum, cat89_mor_iter *it,
                              cat89_check_result *res, int *out_done)
{
    cat89_category *shape;
    cat89_category *ccat;
    cat89_diagram *diagram;
    cat89_mor *m;
    cat89_status st;
    int invalid;

    diagram = cat89_cone_diagram(cone);
    shape = cat89_functor_source(diagram);
    ccat = cat89_cone_category(cone);
    m = NULL;
    invalid = 0;
    st = cat89_mor_iter_next(shape_enum, it, &m, out_done);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (*out_done)
    {
        return CAT89_OK;
    }
    st = cone_mor_check(ccat, shape, eq, diagram, cone, m, &invalid);
    cat89_mor_release(shape, m);
    if (st != CAT89_OK)
    {
        return st;
    }
    res->checked = res->checked + 1;
    if (invalid)
    {
        ++res->failed;
    }
    return CAT89_OK;
}

cat89_status cat89_check_cone_exhaustive(const cat89_cone *cone, cat89_eq *eq,
                                         cat89_enum *shape_enum,
                                         cat89_check_result *result)
{
    cat89_mor_iter *it;
    cat89_status st;
    int done;

    if (result == NULL)
    {
        return CAT89_INVALID;
    }
    result->checked = 0;
    result->failed = 0;
    result->status = CAT89_OK;
    if (cone == NULL)
    {
        return CAT89_INVALID;
    }
    if (eq == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    if (shape_enum == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    if (cat89_eq_category(eq) != cat89_cone_category(cone))
    {
        return CAT89_INVALID;
    }
    if (cat89_enum_category(shape_enum) !=
        cat89_functor_source(cat89_cone_diagram(cone)))
    {
        return CAT89_INVALID;
    }
    it = NULL;
    st = cat89_mor_iter_open(shape_enum, &it);
    if (st != CAT89_OK)
    {
        return st;
    }
    done = 0;
    while (!done)
    {
        st = cone_cell(cone, eq, shape_enum, it, result, &done);
        if (st != CAT89_OK)
        {
            cat89_mor_iter_close(shape_enum, it);
            return st;
        }
    }
    cat89_mor_iter_close(shape_enum, it);
    return CAT89_OK;
}

static cat89_status cocone_cell(const cat89_cocone *cocone, cat89_eq *eq,
                                cat89_enum *shape_enum, cat89_mor_iter *it,
                                cat89_check_result *res, int *out_done)
{
    cat89_category *shape;
    cat89_category *ccat;
    cat89_diagram *diagram;
    cat89_mor *m;
    cat89_status st;
    int invalid;

    diagram = cat89_cocone_diagram(cocone);
    shape = cat89_functor_source(diagram);
    ccat = cat89_cocone_category(cocone);
    m = NULL;
    invalid = 0;
    st = cat89_mor_iter_next(shape_enum, it, &m, out_done);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (*out_done)
    {
        return CAT89_OK;
    }
    st = cocone_mor_check(ccat, shape, eq, diagram, cocone, m, &invalid);
    cat89_mor_release(shape, m);
    if (st != CAT89_OK)
    {
        return st;
    }
    res->checked = res->checked + 1;
    if (invalid)
    {
        ++res->failed;
    }
    return CAT89_OK;
}

cat89_status cat89_check_cocone_exhaustive(const cat89_cocone *cocone,
                                           cat89_eq *eq, cat89_enum *shape_enum,
                                           cat89_check_result *result)
{
    cat89_mor_iter *it;
    cat89_status st;
    int done;

    if (result == NULL)
    {
        return CAT89_INVALID;
    }
    result->checked = 0;
    result->failed = 0;
    result->status = CAT89_OK;
    if (cocone == NULL)
    {
        return CAT89_INVALID;
    }
    if (eq == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    if (shape_enum == NULL)
    {
        return CAT89_NOT_SUPPORTED;
    }
    if (cat89_eq_category(eq) != cat89_cocone_category(cocone))
    {
        return CAT89_INVALID;
    }
    if (cat89_enum_category(shape_enum) !=
        cat89_functor_source(cat89_cocone_diagram(cocone)))
    {
        return CAT89_INVALID;
    }
    it = NULL;
    st = cat89_mor_iter_open(shape_enum, &it);
    if (st != CAT89_OK)
    {
        return st;
    }
    done = 0;
    while (!done)
    {
        st = cocone_cell(cocone, eq, shape_enum, it, result, &done);
        if (st != CAT89_OK)
        {
            cat89_mor_iter_close(shape_enum, it);
            return st;
        }
    }
    cat89_mor_iter_close(shape_enum, it);
    return CAT89_OK;
}
