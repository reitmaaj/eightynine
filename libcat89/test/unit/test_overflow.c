/* cat89_test_overflow.c - checked arithmetic and refcount overflow (AR01-AR06).
 *
 * White-box: uses the internal checked-size helpers, the internal vector and
 * the test-only refcount hooks declared in cat89_internal.h. No public API is
 * added for corruption. */
#include <cat89/cat89.h>
#include <cat89_internal.h>
#include <cat89_mock_backend.h>
#include <test.h>

int cat89_test_failures = 0;

struct counting_alloc
{
    unsigned long calls;
};

static void *ca_alloc(void *ctx, size_t size)
{
    struct counting_alloc *c = ctx;
    (void)size;
    c->calls = c->calls + 1;
    return NULL;
}

static void *ca_realloc(void *ctx, void *ptr, size_t size)
{
    struct counting_alloc *c = ctx;
    (void)ptr;
    (void)size;
    c->calls = c->calls + 1;
    return NULL;
}

static void ca_free(void *ctx, void *ptr)
{
    (void)ctx;
    (void)ptr;
}

static void test_ar01_size_add_overflow(void)
{
    size_t out;
    cat89_status st;

    out = 123;
    st = cat89_size_add((size_t)-1, 1, &out);
    T_ASSERT(st != CAT89_OK);
    T_EQ_UL(out, 123);
}

static void test_ar02_size_mul_overflow(void)
{
    size_t out;
    cat89_status st;

    out = 123;
    st = cat89_size_mul((size_t)-1, 2, &out);
    T_ASSERT(st != CAT89_OK);
    T_EQ_UL(out, 123);
    out = 123;
    st = cat89_size_mul(((size_t)-1) / 2 + 1, 2, &out);
    T_ASSERT(st != CAT89_OK);
    T_EQ_UL(out, 123);
}

static void test_ar03_vec_capacity_overflow(void)
{
    cat89_vec v;
    struct counting_alloc c;
    cat89_allocator a;
    unsigned long byte = 7;
    void *data_before;
    size_t len_before;
    size_t cap_before;
    cat89_status st;

    c.calls = 0;
    a.ctx = &c;
    a.alloc = ca_alloc;
    a.realloc = ca_realloc;
    a.free = ca_free;

    cat89_vec_init(&v, 1);
    v.data = &byte;
    v.len = ((size_t)-1) / 2 + 1;
    v.cap = v.len;
    data_before = v.data;
    len_before = v.len;
    cap_before = v.cap;
    st = cat89_vec_push(&v, &a, &byte);
    T_ASSERT(st != CAT89_OK);
    T_ASSERT(v.data == data_before);
    T_EQ_UL(v.len, len_before);
    T_EQ_UL(v.cap, cap_before);
    T_EQ_UL(c.calls, 0);
}

static void test_ar04_vec_element_overflow(void)
{
    cat89_vec v;
    struct counting_alloc c;
    cat89_allocator a;
    unsigned long byte = 7;
    cat89_status st;

    c.calls = 0;
    a.ctx = &c;
    a.alloc = ca_alloc;
    a.realloc = ca_realloc;
    a.free = ca_free;

    cat89_vec_init(&v, ((size_t)-1) / 2 + 1);
    v.data = &byte;
    v.len = 2;
    v.cap = 2;
    st = cat89_vec_push(&v, &a, &byte);
    T_ASSERT(st != CAT89_OK);
    T_EQ_UL(c.calls, 0);
}

static void test_ar05_refcount_overflow(void)
{
    unsigned long refs;
    cat89_category *cat;
    cat89_mock *mock;
    cat89_status st;

    refs = (unsigned long)-1;
    st = cat89_ref_inc(&refs);
    T_STATUS(st, CAT89_INVALID);
    T_EQ_UL(refs, (unsigned long)-1);

    cat = NULL;
    mock = NULL;
    T_STATUS(cat89_mock_new(NULL, &cat, &mock), CAT89_OK);
    cat89_category_test_set_refs(cat, (unsigned long)-1);
    st = cat89_category_retain(cat);
    T_STATUS(st, CAT89_INVALID);
    cat89_category_test_set_refs(cat, 1);
    cat89_category_release(cat);
    cat89_mock_free(mock);
}

static void test_ar06_backend_refcount_overflow(void)
{
    cat89_finite_builder *b;
    cat89_finite_obj_id o;
    cat89_finite_mor_id id;
    cat89_category *cat;
    cat89_enum *en;
    cat89_mor *mor;
    cat89_obj_iter *it;
    const cat89_obj *obj;
    int done;
    cat89_status st;

    b = NULL;
    cat = NULL;
    en = NULL;
    mor = NULL;
    T_STATUS(cat89_finite_builder_new(NULL, &b), CAT89_OK);
    o = 0;
    id = 0;
    cat89_finite_add_object(b, &o);
    cat89_finite_add_morphism(b, o, o, &id);
    cat89_finite_set_identity(b, o, id);
    cat89_finite_set_composition(b, id, id, id);
    T_STATUS(cat89_finite_build(b, &cat, NULL, &en), CAT89_OK);
    cat89_finite_builder_release(b);
    it = NULL;
    obj = NULL;
    done = 0;
    T_STATUS(cat89_obj_iter_open(en, &it), CAT89_OK);
    T_STATUS(cat89_obj_iter_next(en, it, &obj, &done), CAT89_OK);
    cat89_obj_iter_close(en, it);
    T_ASSERT(obj != NULL);
    T_STATUS(cat89_identity(cat, obj, &mor), CAT89_OK);
    T_ASSERT(mor != NULL);
    cat89_finite_test_set_mor_refs(mor, (unsigned long)-1);
    st = cat89_mor_retain(cat, mor);
    T_STATUS(st, CAT89_INVALID);
    cat89_finite_test_set_mor_refs(mor, 1);
    cat89_mor_release(cat, mor);
    cat89_enum_release(en);
    cat89_category_release(cat);
}

int main(void)
{
    T_START();
    test_ar01_size_add_overflow();
    test_ar02_size_mul_overflow();
    test_ar03_vec_capacity_overflow();
    test_ar04_vec_element_overflow();
    test_ar05_refcount_overflow();
    test_ar06_backend_refcount_overflow();
    return T_END() ? 0 : 1;
}
