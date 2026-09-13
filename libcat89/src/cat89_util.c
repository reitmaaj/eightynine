/* cat89_util.c - reference-count and checked-arithmetic helpers. */

#include "cat89_internal.h"
#include <cat89/alloc.h>

static unsigned long drop_one(unsigned long *refs)
{
    if (*refs == 0)
    {
        return 0;
    }
    *refs = *refs - 1;
    return *refs;
}

static size_t max_factor(size_t a)
{
    if (a == 0)
    {
        return (size_t)-1;
    }
    return (size_t)-1 / a;
}

static int mul_would_overflow(size_t a, size_t b)
{
    size_t limit;

    limit = max_factor(a);
    return b > limit;
}

cat89_status cat89_ref_inc(unsigned long *refs)
{
    if (*refs == ULONG_MAX)
    {
        return CAT89_INVALID;
    }
    *refs = *refs + 1;
    return CAT89_OK;
}

int cat89_ref_dec(unsigned long *refs)
{
    unsigned long after;

    after = drop_one(refs);
    return after == 0;
}

cat89_status cat89_size_add(size_t a, size_t b, size_t *out)
{
    if (a > (size_t)-1 - b)
    {
        return CAT89_NOMEM;
    }
    *out = a + b;
    return CAT89_OK;
}

cat89_status cat89_size_mul(size_t a, size_t b, size_t *out)
{
    int overflow;

    overflow = mul_would_overflow(a, b);
    if (overflow)
    {
        return CAT89_NOMEM;
    }
    *out = a * b;
    return CAT89_OK;
}

void cat89_vec_init(cat89_vec *vec, size_t elem)
{
    vec->data = NULL;
    vec->len = 0;
    vec->cap = 0;
    vec->elem = elem;
}

void *cat89_vec_at(const cat89_vec *vec, size_t i)
{
    return (void *)((unsigned char *)vec->data + i * vec->elem);
}

static cat89_status vec_new_cap(const cat89_vec *vec, size_t *out)
{
    if (vec->cap == 0)
    {
        *out = 4;
        return CAT89_OK;
    }
    return cat89_size_add(vec->cap, vec->cap, out);
}

static cat89_status vec_grow(cat89_vec *vec, const cat89_allocator *allocator)
{
    size_t new_cap;
    size_t bytes;
    void *data;
    cat89_status st;

    st = vec_new_cap(vec, &new_cap);
    if (st != CAT89_OK)
    {
        return st;
    }
    st = cat89_size_mul(new_cap, vec->elem, &bytes);
    if (st != CAT89_OK)
    {
        return st;
    }
    data = cat89_alloc(allocator, bytes);
    if (data == NULL)
    {
        return CAT89_NOMEM;
    }
    if (vec->len > 0)
    {
        memcpy(data, vec->data, vec->len * vec->elem);
    }
    cat89_free(allocator, vec->data);
    vec->data = data;
    vec->cap = new_cap;
    return CAT89_OK;
}

cat89_status cat89_vec_push(cat89_vec *vec, const cat89_allocator *allocator,
                            const void *elem)
{
    cat89_status st;

    if (vec->len == vec->cap)
    {
        st = vec_grow(vec, allocator);
        if (st != CAT89_OK)
        {
            return st;
        }
    }
    memcpy((unsigned char *)vec->data + vec->len * vec->elem, elem, vec->elem);
    vec->len = vec->len + 1;
    return CAT89_OK;
}

void cat89_vec_free(cat89_vec *vec, const cat89_allocator *allocator)
{
    if (vec->data != NULL)
    {
        cat89_free(allocator, vec->data);
    }
    vec->data = NULL;
    vec->len = 0;
    vec->cap = 0;
}
