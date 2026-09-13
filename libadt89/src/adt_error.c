/* adt_error.c - structured error accessors. */
#include <adt.h>

#include "adt_internal.h"

adt_error_kind adt_error_kind_of(const adt_error *error)
{
    if (error == NULL)
    {
        return ADT_ERR_NONE;
    }
    return error->kind;
}

const char *adt_error_message(const adt_error *error)
{
    if (error == NULL)
    {
        return "";
    }
    if (error->message == NULL)
    {
        return "";
    }
    return error->message;
}

const adt_type *adt_error_type(const adt_error *error)
{
    if (error == NULL)
    {
        return NULL;
    }
    return error->type;
}

const adt_ctor *adt_error_ctor(const adt_error *error)
{
    if (error == NULL)
    {
        return NULL;
    }
    return error->ctor;
}

const hm_error *adt_error_hm(const adt_error *error)
{
    if (error == NULL)
    {
        return NULL;
    }
    return error->hm_error;
}

void *adt_error_origin(const adt_error *error)
{
    if (error == NULL)
    {
        return NULL;
    }
    return error->origin;
}
