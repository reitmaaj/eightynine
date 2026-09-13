/* hm_env.c - lexical type environments with child scoping. */
#include <hm.h>

#include "hm_internal.h"

#include <string.h>

hm_env *hm_env_new(hm_ctx *ctx)
{
    hm_env *env;
    void *raw;
    raw = hm_i_alloc(ctx, sizeof(hm_env));
    if (raw == NULL)
    {
        return NULL;
    }
    env = raw;
    env->ctx = ctx;
    env->parent = NULL;
    env->bindings = NULL;
    return env;
}

hm_env *hm_env_child(hm_ctx *ctx, hm_env *parent)
{
    hm_env *env;
    env = hm_env_new(ctx);
    if (env == NULL)
    {
        return NULL;
    }
    env->parent = parent;
    return env;
}

hm_status hm_env_bind(hm_env *env, const char *name, hm_scheme *scheme)
{
    struct hm_binding *binding;
    char *n;
    void *raw;
    if (scheme == NULL)
    {
        return HM_ERROR_INVALID;
    }
    n = hm_i_strdup(env->ctx, name);
    if (n == NULL)
    {
        return HM_ERROR_NOMEM;
    }
    raw = hm_i_alloc(env->ctx, sizeof(struct hm_binding));
    if (raw == NULL)
    {
        return HM_ERROR_NOMEM;
    }
    binding = raw;
    binding->name = n;
    binding->scheme = scheme;
    binding->next = env->bindings;
    env->bindings = binding;
    return HM_OK;
}

static hm_scheme *hm_i_lookup_binding(struct hm_binding *binding,
                                      const char *name)
{
    hm_scheme *found;
    int cmp;
    if (binding == NULL)
    {
        return NULL;
    }
    cmp = strcmp(binding->name, name);
    if (cmp == 0)
    {
        return binding->scheme;
    }
    found = hm_i_lookup_binding(binding->next, name);
    return found;
}

hm_scheme *hm_env_lookup(hm_env *env, const char *name)
{
    hm_scheme *found;
    if (env == NULL)
    {
        return NULL;
    }
    found = hm_i_lookup_binding(env->bindings, name);
    if (found != NULL)
    {
        return found;
    }
    found = hm_env_lookup(env->parent, name);
    return found;
}
