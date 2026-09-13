/* cat89_backend_fixtures.c - one usable fixture per built-in backend. */

#include <cat89/cat89.h>

#include "backend_fixtures.h"

/* ------------------------------------------------------- finite base */

static cat89_status first_obj(cat89_enum *en, const cat89_obj **out_obj)
{
    cat89_obj_iter *it;
    const cat89_obj *o;
    int done;
    cat89_status st;

    it = NULL;
    o = NULL;
    st = cat89_obj_iter_open(en, &it);
    if (st != CAT89_OK)
    {
        return st;
    }
    done = 0;
    st = cat89_obj_iter_next(en, it, &o, &done);
    cat89_obj_iter_close(en, it);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_obj = o;
    return CAT89_OK;
}

static cat89_status pair_build(cat89_category **out_cat, cat89_eq **out_eq,
                               cat89_enum **out_en, const cat89_obj **out_obj,
                               cat89_mor **out_mor)
{
    cat89_finite_builder *b;
    cat89_finite_obj_id a;
    cat89_finite_obj_id bb;
    cat89_finite_mor_id ia;
    cat89_finite_mor_id ib;
    cat89_finite_mor_id f;
    cat89_status st;
    cat89_mor_iter *mit;
    cat89_mor *m;
    int done;
    int skip;

    b = NULL;
    st = cat89_finite_builder_new(NULL, &b);
    if (st != CAT89_OK)
    {
        return st;
    }
    a = 0;
    bb = 0;
    ia = 0;
    ib = 0;
    f = 0;
    cat89_finite_add_object(b, &a);
    cat89_finite_add_object(b, &bb);
    cat89_finite_add_morphism(b, a, a, &ia);
    cat89_finite_add_morphism(b, bb, bb, &ib);
    cat89_finite_add_morphism(b, a, bb, &f);
    cat89_finite_set_identity(b, a, ia);
    cat89_finite_set_identity(b, bb, ib);
    cat89_finite_set_composition(b, ia, ia, ia);
    cat89_finite_set_composition(b, ib, ib, ib);
    cat89_finite_set_composition(b, ib, f, f);
    cat89_finite_set_composition(b, f, ia, f);
    st = cat89_finite_build(b, out_cat, out_eq, out_en);
    cat89_finite_builder_release(b);
    if (st != CAT89_OK)
    {
        return st;
    }

    st = first_obj(*out_en, out_obj);
    if (st != CAT89_OK)
    {
        return st;
    }

    mit = NULL;
    m = NULL;
    st = cat89_mor_iter_open(*out_en, &mit);
    if (st != CAT89_OK)
    {
        return st;
    }
    for (skip = 0; skip < 2; skip = skip + 1)
    {
        done = 0;
        st = cat89_mor_iter_next(*out_en, mit, &m, &done);
        if (st != CAT89_OK)
        {
            cat89_mor_iter_close(*out_en, mit);
            return st;
        }
        cat89_mor_release(*out_cat, m);
    }
    m = NULL;
    done = 0;
    st = cat89_mor_iter_next(*out_en, mit, &m, &done);
    cat89_mor_iter_close(*out_en, mit);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_mor = m;
    return CAT89_OK;
}

static cat89_status terminal_build(cat89_category **out_cat, cat89_eq **out_eq,
                                   cat89_enum **out_en,
                                   const cat89_obj **out_obj,
                                   cat89_mor **out_id)
{
    cat89_finite_builder *b;
    cat89_finite_obj_id o;
    cat89_finite_mor_id id;
    cat89_status st;

    b = NULL;
    st = cat89_finite_builder_new(NULL, &b);
    if (st != CAT89_OK)
    {
        return st;
    }
    o = 0;
    id = 0;
    cat89_finite_add_object(b, &o);
    cat89_finite_add_morphism(b, o, o, &id);
    cat89_finite_set_identity(b, o, id);
    cat89_finite_set_composition(b, id, id, id);
    st = cat89_finite_build(b, out_cat, out_eq, out_en);
    cat89_finite_builder_release(b);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = first_obj(*out_en, out_obj);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_identity(*out_cat, *out_obj, out_id);
    return st;
}

/* ----------------------------------------------------------- free */

static const char free_vertex = 0;
static const char free_edge = 0;

static int free_vertex_same(void *ctx, const void *a, const void *b)
{
    (void)ctx;
    return a == b;
}

static const void *free_edge_source(void *ctx, const void *edge)
{
    (void)ctx;
    (void)edge;
    return &free_vertex;
}

static const void *free_edge_target(void *ctx, const void *edge)
{
    (void)ctx;
    (void)edge;
    return &free_vertex;
}

/* ---------------------------------------------------------- thin */

static const char thin_value = 0;

static int thin_same(void *ctx, const void *a, const void *b)
{
    (void)ctx;
    return a == b;
}

static cat89_status thin_leq(void *ctx, const void *a, const void *b,
                             int *out_leq)
{
    (void)ctx;
    (void)a;
    (void)b;
    *out_leq = 1;
    return CAT89_OK;
}

/* ------------------------------------------------------- builders */

static cat89_status slot_free(struct cat89_bf_slot *s)
{
    cat89_free_graph_ops ops;
    const cat89_obj *obj;
    cat89_status st;

    ops.vertex_same = free_vertex_same;
    ops.edge_source = free_edge_source;
    ops.edge_target = free_edge_target;
    st = cat89_free_category_new(&ops, NULL, NULL, &s->category);
    if (st != CAT89_OK)
    {
        return st;
    }
    obj = NULL;
    st = cat89_free_obj(s->category, &free_vertex, &obj);
    if (st != CAT89_OK)
    {
        return st;
    }
    s->obj = obj;
    st = cat89_free_edge(s->category, &free_edge, &s->mor);
    return st;
}

static cat89_status slot_thin(struct cat89_bf_slot *s)
{
    cat89_preorder_ops ops;
    const cat89_obj *obj;
    cat89_status st;

    ops.same = thin_same;
    ops.leq = thin_leq;
    st = cat89_thin_category_new(&ops, NULL, NULL, &s->category);
    if (st != CAT89_OK)
    {
        return st;
    }
    obj = NULL;
    st = cat89_thin_obj(s->category, &thin_value, &obj);
    if (st != CAT89_OK)
    {
        return st;
    }
    s->obj = obj;
    st = cat89_thin_mor(s->category, obj, obj, &s->mor);
    return st;
}

static cat89_status slot_product(struct cat89_bf_slot *s)
{
    cat89_status st;

    st = terminal_build(&s->base1, &s->base_eq1, &s->base_enum1, &s->base1_obj,
                        &s->base1_id);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = terminal_build(&s->base2, &s->base_eq2, &s->base_enum2, &s->base2_obj,
                        &s->base2_id);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_product_category_new(s->base1, s->base2, NULL, &s->category);
    if (st != CAT89_OK)
    {
        return st;
    }
    st =
        cat89_product_obj_new(s->category, s->base1_obj, s->base2_obj, &s->obj);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_product_mor_new(s->category, s->base1_id, s->base2_id, &s->mor);
    return st;
}

static cat89_status slot_opposite(struct cat89_bf_slot *s)
{
    cat89_status st;

    st = terminal_build(&s->base1, &s->base_eq1, &s->base_enum1, &s->base1_obj,
                        &s->base1_id);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_opposite_new(s->base1, NULL, &s->category);
    if (st != CAT89_OK)
    {
        return st;
    }
    s->obj = s->base1_obj;
    st = cat89_opposite_mor(s->category, s->base1_id, &s->mor);
    return st;
}

static cat89_status slot_comma(struct cat89_bf_slot *s)
{
    const cat89_obj *cobj;
    cat89_status st;

    st = terminal_build(&s->base1, &s->base_eq1, &s->base_enum1, &s->base1_obj,
                        &s->base1_id);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_functor_identity(s->base1, NULL, &s->fun1);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_functor_identity(s->base1, NULL, &s->fun2);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_comma_category_new(s->fun1, s->fun2, s->base_eq1, NULL,
                                  &s->category);
    if (st != CAT89_OK)
    {
        return st;
    }
    cobj = NULL;
    st = cat89_comma_obj_new(s->category, s->base1_obj, s->base1_id,
                             s->base1_obj, &cobj);
    if (st != CAT89_OK)
    {
        return st;
    }
    s->obj = cobj;
    st = cat89_comma_mor_new(s->category, cobj, cobj, s->base1_id, s->base1_id,
                             &s->mor);
    return st;
}

static cat89_status slot_slice(struct cat89_bf_slot *s)
{
    const cat89_obj *sobj;
    cat89_status st;

    st = terminal_build(&s->base1, &s->base_eq1, &s->base_enum1, &s->base1_obj,
                        &s->base1_id);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_slice_category_new(s->base1, s->base_eq1, s->base1_obj, NULL,
                                  &s->category);
    if (st != CAT89_OK)
    {
        return st;
    }
    sobj = NULL;
    st = cat89_slice_obj_new(s->category, s->base1_id, &sobj);
    if (st != CAT89_OK)
    {
        return st;
    }
    s->obj = sobj;
    st = cat89_slice_mor_new(s->category, sobj, sobj, s->base1_id, &s->mor);
    return st;
}

static cat89_status slot_coslice(struct cat89_bf_slot *s)
{
    const cat89_obj *cobj;
    cat89_status st;

    st = terminal_build(&s->base1, &s->base_eq1, &s->base_enum1, &s->base1_obj,
                        &s->base1_id);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_coslice_category_new(s->base1, s->base_eq1, s->base1_obj, NULL,
                                    &s->category);
    if (st != CAT89_OK)
    {
        return st;
    }
    cobj = NULL;
    st = cat89_coslice_obj_new(s->category, s->base1_id, &cobj);
    if (st != CAT89_OK)
    {
        return st;
    }
    s->obj = cobj;
    st = cat89_coslice_mor_new(s->category, cobj, cobj, s->base1_id, &s->mor);
    return st;
}

static cat89_status slot_finite(struct cat89_bf_slot *s)
{
    return pair_build(&s->category, &s->base_eq1, &s->base_enum1, &s->obj,
                      &s->mor);
}

cat89_status cat89_backend_set_init(struct cat89_backend_set *set)
{
    unsigned long i;
    unsigned long inst;
    enum cat89_bf_kind kind;
    cat89_status st;

    if (set == NULL)
    {
        return CAT89_INVALID;
    }
    set->n = 0;
    for (i = 0; i < CAT89_BF_MAX; i = i + 1)
    {
        set->slots[i].kind = CAT89_BF_FINITE;
        set->slots[i].category = NULL;
        set->slots[i].obj = NULL;
        set->slots[i].mor = NULL;
        set->slots[i].base1 = NULL;
        set->slots[i].base2 = NULL;
        set->slots[i].base_eq1 = NULL;
        set->slots[i].base_eq2 = NULL;
        set->slots[i].base_enum1 = NULL;
        set->slots[i].base_enum2 = NULL;
        set->slots[i].fun1 = NULL;
        set->slots[i].fun2 = NULL;
        set->slots[i].base1_obj = NULL;
        set->slots[i].base2_obj = NULL;
        set->slots[i].base1_id = NULL;
        set->slots[i].base2_id = NULL;
    }

    for (kind = CAT89_BF_FINITE; kind < CAT89_BF_KIND_COUNT;
         kind = (enum cat89_bf_kind)(kind + 1))
    {
        for (inst = 0; inst < 2; inst = inst + 1)
        {
            struct cat89_bf_slot *s = &set->slots[set->n];
            s->kind = kind;
            if (kind == CAT89_BF_FINITE)
            {
                st = slot_finite(s);
            }
            else if (kind == CAT89_BF_FREE)
            {
                st = slot_free(s);
            }
            else if (kind == CAT89_BF_THIN)
            {
                st = slot_thin(s);
            }
            else if (kind == CAT89_BF_PRODUCT)
            {
                st = slot_product(s);
            }
            else if (kind == CAT89_BF_OPPOSITE)
            {
                st = slot_opposite(s);
            }
            else if (kind == CAT89_BF_COMMA)
            {
                st = slot_comma(s);
            }
            else if (kind == CAT89_BF_SLICE)
            {
                st = slot_slice(s);
            }
            else
            {
                st = slot_coslice(s);
            }
            if (st != CAT89_OK)
            {
                return st;
            }
            set->n = set->n + 1;
        }
    }
    return CAT89_OK;
}

static void slot_release(struct cat89_bf_slot *s)
{
    if (s->mor != NULL)
    {
        cat89_mor_release(s->category, s->mor);
        s->mor = NULL;
    }
    if (s->category != NULL)
    {
        cat89_category_release(s->category);
        s->category = NULL;
    }
    if (s->fun1 != NULL)
    {
        cat89_functor_release(s->fun1);
        s->fun1 = NULL;
    }
    if (s->fun2 != NULL)
    {
        cat89_functor_release(s->fun2);
        s->fun2 = NULL;
    }
    if (s->base_eq1 != NULL)
    {
        cat89_eq_release(s->base_eq1);
        s->base_eq1 = NULL;
    }
    if (s->base_eq2 != NULL)
    {
        cat89_eq_release(s->base_eq2);
        s->base_eq2 = NULL;
    }
    if (s->base_enum1 != NULL)
    {
        cat89_enum_release(s->base_enum1);
        s->base_enum1 = NULL;
    }
    if (s->base_enum2 != NULL)
    {
        cat89_enum_release(s->base_enum2);
        s->base_enum2 = NULL;
    }
    if (s->base1_id != NULL)
    {
        cat89_mor_release(s->base1, s->base1_id);
        s->base1_id = NULL;
    }
    if (s->base2_id != NULL)
    {
        cat89_mor_release(s->base2, s->base2_id);
        s->base2_id = NULL;
    }
    if (s->base1 != NULL)
    {
        cat89_category_release(s->base1);
        s->base1 = NULL;
    }
    if (s->base2 != NULL)
    {
        cat89_category_release(s->base2);
        s->base2 = NULL;
    }
}

void cat89_backend_set_dispose(struct cat89_backend_set *set)
{
    unsigned long i;

    if (set == NULL)
    {
        return;
    }
    for (i = 0; i < set->n; i = i + 1)
    {
        slot_release(&set->slots[i]);
    }
    set->n = 0;
}
