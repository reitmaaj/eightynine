/* cat89_test_allocfail.c - allocator-failure campaign (error atomicity).
 *
 * A fault-injecting allocator fails a single chosen allocation (the Nth call),
 * then forwards to malloc/free. For each public constructor path we sweep that
 * failure point across every allocation the path can make and assert
 * error-atomicity: on failure the produced handle is NULL (no partial object
 * escapes) and the call returns an error status. The suite also runs under
 * ASan+LSan, so any partial-failure leak is reported. */
#include <stdlib.h>

#include <cat89/cat89.h>
#include <test.h>

int cat89_test_failures = 0;

struct faila
{
    unsigned long count;
    unsigned long fail_at;
};

static void *fa_alloc(void *ctx, size_t size)
{
    struct faila *f = ctx;
    f->count = f->count + 1;
    if (f->count == f->fail_at)
    {
        return NULL;
    }
    return malloc(size);
}

static void *fa_realloc(void *ctx, void *ptr, size_t size)
{
    struct faila *f = ctx;
    f->count = f->count + 1;
    if (f->count == f->fail_at)
    {
        return NULL;
    }
    return realloc(ptr, size);
}

static void fa_free(void *ctx, void *ptr)
{
    (void)ctx;
    free(ptr);
}

/* Build a finite terminal category through the faulting allocator. On success
 * the caller owns nothing (everything released). Returns the status. */
static cat89_status build_terminal_alloc(const cat89_allocator *alloc)
{
    cat89_finite_builder *b;
    cat89_category *cat;
    cat89_eq *eq;
    cat89_enum *enumeration;
    cat89_finite_obj_id o;
    cat89_finite_mor_id id;
    cat89_status st;

    b = NULL;
    cat = NULL;
    eq = NULL;
    enumeration = NULL;
    o = 0;
    id = 0;
    st = cat89_finite_builder_new(alloc, &b);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_finite_add_object(b, &o);
    if (st != CAT89_OK)
    {
        cat89_finite_builder_release(b);
        return st;
    }
    st = cat89_finite_add_morphism(b, o, o, &id);
    if (st != CAT89_OK)
    {
        cat89_finite_builder_release(b);
        return st;
    }
    st = cat89_finite_set_identity(b, o, id);
    if (st != CAT89_OK)
    {
        cat89_finite_builder_release(b);
        return st;
    }
    st = cat89_finite_set_composition(b, id, id, id);
    if (st != CAT89_OK)
    {
        cat89_finite_builder_release(b);
        return st;
    }
    st = cat89_finite_build(b, &cat, &eq, &enumeration);
    cat89_finite_builder_release(b);
    if (st != CAT89_OK)
    {
        return st;
    }
    cat89_enum_release(enumeration);
    cat89_eq_release(eq);
    cat89_category_release(cat);
    return CAT89_OK;
}

static void test_finite_builder_atomic(void)
{
    unsigned long at;
    unsigned long ok_count;
    unsigned long fail_count;
    struct faila f;
    cat89_allocator alloc;
    cat89_status st;

    ok_count = 0;
    fail_count = 0;
    /* sweep well past the number of allocations the path can make */
    for (at = 1; at <= 400; at = at + 1)
    {
        f.count = 0;
        f.fail_at = at;
        alloc.ctx = &f;
        alloc.alloc = fa_alloc;
        alloc.realloc = fa_realloc;
        alloc.free = fa_free;
        st = build_terminal_alloc(&alloc);
        if (st == CAT89_OK)
        {
            ok_count = ok_count + 1;
        }
        else
        {
            T_ASSERT(st == CAT89_NOMEM);
            fail_count = fail_count + 1;
        }
    }
    /* the path both fails at early points and succeeds once past them */
    T_ASSERT(fail_count > 0);
    T_ASSERT(ok_count > 0);
}

static void test_shape_build_atomic(void)
{
    unsigned long at;
    unsigned long ok_count;
    unsigned long fail_count;
    struct faila f;
    cat89_allocator alloc;
    cat89_status st;

    ok_count = 0;
    fail_count = 0;
    for (at = 1; at <= 600; at = at + 1)
    {
        cat89_category *cat;
        cat89_eq *eq;
        cat89_enum *enumeration;

        cat = NULL;
        eq = NULL;
        enumeration = NULL;
        f.count = 0;
        f.fail_at = at;
        alloc.ctx = &f;
        alloc.alloc = fa_alloc;
        alloc.realloc = fa_realloc;
        alloc.free = fa_free;
        st = cat89_shape_category_new(CAT89_SHAPE_DISCRETE2, &alloc, &cat, &eq,
                                      &enumeration);
        if (st == CAT89_OK)
        {
            T_ASSERT(cat != NULL);
            cat89_enum_release(enumeration);
            cat89_eq_release(eq);
            cat89_category_release(cat);
            ok_count = ok_count + 1;
        }
        else
        {
            T_ASSERT(cat == NULL);
            fail_count = fail_count + 1;
        }
    }
    T_ASSERT(fail_count > 0);
    T_ASSERT(ok_count > 0);
}

/* ------------------------------------- FA01-FA05: builder atomicity */

static void test_fa01_add_object_atomic(void)
{
    struct faila f;
    cat89_allocator alloc;
    cat89_finite_builder *b;
    cat89_finite_obj_id id;
    unsigned long at;
    cat89_status st;

    for (at = 1; at <= 20; at = at + 1)
    {
        f.count = 0;
        f.fail_at = 0;
        alloc.ctx = &f;
        alloc.alloc = fa_alloc;
        alloc.realloc = fa_realloc;
        alloc.free = fa_free;
        b = NULL;
        T_STATUS(cat89_finite_builder_new(&alloc, &b), CAT89_OK);
        id = 99;
        T_STATUS(cat89_finite_add_object(b, &id), CAT89_OK);
        T_EQ_UL(id, 0);

        f.count = 0;
        f.fail_at = at;
        id = 99;
        st = cat89_finite_add_object(b, &id);
        if (st == CAT89_NOMEM)
        {
            T_EQ_UL(id, 0);
            f.fail_at = 0;
            T_STATUS(cat89_finite_add_object(b, &id), CAT89_OK);
            T_EQ_UL(id, 1);
        }
        else
        {
            T_STATUS(st, CAT89_OK);
            T_EQ_UL(id, 1);
        }
        cat89_finite_builder_release(b);
    }
}

static void test_fa02_add_morphism_atomic(void)
{
    struct faila f;
    cat89_allocator alloc;
    cat89_finite_builder *b;
    cat89_finite_obj_id a;
    cat89_finite_obj_id bb;
    cat89_finite_mor_id id;
    unsigned long at;
    cat89_status st;

    for (at = 1; at <= 20; at = at + 1)
    {
        f.count = 0;
        f.fail_at = 0;
        alloc.ctx = &f;
        alloc.alloc = fa_alloc;
        alloc.realloc = fa_realloc;
        alloc.free = fa_free;
        b = NULL;
        T_STATUS(cat89_finite_builder_new(&alloc, &b), CAT89_OK);
        a = 0;
        bb = 0;
        id = 0;
        cat89_finite_add_object(b, &a);
        cat89_finite_add_object(b, &bb);
        cat89_finite_add_morphism(b, a, a, &id);
        cat89_finite_add_morphism(b, bb, bb, &id);
        T_EQ_UL(id, 1);

        f.count = 0;
        f.fail_at = at;
        id = 99;
        st = cat89_finite_add_morphism(b, a, bb, &id);
        if (st == CAT89_NOMEM)
        {
            T_EQ_UL(id, 0);
            f.fail_at = 0;
            T_STATUS(cat89_finite_add_morphism(b, a, bb, &id), CAT89_OK);
            T_EQ_UL(id, 2);
        }
        else
        {
            T_STATUS(st, CAT89_OK);
            T_EQ_UL(id, 2);
        }
        cat89_finite_builder_release(b);
    }
}

/* A -> B -> C with h = g o f. Pre-table holds four composition rows, so the
 * next append grows the composition vector (cap == len == 4). */
static void partial_chain(cat89_finite_builder *b, cat89_finite_obj_id *a,
                          cat89_finite_obj_id *bb, cat89_finite_obj_id *c,
                          cat89_finite_mor_id *ia, cat89_finite_mor_id *ib,
                          cat89_finite_mor_id *ic, cat89_finite_mor_id *f,
                          cat89_finite_mor_id *g, cat89_finite_mor_id *h)
{
    cat89_finite_add_object(b, a);
    cat89_finite_add_object(b, bb);
    cat89_finite_add_object(b, c);
    cat89_finite_add_morphism(b, *a, *a, ia);
    cat89_finite_add_morphism(b, *bb, *bb, ib);
    cat89_finite_add_morphism(b, *c, *c, ic);
    cat89_finite_add_morphism(b, *a, *bb, f);
    cat89_finite_add_morphism(b, *bb, *c, g);
    cat89_finite_add_morphism(b, *a, *c, h);
    cat89_finite_set_identity(b, *a, *ia);
    cat89_finite_set_identity(b, *bb, *ib);
    cat89_finite_set_identity(b, *c, *ic);
    cat89_finite_set_composition(b, *ia, *ia, *ia);
    cat89_finite_set_composition(b, *ib, *ib, *ib);
    cat89_finite_set_composition(b, *ic, *ic, *ic);
    cat89_finite_set_composition(b, *ib, *f, *f);
}

static void finish_chain(cat89_finite_builder *b, cat89_finite_mor_id ia,
                         cat89_finite_mor_id ib, cat89_finite_mor_id ic,
                         cat89_finite_mor_id f, cat89_finite_mor_id g,
                         cat89_finite_mor_id h)
{
    (void)ia;
    (void)ib;
    (void)ic;
    (void)f;
    cat89_finite_set_composition(b, ic, g, g);
    cat89_finite_set_composition(b, g, ib, g);
    cat89_finite_set_composition(b, ic, h, h);
    cat89_finite_set_composition(b, h, ia, h);
    cat89_finite_set_composition(b, g, f, h);
}

static void test_fa03_composition_atomic(void)
{
    struct faila f;
    cat89_allocator alloc;
    cat89_finite_builder *b;
    cat89_finite_obj_id a;
    cat89_finite_obj_id bb;
    cat89_finite_obj_id c;
    cat89_finite_mor_id ia;
    cat89_finite_mor_id ib;
    cat89_finite_mor_id ic;
    cat89_finite_mor_id fr;
    cat89_finite_mor_id g;
    cat89_finite_mor_id h;
    unsigned long at;
    cat89_status st;
    int valid;

    for (at = 1; at <= 20; at = at + 1)
    {
        f.count = 0;
        f.fail_at = 0;
        alloc.ctx = &f;
        alloc.alloc = fa_alloc;
        alloc.realloc = fa_realloc;
        alloc.free = fa_free;
        b = NULL;
        T_STATUS(cat89_finite_builder_new(&alloc, &b), CAT89_OK);
        partial_chain(b, &a, &bb, &c, &ia, &ib, &ic, &fr, &g, &h);

        f.count = 0;
        f.fail_at = at;
        st = cat89_finite_set_composition(b, fr, ia, fr);
        if (st == CAT89_NOMEM)
        {
            f.fail_at = 0;
            T_STATUS(cat89_finite_set_composition(b, fr, ia, fr), CAT89_OK);
        }
        else
        {
            T_STATUS(st, CAT89_OK);
        }
        f.fail_at = 0;
        finish_chain(b, ia, ib, ic, fr, g, h);
        valid = -1;
        T_STATUS(cat89_finite_validate(b, &valid), CAT89_OK);
        T_EQ_UL(valid, 1);
        cat89_finite_builder_release(b);
    }
}

static void test_fa04_repeated_failure(void)
{
    struct faila f;
    cat89_allocator alloc;
    cat89_finite_builder *b;
    cat89_finite_obj_id a;
    cat89_finite_obj_id bb;
    cat89_finite_obj_id c;
    cat89_finite_mor_id ia;
    cat89_finite_mor_id ib;
    cat89_finite_mor_id ic;
    cat89_finite_mor_id fr;
    cat89_finite_mor_id g;
    cat89_finite_mor_id h;
    cat89_status st;
    int valid;

    f.count = 0;
    f.fail_at = 0;
    alloc.ctx = &f;
    alloc.alloc = fa_alloc;
    alloc.realloc = fa_realloc;
    alloc.free = fa_free;
    b = NULL;
    T_STATUS(cat89_finite_builder_new(&alloc, &b), CAT89_OK);
    partial_chain(b, &a, &bb, &c, &ia, &ib, &ic, &fr, &g, &h);

    f.fail_at = 1;
    f.count = 0;
    st = cat89_finite_set_composition(b, fr, ia, fr);
    T_STATUS(st, CAT89_NOMEM);
    f.fail_at = 1;
    f.count = 0;
    st = cat89_finite_set_composition(b, fr, ia, fr);
    T_STATUS(st, CAT89_NOMEM);
    f.fail_at = 0;
    T_STATUS(cat89_finite_set_composition(b, fr, ia, fr), CAT89_OK);
    finish_chain(b, ia, ib, ic, fr, g, h);
    valid = -1;
    T_STATUS(cat89_finite_validate(b, &valid), CAT89_OK);
    T_EQ_UL(valid, 1);
    cat89_finite_builder_release(b);
}

static void test_fa05_existing_rows_intact(void)
{
    struct faila f;
    cat89_allocator alloc;
    cat89_finite_builder *b;
    cat89_finite_obj_id a;
    cat89_finite_obj_id bb;
    cat89_finite_obj_id c;
    cat89_finite_mor_id ia;
    cat89_finite_mor_id ib;
    cat89_finite_mor_id ic;
    cat89_finite_mor_id fr;
    cat89_finite_mor_id g;
    cat89_finite_mor_id h;

    int valid;
    cat89_category *cat = NULL;

    f.count = 0;
    f.fail_at = 0;
    alloc.ctx = &f;
    alloc.alloc = fa_alloc;
    alloc.realloc = fa_realloc;
    alloc.free = fa_free;
    b = NULL;
    T_STATUS(cat89_finite_builder_new(&alloc, &b), CAT89_OK);
    partial_chain(b, &a, &bb, &c, &ia, &ib, &ic, &fr, &g, &h);
    f.fail_at = 1;
    f.count = 0;
    T_STATUS(cat89_finite_set_composition(b, fr, ia, fr), CAT89_NOMEM);
    valid = -1;
    T_STATUS(cat89_finite_validate(b, &valid), CAT89_OK);
    T_EQ_UL(valid, 0);
    f.fail_at = 0;
    T_STATUS(cat89_finite_set_composition(b, fr, ia, fr), CAT89_OK);
    finish_chain(b, ia, ib, ic, fr, g, h);
    T_STATUS(cat89_finite_validate(b, &valid), CAT89_OK);
    T_EQ_UL(valid, 1);
    T_STATUS(cat89_finite_build(b, &cat, NULL, NULL), CAT89_OK);
    cat89_category_release(cat);
    cat89_finite_builder_release(b);
}

int main(void)
{
    T_START();
    test_finite_builder_atomic();
    test_shape_build_atomic();
    test_fa01_add_object_atomic();
    test_fa02_add_morphism_atomic();
    test_fa03_composition_atomic();
    test_fa04_repeated_failure();
    test_fa05_existing_rows_intact();
    return T_END() ? 0 : 1;
}
