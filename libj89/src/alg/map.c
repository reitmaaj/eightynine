/* map.c - recursive functorial mapping over the N and S carriers for j89_alg.
 */
#include "alg_internal.h"

typedef struct j89a_map_ctx
{
    j89a_alloc *al;
    j89a_number_map_fn number_map;
    j89a_string_map_fn string_map;
    void *user;
} j89a_map_ctx;

static j89a_status map_value(void *c, const j89a_json *value, j89a_json **out);

static j89a_status map_null(void *c, j89a_json **out)
{
    j89a_map_ctx *mc;
    j89a_json *r;
    mc = (j89a_map_ctx *)c;
    r = j89a_json_null(mc->al);
    if (r == NULL)
    {
        *out = NULL;
        return J89A_NOMEM;
    }
    *out = r;
    return J89A_OK;
}

static j89a_status map_boolean(void *c, const j89a_json *value, j89a_json **out)
{
    j89a_map_ctx *mc;
    j89a_json *r;
    mc = (j89a_map_ctx *)c;
    r = j89a_json_boolean(value->u.b, mc->al);
    if (r == NULL)
    {
        *out = NULL;
        return J89A_NOMEM;
    }
    *out = r;
    return J89A_OK;
}

static j89a_status map_number(void *c, const j89a_json *value, j89a_json **out)
{
    j89a_map_ctx *mc;
    double mapped;
    j89a_json *r;
    mc = (j89a_map_ctx *)c;
    mapped = mc->number_map(mc->user, value->u.n);
    r = j89a_json_number(mapped, mc->al);
    if (r == NULL)
    {
        *out = NULL;
        return J89A_NOMEM;
    }
    *out = r;
    return J89A_OK;
}

static j89a_status map_string(void *c, const j89a_json *value, j89a_json **out)
{
    j89a_map_ctx *mc;
    j89a_str *ns;
    j89a_json *r;
    mc = (j89a_map_ctx *)c;
    ns = mc->string_map(mc->user, value->u.s);
    if (ns == NULL)
    {
        *out = NULL;
        return J89A_NOMEM;
    }
    r = j89a_json_string(ns, mc->al);
    j89a_str_release(ns);
    if (r == NULL)
    {
        *out = NULL;
        return J89A_NOMEM;
    }
    *out = r;
    return J89A_OK;
}

static j89a_status map_array(void *c, const j89a_json *value, j89a_json **out)
{
    j89a_map_ctx *mc;
    j89a_list *mapped;
    j89a_json *r;
    j89a_status st;
    mc = (j89a_map_ctx *)c;
    st = j89a_list_map(value->u.l, map_value, c, &mapped, mc->al);
    if (st != J89A_OK)
    {
        *out = NULL;
        return st;
    }
    r = j89a_json_array(mapped, mc->al);
    j89a_list_release(mapped);
    if (r == NULL)
    {
        *out = NULL;
        return J89A_NOMEM;
    }
    *out = r;
    return J89A_OK;
}

static j89a_status map_object(void *c, const j89a_json *value, j89a_json **out)
{
    j89a_map_ctx *mc;
    j89a_object *mapped;
    j89a_json *r;
    j89a_status st;
    mc = (j89a_map_ctx *)c;
    st = j89a_object_map(value->u.o, mc->string_map, map_value, c, &mapped,
                         mc->al);
    if (st != J89A_OK)
    {
        *out = NULL;
        return st;
    }
    r = j89a_json_object(mapped, mc->al);
    j89a_object_release(mapped);
    if (r == NULL)
    {
        *out = NULL;
        return J89A_NOMEM;
    }
    *out = r;
    return J89A_OK;
}

static j89a_status map_value(void *c, const j89a_json *value, j89a_json **out)
{
    const struct j89a_json *v;
    j89a_status st;
    v = value;
    if (v->kind == J89A_NULL)
    {
        st = map_null(c, out);
        return st;
    }
    if (v->kind == J89A_BOOLEAN)
    {
        st = map_boolean(c, value, out);
        return st;
    }
    if (v->kind == J89A_NUMBER)
    {
        st = map_number(c, value, out);
        return st;
    }
    if (v->kind == J89A_STRING)
    {
        st = map_string(c, value, out);
        return st;
    }
    if (v->kind == J89A_ARRAY)
    {
        st = map_array(c, value, out);
        return st;
    }
    st = map_object(c, value, out);
    return st;
}

j89a_json *j89a_json_map(const j89a_json *value, j89a_number_map_fn number_map,
                         j89a_string_map_fn string_map, void *ctx,
                         j89a_alloc *al)
{
    j89a_map_ctx mc;
    j89a_json *result;
    j89a_status st;
    mc.al = al;
    mc.number_map = number_map;
    mc.string_map = string_map;
    mc.user = ctx;
    st = map_value((void *)&mc, value, &result);
    if (st != J89A_OK)
    {
        return NULL;
    }
    return result;
}
