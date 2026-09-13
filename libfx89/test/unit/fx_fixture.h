#ifndef FX_FIXTURE_H
#define FX_FIXTURE_H

/* fx_fixture.h - shared static helpers for libfx89 unit tests. Not part of
 * the library. Static so it may be included from many translation units. */

#include "test.h"

static int fx_fixture_new(
    fx_ctx **out_ctx,
    const fx_atom **a,
    const fx_atom **b,
    const fx_atom **c)
{
    fx_ctx *ctx;
    fx_kind_id ka;
    fx_kind_id kb;
    fx_kind_id kc;
    if (fx_ctx_new(&ctx) != FX_OK)
    {
        return 0;
    }
    if (fx_kind_define(ctx, "A", 0, &ka) != FX_OK ||
        fx_kind_define(ctx, "B", 0, &kb) != FX_OK ||
        fx_kind_define(ctx, "C", 0, &kc) != FX_OK)
    {
        fx_ctx_free(ctx);
        return 0;
    }
    if (fx_atom_nominal(ctx, ka, a) != FX_OK ||
        fx_atom_nominal(ctx, kb, b) != FX_OK ||
        fx_atom_nominal(ctx, kc, c) != FX_OK)
    {
        fx_ctx_free(ctx);
        return 0;
    }
    *out_ctx = ctx;
    return 1;
}

#endif /* FX_FIXTURE_H */
