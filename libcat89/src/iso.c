/* cat89_iso.c - isomorphisms as packaged ordinary morphisms. */

#include "cat89_internal.h"
#include <cat89/iso.h>

struct cat89_iso
{
    unsigned long refs;
    cat89_category *category;
    cat89_mor *forward;
    cat89_mor *inverse;
    cat89_allocator allocator;
};

static void iso_init(cat89_iso *iso, cat89_category *category,
                     cat89_mor *forward, cat89_mor *inverse,
                     const cat89_allocator *allocator)
{
    iso->refs = 1;
    iso->category = category;
    iso->forward = forward;
    iso->inverse = inverse;
    iso->allocator = *allocator;
}

static const cat89_allocator *resolve_alloc(const cat89_allocator *allocator)
{
    const cat89_allocator *actual;

    if (allocator == NULL)
    {
        actual = cat89_allocator_default();
    }
    else
    {
        actual = allocator;
    }
    return actual;
}

static void rel2(cat89_category *c, cat89_mor *f)
{
    cat89_mor_release(c, f);
    cat89_category_release(c);
}

static void rel3(cat89_category *c, cat89_mor *f, cat89_mor *g)
{
    cat89_mor_release(c, g);
    rel2(c, f);
}

static cat89_status iso_new_inner(cat89_category *category, cat89_mor *forward,
                                  cat89_mor *inverse,
                                  const cat89_allocator *allocator,
                                  cat89_iso **out_iso)
{
    cat89_iso *iso;
    cat89_status st;
    const cat89_allocator *actual;

    if (out_iso == NULL)
    {
        return CAT89_INVALID;
    }
    *out_iso = NULL;
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (forward == NULL)
    {
        return CAT89_INVALID;
    }
    if (inverse == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(category, forward) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(category, inverse) == 0)
    {
        return CAT89_INVALID;
    }

    actual = resolve_alloc(allocator);

    st = cat89_category_retain(category);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_mor_retain(category, forward);
    if (st != CAT89_OK)
    {
        cat89_category_release(category);
        return st;
    }
    st = cat89_mor_retain(category, inverse);
    if (st != CAT89_OK)
    {
        rel2(category, forward);
        return st;
    }

    iso = cat89_alloc(actual, sizeof(*iso));
    if (iso == NULL)
    {
        rel3(category, forward, inverse);
        return CAT89_NOMEM;
    }

    iso_init(iso, category, forward, inverse, actual);
    *out_iso = iso;
    return CAT89_OK;
}

cat89_status cat89_iso_new(cat89_category *category, cat89_mor *forward,
                           cat89_mor *inverse, const cat89_allocator *allocator,
                           cat89_iso **out_iso)
{
    cat89_status st;

    st = iso_new_inner(category, forward, inverse, allocator, out_iso);
    return st;
}

static void iso_destroy(cat89_iso *iso)
{
    cat89_mor_release(iso->category, iso->forward);
    cat89_mor_release(iso->category, iso->inverse);
    cat89_category_release(iso->category);
    cat89_free(&iso->allocator, iso);
}

cat89_status cat89_iso_retain(cat89_iso *iso)
{
    cat89_status st;

    if (iso == NULL)
    {
        return CAT89_INVALID;
    }
    st = cat89_ref_inc(&iso->refs);
    return st;
}

void cat89_iso_release(cat89_iso *iso)
{
    int zero;

    if (iso == NULL)
    {
        return;
    }
    zero = cat89_ref_dec(&iso->refs);
    if (zero)
    {
        iso_destroy(iso);
    }
}

cat89_category *cat89_iso_category(const cat89_iso *iso)
{
    if (iso == NULL)
    {
        return NULL;
    }
    return iso->category;
}

cat89_mor *cat89_iso_forward(const cat89_iso *iso)
{
    if (iso == NULL)
    {
        return NULL;
    }
    return iso->forward;
}

cat89_mor *cat89_iso_inverse(const cat89_iso *iso)
{
    if (iso == NULL)
    {
        return NULL;
    }
    return iso->inverse;
}

cat89_status cat89_iso_identity(cat89_category *category, const cat89_obj *obj,
                                const cat89_allocator *allocator,
                                cat89_iso **out_iso)
{
    cat89_mor *id1;
    cat89_mor *id2;
    cat89_status st;

    id1 = NULL;
    id2 = NULL;
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (obj == NULL)
    {
        return CAT89_INVALID;
    }
    st = cat89_identity(category, obj, &id1);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_identity(category, obj, &id2);
    if (st != CAT89_OK)
    {
        cat89_mor_release(category, id1);
        return st;
    }
    st = iso_new_inner(category, id1, id2, allocator, out_iso);
    cat89_mor_release(category, id1);
    cat89_mor_release(category, id2);
    return st;
}

cat89_status cat89_iso_invert(const cat89_iso *iso,
                              const cat89_allocator *allocator,
                              cat89_iso **out_iso)
{
    cat89_status st;

    if (out_iso == NULL)
    {
        return CAT89_INVALID;
    }
    *out_iso = NULL;
    if (iso == NULL)
    {
        return CAT89_INVALID;
    }
    st = iso_new_inner(iso->category, iso->inverse, iso->forward, allocator,
                       out_iso);
    return st;
}

static void rel_mor(cat89_category *category, cat89_mor *mor)
{
    cat89_mor_release(category, mor);
}

cat89_status cat89_iso_compose(const cat89_iso *g, const cat89_iso *f,
                               const cat89_allocator *allocator,
                               cat89_iso **out_iso)
{
    cat89_mor *fwd;
    cat89_mor *inv;
    cat89_status st;

    fwd = NULL;
    inv = NULL;
    if (out_iso == NULL)
    {
        return CAT89_INVALID;
    }
    *out_iso = NULL;
    if (g == NULL)
    {
        return CAT89_INVALID;
    }
    if (f == NULL)
    {
        return CAT89_INVALID;
    }
    if (g->category != f->category)
    {
        return CAT89_INVALID;
    }

    st = cat89_compose(g->category, g->forward, f->forward, &fwd);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_compose(g->category, f->inverse, g->inverse, &inv);
    if (st != CAT89_OK)
    {
        rel_mor(g->category, fwd);
        return st;
    }
    st = iso_new_inner(g->category, fwd, inv, allocator, out_iso);
    rel_mor(g->category, fwd);
    rel_mor(g->category, inv);
    return st;
}
