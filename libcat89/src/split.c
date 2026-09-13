/* cat89_split.c - split (section/retraction) structures. */

#include "cat89_internal.h"
#include <cat89/split.h>

struct cat89_split_mono
{
    unsigned long refs;
    cat89_category *category;
    cat89_mor *section;
    cat89_mor *retraction;
    cat89_allocator allocator;
};

struct cat89_split_epi
{
    unsigned long refs;
    cat89_category *category;
    cat89_mor *section;
    cat89_mor *retraction;
    cat89_allocator allocator;
};

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

static cat89_status retain_pair(cat89_category *category, cat89_mor *a,
                                cat89_mor *b)
{
    cat89_status st;

    st = cat89_category_retain(category);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_mor_retain(category, a);
    if (st != CAT89_OK)
    {
        cat89_category_release(category);
        return st;
    }
    st = cat89_mor_retain(category, b);
    if (st != CAT89_OK)
    {
        rel2(category, a);
        return st;
    }
    return CAT89_OK;
}

cat89_status cat89_split_mono_new(cat89_category *category, cat89_mor *section,
                                  cat89_mor *retraction,
                                  const cat89_allocator *allocator,
                                  cat89_split_mono **out_split)
{
    cat89_split_mono *split;
    cat89_status st;
    const cat89_allocator *actual;

    if (out_split == NULL)
    {
        return CAT89_INVALID;
    }
    *out_split = NULL;
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (section == NULL)
    {
        return CAT89_INVALID;
    }
    if (retraction == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(category, section) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(category, retraction) == 0)
    {
        return CAT89_INVALID;
    }
    actual = resolve_alloc(allocator);

    st = retain_pair(category, section, retraction);
    if (st != CAT89_OK)
    {
        return st;
    }
    split = cat89_alloc(actual, sizeof(*split));
    if (split == NULL)
    {
        rel3(category, section, retraction);
        return CAT89_NOMEM;
    }
    split->refs = 1;
    split->category = category;
    split->section = section;
    split->retraction = retraction;
    split->allocator = *actual;
    *out_split = split;
    return CAT89_OK;
}

static void split_mono_destroy(cat89_split_mono *split)
{
    cat89_mor_release(split->category, split->section);
    cat89_mor_release(split->category, split->retraction);
    cat89_category_release(split->category);
    cat89_free(&split->allocator, split);
}

cat89_status cat89_split_mono_retain(cat89_split_mono *split)
{
    cat89_status st;

    if (split == NULL)
    {
        return CAT89_INVALID;
    }
    st = cat89_ref_inc(&split->refs);
    return st;
}

void cat89_split_mono_release(cat89_split_mono *split)
{
    int zero;

    if (split == NULL)
    {
        return;
    }
    zero = cat89_ref_dec(&split->refs);
    if (zero)
    {
        split_mono_destroy(split);
    }
}

cat89_category *cat89_split_mono_category(const cat89_split_mono *split)
{
    if (split == NULL)
    {
        return NULL;
    }
    return split->category;
}

const cat89_mor *cat89_split_mono_section(const cat89_split_mono *split)
{
    if (split == NULL)
    {
        return NULL;
    }
    return split->section;
}

const cat89_mor *cat89_split_mono_retraction(const cat89_split_mono *split)
{
    if (split == NULL)
    {
        return NULL;
    }
    return split->retraction;
}

cat89_status cat89_split_epi_new(cat89_category *category, cat89_mor *section,
                                 cat89_mor *retraction,
                                 const cat89_allocator *allocator,
                                 cat89_split_epi **out_split)
{
    cat89_split_epi *split;
    cat89_status st;
    const cat89_allocator *actual;

    if (out_split == NULL)
    {
        return CAT89_INVALID;
    }
    *out_split = NULL;
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (section == NULL)
    {
        return CAT89_INVALID;
    }
    if (retraction == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(category, section) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_mor(category, retraction) == 0)
    {
        return CAT89_INVALID;
    }
    actual = resolve_alloc(allocator);

    st = retain_pair(category, section, retraction);
    if (st != CAT89_OK)
    {
        return st;
    }
    split = cat89_alloc(actual, sizeof(*split));
    if (split == NULL)
    {
        rel3(category, section, retraction);
        return CAT89_NOMEM;
    }
    split->refs = 1;
    split->category = category;
    split->section = section;
    split->retraction = retraction;
    split->allocator = *actual;
    *out_split = split;
    return CAT89_OK;
}

static void split_epi_destroy(cat89_split_epi *split)
{
    cat89_mor_release(split->category, split->section);
    cat89_mor_release(split->category, split->retraction);
    cat89_category_release(split->category);
    cat89_free(&split->allocator, split);
}

cat89_status cat89_split_epi_retain(cat89_split_epi *split)
{
    cat89_status st;

    if (split == NULL)
    {
        return CAT89_INVALID;
    }
    st = cat89_ref_inc(&split->refs);
    return st;
}

void cat89_split_epi_release(cat89_split_epi *split)
{
    int zero;

    if (split == NULL)
    {
        return;
    }
    zero = cat89_ref_dec(&split->refs);
    if (zero)
    {
        split_epi_destroy(split);
    }
}

cat89_category *cat89_split_epi_category(const cat89_split_epi *split)
{
    if (split == NULL)
    {
        return NULL;
    }
    return split->category;
}

const cat89_mor *cat89_split_epi_section(const cat89_split_epi *split)
{
    if (split == NULL)
    {
        return NULL;
    }
    return split->section;
}

const cat89_mor *cat89_split_epi_retraction(const cat89_split_epi *split)
{
    if (split == NULL)
    {
        return NULL;
    }
    return split->retraction;
}

/* --------------------------------------------- iso -> split adapters */

cat89_status cat89_iso_as_split_mono(const cat89_iso *iso,
                                     const cat89_allocator *allocator,
                                     cat89_split_mono **out_split)
{
    cat89_category *category;
    cat89_mor *fwd;
    cat89_mor *inv;
    cat89_status st;

    if (out_split == NULL)
    {
        return CAT89_INVALID;
    }
    *out_split = NULL;
    if (iso == NULL)
    {
        return CAT89_INVALID;
    }
    category = cat89_iso_category(iso);
    fwd = cat89_iso_forward(iso);
    inv = cat89_iso_inverse(iso);
    st = cat89_split_mono_new(category, fwd, inv, allocator, out_split);
    return st;
}

cat89_status cat89_iso_as_split_epi(const cat89_iso *iso,
                                    const cat89_allocator *allocator,
                                    cat89_split_epi **out_split)
{
    cat89_category *category;
    cat89_mor *fwd;
    cat89_mor *inv;
    cat89_status st;

    if (out_split == NULL)
    {
        return CAT89_INVALID;
    }
    *out_split = NULL;
    if (iso == NULL)
    {
        return CAT89_INVALID;
    }
    category = cat89_iso_category(iso);
    fwd = cat89_iso_forward(iso);
    inv = cat89_iso_inverse(iso);
    st = cat89_split_epi_new(category, fwd, inv, allocator, out_split);
    return st;
}

static void mor_rel2(cat89_category *category, cat89_mor *a, cat89_mor *b)
{
    cat89_mor_release(category, a);
    cat89_mor_release(category, b);
}

static cat89_status build_mono_pair(cat89_category *category,
                                    cat89_mor *section, cat89_mor *retraction,
                                    const cat89_allocator *allocator,
                                    cat89_split_mono **out_split)
{
    cat89_status st;

    st = cat89_split_mono_new(category, section, retraction, allocator,
                              out_split);
    return st;
}

static cat89_status build_epi_pair(cat89_category *category, cat89_mor *section,
                                   cat89_mor *retraction,
                                   const cat89_allocator *allocator,
                                   cat89_split_epi **out_split)
{
    cat89_status st;

    st = cat89_split_epi_new(category, section, retraction, allocator,
                             out_split);
    return st;
}

cat89_status cat89_split_mono_identity(cat89_category *category,
                                       const cat89_obj *obj,
                                       const cat89_allocator *allocator,
                                       cat89_split_mono **out_split)
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
    st = build_mono_pair(category, id1, id2, allocator, out_split);
    mor_rel2(category, id1, id2);
    return st;
}

cat89_status cat89_split_epi_identity(cat89_category *category,
                                      const cat89_obj *obj,
                                      const cat89_allocator *allocator,
                                      cat89_split_epi **out_split)
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
    st = build_epi_pair(category, id1, id2, allocator, out_split);
    mor_rel2(category, id1, id2);
    return st;
}

cat89_status cat89_split_mono_compose(const cat89_split_mono *g,
                                      const cat89_split_mono *f,
                                      const cat89_allocator *allocator,
                                      cat89_split_mono **out_split)
{
    cat89_category *category;
    const cat89_mor *sg;
    const cat89_mor *sf;
    const cat89_mor *rg;
    const cat89_mor *rf;
    cat89_mor *section;
    cat89_mor *retraction;
    cat89_status st;

    section = NULL;
    retraction = NULL;
    if (out_split == NULL)
    {
        return CAT89_INVALID;
    }
    *out_split = NULL;
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
    category = g->category;
    sg = cat89_split_mono_section(g);
    sf = cat89_split_mono_section(f);
    rg = cat89_split_mono_retraction(g);
    rf = cat89_split_mono_retraction(f);

    st = cat89_compose(category, sg, sf, &section);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_compose(category, rf, rg, &retraction);
    if (st != CAT89_OK)
    {
        cat89_mor_release(category, section);
        return st;
    }
    st = build_mono_pair(category, section, retraction, allocator, out_split);
    mor_rel2(category, section, retraction);
    return st;
}

cat89_status cat89_split_epi_compose(const cat89_split_epi *g,
                                     const cat89_split_epi *f,
                                     const cat89_allocator *allocator,
                                     cat89_split_epi **out_split)
{
    cat89_category *category;
    const cat89_mor *sg;
    const cat89_mor *sf;
    const cat89_mor *rg;
    const cat89_mor *rf;
    cat89_mor *section;
    cat89_mor *retraction;
    cat89_status st;

    section = NULL;
    retraction = NULL;
    if (out_split == NULL)
    {
        return CAT89_INVALID;
    }
    *out_split = NULL;
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
    category = g->category;
    sg = cat89_split_epi_section(g);
    sf = cat89_split_epi_section(f);
    rg = cat89_split_epi_retraction(g);
    rf = cat89_split_epi_retraction(f);

    st = cat89_compose(category, sg, sf, &section);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_compose(category, rf, rg, &retraction);
    if (st != CAT89_OK)
    {
        cat89_mor_release(category, section);
        return st;
    }
    st = build_epi_pair(category, section, retraction, allocator, out_split);
    mor_rel2(category, section, retraction);
    return st;
}
