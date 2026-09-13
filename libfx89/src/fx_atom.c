/* fx_atom.c - nominal and parameterized effect atoms, atom comparison. */
#include "fx_internal.h"

#define FX_ATOM_BUCKETS 256u

int fx__atom_less(const fx_atom *a, const fx_atom *b)
{
    int cmp;
    if (a->kind != b->kind)
    {
        return a->kind < b->kind;
    }
    if (a->parameterized != b->parameterized)
    {
        return a->parameterized == 0;
    }
    if (a->parameterized == 0)
    {
        return 0;
    }
    cmp = a->domain->compare(a->domain_userdata, a->kind, a->param, b->param);
    return cmp < 0;
}

int fx__atom_eq(const fx_atom *a, const fx_atom *b)
{
    int cmp;
    if (a->kind != b->kind)
    {
        return 0;
    }
    if (a->parameterized != b->parameterized)
    {
        return 0;
    }
    if (a->parameterized == 0)
    {
        return 1;
    }
    cmp = a->domain->compare(a->domain_userdata, a->kind, a->param, b->param);
    return cmp == 0;
}

fx_status fx_ctx_set_atom_domain(fx_ctx *ctx, const fx_atom_domain *domain,
                                 void *domain_userdata)
{
    unsigned long i;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (domain == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (domain->compare == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (domain->copy == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (domain->destroy == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (ctx->has_param != 0)
    {
        return FX_ERR_INVALID;
    }
    ctx->domain = *domain;
    ctx->domain_userdata = domain_userdata;
    ctx->has_domain = 1;
    ctx->atombuckets =
        (fx_atom **)fx__alloc(ctx, FX_ATOM_BUCKETS * sizeof(fx_atom *));
    if (ctx->atombuckets == NULL)
    {
        ctx->has_domain = 0;
        return FX_ERR_NOMEM;
    }
    ctx->atombucket_n = FX_ATOM_BUCKETS;
    for (i = 0u; i < FX_ATOM_BUCKETS; ++i)
    {
        ctx->atombuckets[i] = NULL;
    }
    return FX_OK;
}

fx_status fx_atom_nominal(fx_ctx *ctx, fx_kind_id kind, const fx_atom **out)
{
    fx_kindrec *it;
    fx_atom *a;
    fx_atom *atom;
    fx_kindrec *owner;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (out == NULL)
    {
        return FX_ERR_INVALID;
    }
    owner = NULL;
    for (it = ctx->kinds; it != NULL; it = it->next)
    {
        if (it->id == kind)
        {
            owner = it;
            break;
        }
    }
    if (owner == NULL)
    {
        return FX_ERR_UNKNOWN;
    }
    for (a = ctx->atoms; a != NULL; a = a->next)
    {
        if (a->parameterized == 0)
        {
            if (a->kind == kind)
            {
                *out = a;
                return FX_OK;
            }
        }
    }
    atom = (fx_atom *)fx__alloc(ctx, sizeof(fx_atom));
    if (atom == NULL)
    {
        return FX_ERR_NOMEM;
    }
    atom->owner = ctx;
    atom->kind = kind;
    atom->parameterized = 0;
    atom->param = NULL;
    atom->domain = NULL;
    atom->domain_userdata = NULL;
    atom->next = ctx->atoms;
    atom->hnext = NULL;
    ctx->atoms = atom;
    *out = atom;
    return FX_OK;
}

static fx_status param_copy(fx_ctx *ctx, fx_kind_id kind, const void *value,
                            void **out)
{
    fx_status st;
    st = ctx->domain.copy(ctx->domain_userdata, kind, value, out);
    return st;
}

static unsigned long atom_bucket(fx_ctx *ctx, fx_kind_id kind,
                                 const void *param)
{
    unsigned long h;
    h = kind;
    if (ctx->domain.hash != NULL)
    {
        h = ctx->domain.hash(ctx->domain_userdata, kind, param);
    }
    return h & (FX_ATOM_BUCKETS - 1u);
}

static fx_status atom_alloc(fx_ctx *ctx, fx_kind_id kind, void *copy,
                            const fx_atom **out)
{
    fx_atom *atom;
    unsigned long b;
    atom = (fx_atom *)fx__alloc(ctx, sizeof(fx_atom));
    if (atom == NULL)
    {
        ctx->domain.destroy(ctx->domain_userdata, kind, copy);
        return FX_ERR_NOMEM;
    }
    atom->owner = ctx;
    atom->kind = kind;
    atom->parameterized = 1;
    atom->param = copy;
    atom->domain = &ctx->domain;
    atom->domain_userdata = ctx->domain_userdata;
    atom->next = ctx->atoms;
    ctx->atoms = atom;
    b = atom_bucket(ctx, kind, copy);
    atom->hnext = ctx->atombuckets[b];
    ctx->atombuckets[b] = atom;
    ctx->has_param = 1;
    *out = atom;
    return FX_OK;
}

static int atom_same_param(const fx_atom *a, const fx_atom *probe)
{
    if (a->parameterized == 0)
    {
        return 0;
    }
    if (a->kind != probe->kind)
    {
        return 0;
    }
    return fx__atom_eq(a, probe);
}

static fx_status reuse_param(fx_ctx *ctx, fx_kind_id kind, const fx_atom *a,
                             void *copy, const fx_atom **out)
{
    ctx->domain.destroy(ctx->domain_userdata, kind, copy);
    *out = a;
    return FX_OK;
}

static fx_atom *intern_advance(fx_atom *a)
{
    return a->hnext;
}

static fx_status intern_or_alloc(fx_ctx *ctx, fx_kind_id kind, void *copy,
                                 const fx_atom **out)
{
    fx_atom probe;
    fx_atom *a;
    fx_status st;
    int same;
    unsigned long b;
    probe.kind = kind;
    probe.parameterized = 1;
    probe.domain = &ctx->domain;
    probe.domain_userdata = ctx->domain_userdata;
    probe.param = copy;
    probe.next = NULL;
    b = atom_bucket(ctx, kind, copy);
    a = ctx->atombuckets[b];
    while (a != NULL)
    {
        same = atom_same_param(a, &probe);
        if (same != 0)
        {
            st = reuse_param(ctx, kind, a, copy, out);
            return st;
        }
        a = intern_advance(a);
    }
    st = atom_alloc(ctx, kind, copy, out);
    return st;
}

fx_status fx_atom_parameterized(fx_ctx *ctx, fx_kind_id kind,
                                const void *parameter, const fx_atom **out)
{
    fx_kindrec *it;
    fx_kindrec *owner;
    void *copy;
    fx_status st;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (parameter == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (out == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (ctx->has_domain == 0)
    {
        return FX_ERR_INVALID;
    }
    owner = NULL;
    for (it = ctx->kinds; it != NULL; it = it->next)
    {
        if (it->id == kind)
        {
            owner = it;
            break;
        }
    }
    if (owner == NULL)
    {
        return FX_ERR_UNKNOWN;
    }
    copy = NULL;
    st = param_copy(ctx, kind, parameter, &copy);
    if (st != FX_OK)
    {
        return st;
    }
    st = intern_or_alloc(ctx, kind, copy, out);
    return st;
}

fx_kind_id fx_atom_kind(const fx_atom *atom)
{
    if (atom == NULL)
    {
        return 0u;
    }
    return atom->kind;
}

int fx_atom_is_parameterized(const fx_atom *atom)
{
    if (atom == NULL)
    {
        return 0;
    }
    return atom->parameterized;
}

const void *fx_atom_parameter(const fx_atom *atom)
{
    if (atom == NULL)
    {
        return NULL;
    }
    return atom->param;
}

int fx_atom_equal(const fx_ctx *ctx, const fx_atom *a, const fx_atom *b)
{
    int res;
    if (a == NULL)
    {
        return 0;
    }
    if (b == NULL)
    {
        return 0;
    }
    if (ctx != NULL)
    {
        if (a->owner != ctx)
        {
            return 0;
        }
        if (b->owner != ctx)
        {
            return 0;
        }
    }
    if (a->owner != b->owner)
    {
        return 0;
    }
    res = fx__atom_eq(a, b);
    return res;
}
