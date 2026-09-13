/* json.c - the six JSON constructors, one-layer project/embed for j89_alg. */
#include "alg_internal.h"

void j89a_json_node_free(struct j89a_json *value)
{
    j89a_nfree(value->hdr.freer, value->hdr.fctx, value);
}

static struct j89a_json *new_json(j89a_alloc *al)
{
    struct j89a_json *value;
    value = (struct j89a_json *)j89a_malloc(al, sizeof(struct j89a_json));
    if (value == NULL)
    {
        return NULL;
    }
    value->hdr.refs = 1;
    value->hdr.freer = al->free;
    value->hdr.fctx = al->ctx;
    value->kind = J89A_NULL;
    value->u.b = J89A_FALSE;
    value->u.n = 0.0;
    value->u.s = NULL;
    value->u.l = NULL;
    value->u.o = NULL;
    return value;
}

void j89a_json_retain(j89a_json *value)
{
    if (value == NULL)
    {
        return;
    }
    value->hdr.refs = value->hdr.refs + 1;
}

static void release_string(struct j89a_json *value)
{
    j89a_str_release(value->u.s);
}

static void release_array(struct j89a_json *value)
{
    j89a_list_release(value->u.l);
}

static void release_object(struct j89a_json *value)
{
    j89a_object_release(value->u.o);
}

void j89a_json_release(j89a_json *value)
{
    if (value == NULL)
    {
        return;
    }
    value->hdr.refs = value->hdr.refs - 1;
    if (value->hdr.refs == 0)
    {
        if (value->kind == J89A_STRING)
        {
            release_string(value);
        }
        else if (value->kind == J89A_ARRAY)
        {
            release_array(value);
        }
        else if (value->kind == J89A_OBJECT)
        {
            release_object(value);
        }
        j89a_json_node_free(value);
    }
}

static void project_bool(const struct j89a_json *v, j89a_shape *shape)
{
    shape->boolean_value = v->u.b;
}

static void project_number(const struct j89a_json *v, j89a_shape *shape)
{
    shape->number_value = v->u.n;
}

static void project_string(const struct j89a_json *v, j89a_shape *shape)
{
    shape->string_value = v->u.s;
}

static void project_array(const struct j89a_json *v, j89a_shape *shape)
{
    shape->array_value = v->u.l;
}

static void project_object(const struct j89a_json *v, j89a_shape *shape)
{
    shape->object_value = v->u.o;
}

void j89a_json_project(const j89a_json *value, j89a_shape *shape)
{
    const struct j89a_json *v;
    v = value;
    shape->kind = v->kind;
    shape->boolean_value = J89A_FALSE;
    shape->number_value = 0.0;
    shape->string_value = NULL;
    shape->array_value = NULL;
    shape->object_value = NULL;
    if (v->kind == J89A_BOOLEAN)
    {
        project_bool(v, shape);
    }
    else if (v->kind == J89A_NUMBER)
    {
        project_number(v, shape);
    }
    else if (v->kind == J89A_STRING)
    {
        project_string(v, shape);
    }
    else if (v->kind == J89A_ARRAY)
    {
        project_array(v, shape);
    }
    else if (v->kind == J89A_OBJECT)
    {
        project_object(v, shape);
    }
}

static void embed_bool(struct j89a_json *v, const j89a_shape *shape)
{
    v->u.b = shape->boolean_value;
}

static void embed_number(struct j89a_json *v, const j89a_shape *shape)
{
    v->u.n = shape->number_value;
}

static void embed_string(struct j89a_json *v, const j89a_shape *shape)
{
    v->u.s = shape->string_value;
    j89a_str_retain(v->u.s);
}

static void embed_array(struct j89a_json *v, const j89a_shape *shape)
{
    v->u.l = shape->array_value;
    j89a_list_retain(v->u.l);
}

static void embed_object(struct j89a_json *v, const j89a_shape *shape)
{
    v->u.o = shape->object_value;
    j89a_object_retain(v->u.o);
}

j89a_json *j89a_json_embed(const j89a_shape *shape, j89a_alloc *al)
{
    struct j89a_json *value;
    value = new_json(al);
    if (value == NULL)
    {
        return NULL;
    }
    value->kind = shape->kind;
    if (shape->kind == J89A_BOOLEAN)
    {
        embed_bool(value, shape);
    }
    else if (shape->kind == J89A_NUMBER)
    {
        embed_number(value, shape);
    }
    else if (shape->kind == J89A_STRING)
    {
        embed_string(value, shape);
    }
    else if (shape->kind == J89A_ARRAY)
    {
        embed_array(value, shape);
    }
    else if (shape->kind == J89A_OBJECT)
    {
        embed_object(value, shape);
    }
    return value;
}

j89a_json *j89a_json_null(j89a_alloc *al)
{
    struct j89a_json *value;
    value = new_json(al);
    return value;
}

j89a_json *j89a_json_boolean(j89a_bool b, j89a_alloc *al)
{
    struct j89a_json *value;
    value = new_json(al);
    if (value == NULL)
    {
        return NULL;
    }
    value->kind = J89A_BOOLEAN;
    value->u.b = b;
    return value;
}

j89a_json *j89a_json_number(double n, j89a_alloc *al)
{
    struct j89a_json *value;
    value = new_json(al);
    if (value == NULL)
    {
        return NULL;
    }
    value->kind = J89A_NUMBER;
    value->u.n = n;
    return value;
}

j89a_json *j89a_json_string(j89a_str *s, j89a_alloc *al)
{
    struct j89a_json *value;
    value = new_json(al);
    if (value == NULL)
    {
        return NULL;
    }
    value->kind = J89A_STRING;
    value->u.s = s;
    j89a_str_retain(s);
    return value;
}

j89a_json *j89a_json_array(j89a_list *xs, j89a_alloc *al)
{
    struct j89a_json *value;
    value = new_json(al);
    if (value == NULL)
    {
        return NULL;
    }
    value->kind = J89A_ARRAY;
    value->u.l = xs;
    j89a_list_retain(xs);
    return value;
}

j89a_json *j89a_json_object(j89a_object *ms, j89a_alloc *al)
{
    struct j89a_json *value;
    value = new_json(al);
    if (value == NULL)
    {
        return NULL;
    }
    value->kind = J89A_OBJECT;
    value->u.o = ms;
    j89a_object_retain(ms);
    return value;
}
