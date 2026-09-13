/* cat89_arrow.c - the arrow category C^-> as comma(1_C, 1_C). */

#include "cat89_internal.h"
#include <cat89/arrow.h>
#include <cat89/comma.h>
#include <cat89/functor.h>

cat89_status cat89_arrow_category_new(cat89_category *base, cat89_eq *eq,
                                      const cat89_allocator *allocator,
                                      cat89_category **out_category)
{
    cat89_functor *fid;
    cat89_functor *gid;
    cat89_category *arrow;
    cat89_status st;
    cat89_status s2;

    if (out_category == NULL)
    {
        return CAT89_INVALID;
    }
    *out_category = NULL;
    if (base == NULL)
    {
        return CAT89_INVALID;
    }
    if (eq == NULL)
    {
        return CAT89_INVALID;
    }

    fid = NULL;
    gid = NULL;
    arrow = NULL;
    st = cat89_functor_identity(base, allocator, &fid);
    if (st != CAT89_OK)
    {
        return st;
    }
    s2 = cat89_functor_identity(base, allocator, &gid);
    if (s2 != CAT89_OK)
    {
        cat89_functor_release(fid);
        return s2;
    }
    st = cat89_comma_category_new(fid, gid, eq, allocator, &arrow);
    cat89_functor_release(fid);
    cat89_functor_release(gid);
    if (st != CAT89_OK)
    {
        return st;
    }
    *out_category = arrow;
    return CAT89_OK;
}

cat89_status cat89_arrow_obj_new(cat89_category *arrow, const cat89_obj *dom,
                                 cat89_mor *f, const cat89_obj *cod,
                                 const cat89_obj **out_obj)
{
    cat89_status st;

    st = cat89_comma_obj_new(arrow, dom, f, cod, out_obj);
    return st;
}

cat89_status cat89_arrow_mor_new(cat89_category *arrow,
                                 const cat89_obj *dom_obj,
                                 const cat89_obj *cod_obj, cat89_mor *u,
                                 cat89_mor *v, cat89_mor **out_mor)
{
    cat89_status st;

    st = cat89_comma_mor_new(arrow, dom_obj, cod_obj, u, v, out_mor);
    return st;
}
