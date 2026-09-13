/* cat89_find.c - finite universal-object finders. */

#include "cat89_internal.h"
#include <cat89/find.h>

/* Collect all borrowed objects of the enumeration into a heap array. One
 * iterator opening only; the array grows with checked arithmetic. */
static cat89_status collect_objects_step(cat89_enum *en, cat89_obj_iter *it,
                                         cat89_vec *vec,
                                         const cat89_allocator *alloc,
                                         int *out_done)
{
    const cat89_obj *obj;
    cat89_status st;

    obj = NULL;
    st = cat89_obj_iter_next(en, it, &obj, out_done);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (*out_done)
    {
        return CAT89_OK;
    }
    st = cat89_vec_push(vec, alloc, &obj);
    return st;
}

static void collect_objects_abort(cat89_enum *en, cat89_obj_iter *it,
                                  cat89_vec *vec, const cat89_allocator *alloc)
{
    cat89_obj_iter_close(en, it);
    cat89_vec_free(vec, alloc);
}

static cat89_status collect_objects(cat89_enum *en, const cat89_obj ***out,
                                    unsigned long *out_n)
{
    const cat89_allocator *alloc;
    cat89_obj_iter *it;
    cat89_vec vec;
    int done;
    cat89_status st;

    alloc = cat89_allocator_default();
    *out = NULL;
    *out_n = 0;
    cat89_vec_init(&vec, sizeof(const cat89_obj *));
    it = NULL;
    st = cat89_obj_iter_open(en, &it);
    if (st != CAT89_OK)
    {
        return st;
    }
    done = 0;
    while (!done)
    {
        st = collect_objects_step(en, it, &vec, alloc, &done);
        if (st != CAT89_OK)
        {
            collect_objects_abort(en, it, &vec, alloc);
            return st;
        }
    }
    cat89_obj_iter_close(en, it);
    *out = (const cat89_obj **)vec.data;
    *out_n = vec.len;
    return CAT89_OK;
}

static void objects_free(const cat89_obj **arr)
{
    const cat89_allocator *alloc;

    alloc = cat89_allocator_default();
    if (arr != NULL)
    {
        cat89_free(alloc, arr);
    }
}

static void note_dupe(cat89_category *cat, cat89_mor *mor, int *out_dupe)
{
    ++*out_dupe;
    cat89_mor_release(cat, mor);
}

static cat89_status one_mor_step(cat89_category *cat, cat89_enum *en,
                                 cat89_mor_iter *it, const cat89_obj *a,
                                 const cat89_obj *b, cat89_mor **found,
                                 int *out_dupe, int *out_done)
{
    cat89_status st;
    cat89_mor *mor;
    const cat89_obj *dm;
    const cat89_obj *cm;

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
    if (st != CAT89_OK)
    {
        cat89_mor_release(cat, mor);
        return st;
    }
    st = cat89_cod(cat, mor, &cm);
    if (st != CAT89_OK)
    {
        cat89_mor_release(cat, mor);
        return st;
    }
    if (cat89_obj_same(cat, dm, a) != 0)
    {
        if (cat89_obj_same(cat, cm, b) != 0)
        {
            if (*found == NULL)
            {
                *found = mor;
                return CAT89_OK;
            }
            note_dupe(cat, mor, out_dupe);
            return CAT89_OK;
        }
    }
    cat89_mor_release(cat, mor);
    return CAT89_OK;
}

static void one_mor_abort(cat89_category *cat, cat89_enum *en,
                          cat89_mor_iter *it, cat89_mor *found)
{
    if (found != NULL)
    {
        cat89_mor_release(cat, found);
    }
    cat89_mor_iter_close(en, it);
}

/* Return the single morphism a -> b (owned) if unique; otherwise NULL. */
static cat89_status one_mor(cat89_category *cat, cat89_enum *en,
                            const cat89_obj *a, const cat89_obj *b,
                            cat89_mor **out, int *out_one)
{
    cat89_mor_iter *it;
    cat89_mor *found;
    int dupe;
    int done;
    cat89_status st;

    it = NULL;
    found = NULL;
    dupe = 0;
    *out_one = 0;
    st = cat89_mor_iter_open(en, &it);
    if (st != CAT89_OK)
    {
        return st;
    }
    done = 0;
    while (!done)
    {
        st = one_mor_step(cat, en, it, a, b, &found, &dupe, &done);
        if (st != CAT89_OK)
        {
            one_mor_abort(cat, en, it, found);
            return st;
        }
    }
    cat89_mor_iter_close(en, it);
    if (dupe > 0)
    {
        if (found != NULL)
        {
            cat89_mor_release(cat, found);
        }
        *out = NULL;
        return CAT89_OK;
    }
    if (found == NULL)
    {
        *out = NULL;
        return CAT89_OK;
    }
    *out = found;
    *out_one = 1;
    return CAT89_OK;
}

static cat89_status one_hom_ok(cat89_category *cat, cat89_enum *en,
                               const cat89_obj *a, const cat89_obj *b,
                               int *out_ok)
{
    cat89_mor *owned;
    int one;
    cat89_status st;

    owned = NULL;
    one = 0;
    st = one_mor(cat, en, a, b, &owned, &one);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (owned != NULL)
    {
        cat89_mor_release(cat, owned);
    }
    *out_ok = one;
    return CAT89_OK;
}

static cat89_status is_terminal(cat89_category *cat, cat89_enum *en,
                                const cat89_obj *cand, const cat89_obj **objs,
                                unsigned long no, int *out_ok)
{
    unsigned long i;
    int one;
    int ok;
    cat89_status st;

    ok = 1;
    for (i = 0; i < no; i = i + 1)
    {
        st = one_hom_ok(cat, en, objs[i], cand, &one);
        if (st != CAT89_OK)
        {
            return st;
        }
        if (one == 0)
        {
            ok = 0;
            break;
        }
    }
    *out_ok = ok;
    return CAT89_OK;
}

static cat89_status is_initial(cat89_category *cat, cat89_enum *en,
                               const cat89_obj *cand, const cat89_obj **objs,
                               unsigned long no, int *out_ok)
{
    unsigned long i;
    int one;
    int ok;
    cat89_status st;

    ok = 1;
    for (i = 0; i < no; i = i + 1)
    {
        st = one_hom_ok(cat, en, cand, objs[i], &one);
        if (st != CAT89_OK)
        {
            return st;
        }
        if (one == 0)
        {
            ok = 0;
            break;
        }
    }
    *out_ok = ok;
    return CAT89_OK;
}

static cat89_status terminal_scan(cat89_category *cat, cat89_enum *en,
                                  const cat89_obj **objs, unsigned long no,
                                  unsigned long *out_idx)
{
    unsigned long i;
    int ok;
    cat89_status st;

    for (i = 0; i < no; i = i + 1)
    {
        st = is_terminal(cat, en, objs[i], objs, no, &ok);
        if (st != CAT89_OK)
        {
            return st;
        }
        if (ok)
        {
            *out_idx = i;
            return CAT89_OK;
        }
    }
    *out_idx = no;
    return CAT89_OK;
}

static cat89_status initial_scan(cat89_category *cat, cat89_enum *en,
                                 const cat89_obj **objs, unsigned long no,
                                 unsigned long *out_idx)
{
    unsigned long i;
    int ok;
    cat89_status st;

    for (i = 0; i < no; i = i + 1)
    {
        st = is_initial(cat, en, objs[i], objs, no, &ok);
        if (st != CAT89_OK)
        {
            return st;
        }
        if (ok)
        {
            *out_idx = i;
            return CAT89_OK;
        }
    }
    *out_idx = no;
    return CAT89_OK;
}

cat89_status cat89_find_terminal(cat89_category *category,
                                 cat89_enum *enumeration,
                                 const cat89_obj **out_terminal)
{
    const cat89_obj **objs;
    const cat89_obj *res;
    unsigned long no;
    unsigned long idx;
    cat89_status st;

    if (out_terminal == NULL)
    {
        return CAT89_INVALID;
    }
    *out_terminal = NULL;
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (enumeration == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_enum_category(enumeration) != category)
    {
        return CAT89_INVALID;
    }
    objs = NULL;
    no = 0;
    st = collect_objects(enumeration, &objs, &no);
    if (st != CAT89_OK)
    {
        return st;
    }
    idx = no;
    st = terminal_scan(category, enumeration, objs, no, &idx);
    if (st != CAT89_OK)
    {
        objects_free(objs);
        return st;
    }
    if (idx >= no)
    {
        objects_free(objs);
        return CAT89_NOT_FOUND;
    }
    res = objs[idx];
    objects_free(objs);
    *out_terminal = res;
    return CAT89_OK;
}

cat89_status cat89_find_initial(cat89_category *category,
                                cat89_enum *enumeration,
                                const cat89_obj **out_initial)
{
    const cat89_obj **objs;
    const cat89_obj *res;
    unsigned long no;
    unsigned long idx;
    cat89_status st;

    if (out_initial == NULL)
    {
        return CAT89_INVALID;
    }
    *out_initial = NULL;
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (enumeration == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_enum_category(enumeration) != category)
    {
        return CAT89_INVALID;
    }
    objs = NULL;
    no = 0;
    st = collect_objects(enumeration, &objs, &no);
    if (st != CAT89_OK)
    {
        return st;
    }
    idx = no;
    st = initial_scan(category, enumeration, objs, no, &idx);
    if (st != CAT89_OK)
    {
        objects_free(objs);
        return st;
    }
    if (idx >= no)
    {
        objects_free(objs);
        return CAT89_NOT_FOUND;
    }
    res = objs[idx];
    objects_free(objs);
    *out_initial = res;
    return CAT89_OK;
}

cat89_status cat89_terminal_arrow(cat89_category *category,
                                  cat89_enum *enumeration,
                                  const cat89_obj *terminal, const cat89_obj *x,
                                  cat89_mor **out_mor)
{
    int one;
    cat89_status st;

    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (enumeration == NULL)
    {
        return CAT89_INVALID;
    }
    if (terminal == NULL)
    {
        return CAT89_INVALID;
    }
    if (x == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_enum_category(enumeration) != category)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_obj(category, terminal) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_obj(category, x) == 0)
    {
        return CAT89_INVALID;
    }
    one = 0;
    st = one_mor(category, enumeration, x, terminal, out_mor, &one);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (one == 0)
    {
        return CAT89_NOT_FOUND;
    }
    return CAT89_OK;
}

cat89_status cat89_initial_arrow(cat89_category *category,
                                 cat89_enum *enumeration,
                                 const cat89_obj *initial, const cat89_obj *x,
                                 cat89_mor **out_mor)
{
    int one;
    cat89_status st;

    if (out_mor == NULL)
    {
        return CAT89_INVALID;
    }
    *out_mor = NULL;
    if (category == NULL)
    {
        return CAT89_INVALID;
    }
    if (enumeration == NULL)
    {
        return CAT89_INVALID;
    }
    if (initial == NULL)
    {
        return CAT89_INVALID;
    }
    if (x == NULL)
    {
        return CAT89_INVALID;
    }
    if (cat89_enum_category(enumeration) != category)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_obj(category, initial) == 0)
    {
        return CAT89_INVALID;
    }
    if (cat89_owns_obj(category, x) == 0)
    {
        return CAT89_INVALID;
    }
    one = 0;
    st = one_mor(category, enumeration, initial, x, out_mor, &one);
    if (st != CAT89_OK)
    {
        return st;
    }
    if (one == 0)
    {
        return CAT89_NOT_FOUND;
    }
    return CAT89_OK;
}
