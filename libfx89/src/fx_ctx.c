/* fx_ctx.c - context lifecycle, allocation, and shared helpers. */
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "fx_internal.h"

int fx__mul_overflow(unsigned long a, unsigned long b)
{
    if (a == 0u)
    {
        return 0;
    }
    if (b > ULONG_MAX / a)
    {
        return 1;
    }
    return 0;
}

static void *raw_alloc(unsigned long size)
{
    void *p;
    p = malloc(size);
    return p;
}

void *fx__alloc(fx_ctx *ctx, unsigned long size)
{
    fx_block *block;
    char *data;
    if (size == 0)
    {
        size = 1;
    }
    block = (fx_block *)raw_alloc(sizeof(fx_block) + size);
    if (block == NULL)
    {
        return NULL;
    }
    data = (char *)(block + 1);
    block->next = ctx->blocks;
    ctx->blocks = block;
    return data;
}

void *fx__alloc_atoms(fx_ctx *ctx, unsigned long count)
{
    void *p;
    if (fx__mul_overflow(count, sizeof(fx_atom)) != 0)
    {
        return NULL;
    }
    p = fx__alloc(ctx, count * sizeof(fx_atom));
    return p;
}

void *fx__alloc_vars(fx_ctx *ctx, unsigned long count)
{
    void *p;
    if (fx__mul_overflow(count, sizeof(fx_var)) != 0)
    {
        return NULL;
    }
    p = fx__alloc(ctx, count * sizeof(fx_var));
    return p;
}

char *fx__strdup(fx_ctx *ctx, const char *s)
{
    size_t n;
    char *out;
    n = strlen(s);
    out = (char *)fx__alloc(ctx, n + 1u);
    if (out == NULL)
    {
        return NULL;
    }
    memcpy(out, s, n + 1u);
    return out;
}

/* Grow arr to newcap and copy the first count pointers. */
static const fx_atom **grow_copy(fx_ctx *ctx, const fx_atom **arr,
                                 unsigned long count, unsigned long newcap)
{
    const fx_atom **out;
    out = (const fx_atom **)fx__alloc(ctx, newcap * sizeof(const fx_atom *));
    if (out == NULL)
    {
        return NULL;
    }
    if (count != 0)
    {
        memcpy(out, arr, count * sizeof(const fx_atom *));
    }
    return out;
}

static unsigned long grow_cap(unsigned long cap)
{
    if (cap == 0)
    {
        return 4u;
    }
    return cap * 2u;
}

static const fx_atom **ensure_capacity(fx_ctx *ctx, const fx_atom **arr,
                                       unsigned long n, unsigned long *cap)
{
    const fx_atom **out;
    unsigned long nc;
    if (n < *cap)
    {
        return arr;
    }
    if (*cap > ULONG_MAX / 2u)
    {
        return NULL;
    }
    nc = grow_cap(*cap);
    out = grow_copy(ctx, arr, n, nc);
    if (out == NULL)
    {
        return NULL;
    }
    *cap = nc;
    return out;
}

const fx_atom **fx__arr_push(fx_ctx *ctx, const fx_atom **arr, unsigned long *n,
                             unsigned long *cap, const fx_atom *atom)
{
    const fx_atom **out;
    out = ensure_capacity(ctx, arr, *n, cap);
    if (out == NULL)
    {
        return NULL;
    }
    out[*n] = atom;
    *n = *n + 1u;
    return out;
}

fx_status fx_ctx_new(fx_ctx **out)
{
    fx_ctx *ctx;
    if (out == NULL)
    {
        return FX_ERR_INVALID;
    }
    ctx = (fx_ctx *)raw_alloc(sizeof(fx_ctx));
    if (ctx == NULL)
    {
        return FX_ERR_NOMEM;
    }
    ctx->blocks = NULL;
    ctx->kinds = NULL;
    ctx->kinds_tail = NULL;
    ctx->ops = NULL;
    ctx->ops_tail = NULL;
    ctx->atoms = NULL;
    ctx->vars = NULL;
    ctx->constraints = NULL;
    ctx->constraints_tail = NULL;
    ctx->next_kind = 1u;
    ctx->next_op = 1u;
    ctx->next_var = 1u;
    ctx->next_cid = 1u;
    ctx->fact_serial = 0u;
    ctx->conflict = FX_CONFLICT_NONE;
    ctx->conflict_primary = NULL;
    ctx->conflict_secondary = NULL;
    ctx->domain.copy = NULL;
    ctx->domain.destroy = NULL;
    ctx->domain.compare = NULL;
    ctx->domain.hash = NULL;
    ctx->domain_userdata = NULL;
    ctx->has_domain = 0;
    ctx->has_param = 0;
    ctx->next_cp = 1u;
    ctx->cp_top = NULL;
    ctx->trail = NULL;
    ctx->trail_n = 0u;
    ctx->trail_cap = 0u;
    ctx->q_head = NULL;
    ctx->q_tail = NULL;
    ctx->solving = 0;
    ctx->atombuckets = NULL;
    ctx->atombucket_n = 0u;
    *out = ctx;
    return FX_OK;
}

static void atom_destroy(fx_atom *atom)
{
    if (atom->parameterized != 0)
    {
        atom->domain->destroy(atom->domain_userdata, atom->kind, atom->param);
    }
}

static fx_atom *release_atom(fx_atom *atom)
{
    fx_atom *next;
    next = atom->next;
    atom_destroy(atom);
    return next;
}

static void destroy_atoms(fx_ctx *ctx)
{
    fx_atom *atom;
    atom = ctx->atoms;
    while (atom != NULL)
    {
        atom = release_atom(atom);
    }
}

static fx_block *release_block(fx_block *b)
{
    fx_block *next;
    next = b->next;
    free(b);
    return next;
}

void fx_ctx_free(fx_ctx *ctx)
{
    fx_block *block;
    if (ctx == NULL)
    {
        return;
    }
    destroy_atoms(ctx);
    block = ctx->blocks;
    while (block != NULL)
    {
        block = release_block(block);
    }
    free(ctx);
}

void fx__record_conflict(fx_ctx *ctx, fx_conflict_kind kind,
                         const fx_constraint *primary,
                         const fx_constraint *secondary)
{
    (void)fx__trail_conflict(ctx, ctx->conflict, ctx->conflict_primary,
                             ctx->conflict_secondary);
    ctx->conflict = kind;
    ctx->conflict_primary = primary;
    ctx->conflict_secondary = secondary;
}

int fx__atom_local(fx_ctx *ctx, const fx_atom *atom)
{
    if (atom == NULL)
    {
        return 0;
    }
    return atom->owner == ctx;
}

int fx__var_local(fx_ctx *ctx, const fx_var *var)
{
    if (var == NULL)
    {
        return 0;
    }
    return var->owner == ctx;
}

static int row_heads_local(fx_ctx *ctx, const fx_row *row)
{
    unsigned long i;
    for (i = 0; i < row->nhead; ++i)
    {
        if (row->head[i]->owner != ctx)
        {
            return 0;
        }
    }
    return 1;
}

int fx__row_local(fx_ctx *ctx, const fx_row *row)
{
    int ok;
    if (row == NULL)
    {
        return 0;
    }
    if (row->owner != ctx)
    {
        return 0;
    }
    ok = row_heads_local(ctx, row);
    return ok;
}

int fx__constraint_local(fx_ctx *ctx, const fx_constraint *c)
{
    if (c == NULL)
    {
        return 0;
    }
    return c->owner == ctx;
}

fx_status fx_last_conflict(const fx_ctx *ctx, fx_conflict_kind *out_kind,
                           const fx_constraint **out_primary,
                           const fx_constraint **out_secondary)
{
    fx_conflict_kind kind;
    const fx_constraint *primary;
    const fx_constraint *secondary;
    if (ctx == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (out_kind == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (out_primary == NULL)
    {
        return FX_ERR_INVALID;
    }
    if (out_secondary == NULL)
    {
        return FX_ERR_INVALID;
    }
    kind = ctx->conflict;
    primary = ctx->conflict_primary;
    secondary = ctx->conflict_secondary;
    *out_kind = kind;
    *out_primary = primary;
    *out_secondary = secondary;
    return FX_OK;
}

unsigned long fx_version_major(void)
{
    return FX89_VERSION_MAJOR;
}

unsigned long fx_version_minor(void)
{
    return FX89_VERSION_MINOR;
}

unsigned long fx_version_patch(void)
{
    return FX89_VERSION_PATCH;
}
