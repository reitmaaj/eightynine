/* cat89_shape.c - small reusable finite diagram shape categories. */

#include "cat89_internal.h"
#include <cat89/finite.h>
#include <cat89/shape.h>

#define MAXOBJ 8
#define MAXMOR 16

struct shape_table
{
    unsigned long nobj;
    unsigned long nmor;
    unsigned long dom[MAXMOR];
    unsigned long cod[MAXMOR];
    unsigned long ident[MAXOBJ];
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

static void table_identity(struct shape_table *t, unsigned long o,
                           unsigned long m)
{
    t->ident[o] = m;
    t->dom[m] = o;
    t->cod[m] = o;
}

static void table_mor(struct shape_table *t, unsigned long m, unsigned long d,
                      unsigned long c)
{
    t->dom[m] = d;
    t->cod[m] = c;
}

static void init_discrete2(struct shape_table *t)
{
    t->nobj = 2;
    t->nmor = 2;
    table_identity(t, 0, 0);
    table_identity(t, 1, 1);
}

static void init_parallel(struct shape_table *t)
{
    t->nobj = 2;
    t->nmor = 4;
    table_identity(t, 0, 0);
    table_identity(t, 1, 1);
    table_mor(t, 2, 0, 1);
    table_mor(t, 3, 0, 1);
}

static void init_span(struct shape_table *t)
{
    t->nobj = 3;
    t->nmor = 5;
    table_identity(t, 0, 0);
    table_identity(t, 1, 1);
    table_identity(t, 2, 2);
    table_mor(t, 3, 0, 2);
    table_mor(t, 4, 1, 2);
}

static void init_cospan(struct shape_table *t)
{
    t->nobj = 3;
    t->nmor = 5;
    table_identity(t, 0, 0);
    table_identity(t, 1, 1);
    table_identity(t, 2, 2);
    table_mor(t, 3, 0, 1);
    table_mor(t, 4, 0, 2);
}

static void init_shape(enum cat89_shape_kind kind, struct shape_table *t)
{
    t->nobj = 0;
    t->nmor = 0;
    if (kind == CAT89_SHAPE_DISCRETE2)
    {
        init_discrete2(t);
    }
    else if (kind == CAT89_SHAPE_PARALLEL)
    {
        init_parallel(t);
    }
    else if (kind == CAT89_SHAPE_SPAN)
    {
        init_span(t);
    }
    else if (kind == CAT89_SHAPE_COSPAN)
    {
        init_cospan(t);
    }
}

static cat89_status table_add(cat89_finite_builder *builder,
                              const struct shape_table *t)
{
    cat89_finite_obj_id oid;
    cat89_finite_mor_id mid;
    cat89_status st;
    unsigned long i;

    for (i = 0; i < t->nobj; i = i + 1)
    {
        st = cat89_finite_add_object(builder, &oid);
        if (st != CAT89_OK)
        {
            return st;
        }
    }
    for (i = 0; i < t->nmor; i = i + 1)
    {
        st = cat89_finite_add_morphism(builder, t->dom[i], t->cod[i], &mid);
        if (st != CAT89_OK)
        {
            return st;
        }
    }
    for (i = 0; i < t->nobj; i = i + 1)
    {
        st = cat89_finite_set_identity(builder, i, t->ident[i]);
        if (st != CAT89_OK)
        {
            return st;
        }
    }
    return CAT89_OK;
}

static cat89_status table_compose_set(cat89_finite_builder *builder,
                                      const struct shape_table *t,
                                      unsigned long g, unsigned long f)
{
    unsigned long result;
    cat89_status st;

    if (f == t->ident[t->dom[f]])
    {
        result = g;
    }
    else
    {
        result = f;
    }
    st = cat89_finite_set_composition(builder, g, f, result);
    return st;
}

static cat89_status table_compose_pair(cat89_finite_builder *builder,
                                       const struct shape_table *t,
                                       unsigned long g, unsigned long f)
{
    cat89_status st;

    if (t->cod[f] != t->dom[g])
    {
        return CAT89_OK;
    }
    st = table_compose_set(builder, t, g, f);
    return st;
}

static cat89_status table_compose(cat89_finite_builder *builder,
                                  const struct shape_table *t)
{
    cat89_status st;
    unsigned long g;
    unsigned long f;

    for (g = 0; g < t->nmor; g = g + 1)
    {
        for (f = 0; f < t->nmor; f = f + 1)
        {
            st = table_compose_pair(builder, t, g, f);
            if (st != CAT89_OK)
            {
                return st;
            }
        }
    }
    return CAT89_OK;
}

cat89_status cat89_shape_category_new(enum cat89_shape_kind kind,
                                      const cat89_allocator *allocator,
                                      cat89_category **out_category,
                                      cat89_eq **out_eq, cat89_enum **out_enum)
{
    const cat89_allocator *actual;
    struct shape_table t;
    cat89_finite_builder *builder;
    cat89_status st;

    if (out_category == NULL)
    {
        return CAT89_INVALID;
    }
    *out_category = NULL;
    if (out_eq != NULL)
    {
        *out_eq = NULL;
    }
    if (out_enum != NULL)
    {
        *out_enum = NULL;
    }
    actual = resolve_alloc(allocator);

    init_shape(kind, &t);
    builder = NULL;
    st = cat89_finite_builder_new(actual, &builder);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = table_add(builder, &t);
    if (st != CAT89_OK)
    {
        cat89_finite_builder_release(builder);
        return st;
    }
    st = table_compose(builder, &t);
    if (st != CAT89_OK)
    {
        cat89_finite_builder_release(builder);
        return st;
    }
    st = cat89_finite_build(builder, out_category, out_eq, out_enum);
    cat89_finite_builder_release(builder);
    return st;
}
