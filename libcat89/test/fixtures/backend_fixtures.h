#ifndef CAT89_BACKEND_FIXTURES_H
#define CAT89_BACKEND_FIXTURES_H

/* cat89_backend_fixtures.h - one usable fixture per built-in backend.
 *
 * Each slot exposes a live category, a borrowed object and an owned morphism.
 * Derived backends retain their base categories until teardown. Two independent
 * instances of each backend kind are built by cat89_backend_set_init. */

#include <cat89/cat89.h>

enum cat89_bf_kind
{
    CAT89_BF_FINITE = 0,
    CAT89_BF_FREE,
    CAT89_BF_THIN,
    CAT89_BF_PRODUCT,
    CAT89_BF_OPPOSITE,
    CAT89_BF_COMMA,
    CAT89_BF_SLICE,
    CAT89_BF_COSLICE,
    CAT89_BF_KIND_COUNT
};

#define CAT89_BF_MAX 16

struct cat89_bf_slot
{
    enum cat89_bf_kind kind;
    cat89_category *category;
    const cat89_obj *obj;
    cat89_mor *mor;

    cat89_category *base1;
    cat89_category *base2;
    cat89_eq *base_eq1;
    cat89_eq *base_eq2;
    cat89_enum *base_enum1;
    cat89_enum *base_enum2;
    cat89_functor *fun1;
    cat89_functor *fun2;
    const cat89_obj *base1_obj;
    const cat89_obj *base2_obj;
    cat89_mor *base1_id;
    cat89_mor *base2_id;
};

struct cat89_backend_set
{
    struct cat89_bf_slot slots[CAT89_BF_MAX];
    unsigned long n;
};

cat89_status cat89_backend_set_init(struct cat89_backend_set *set);

void cat89_backend_set_dispose(struct cat89_backend_set *set);

#endif
