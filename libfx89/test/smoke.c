/* smoke.c - end-to-end smoke over libfx89: create a context, declare two
 * effect kinds, build nominal atoms and a closed row, normalize a duplicate
 * row, assert membership, equate two permutation-equivalent closed rows, and
 * solve. Exercises ctx -> kind -> atom -> row -> constraint -> solver. */
#include <stdio.h>

#include <fx89.h>

static int fail(const char *what)
{
    fprintf(stderr, "smoke failed: %s\n", what);
    return 1;
}

int main(void)
{
    fx_ctx *ctx;
    fx_kind_id kconsole;
    fx_kind_id krandom;
    const fx_atom *console;
    const fx_atom *random;
    const fx_row *row;
    const fx_row *other;
    const fx_atom *atoms[2];
    fx_status st;
    if (fx_ctx_new(&ctx) != FX_OK)
    {
        return fail("fx_ctx_new");
    }
    if (fx_kind_define(ctx, "Console", 0, &kconsole) != FX_OK ||
        fx_kind_define(ctx, "Random", 0, &krandom) != FX_OK)
    {
        fx_ctx_free(ctx);
        return fail("fx_kind_define");
    }
    if (fx_atom_nominal(ctx, kconsole, &console) != FX_OK ||
        fx_atom_nominal(ctx, krandom, &random) != FX_OK)
    {
        fx_ctx_free(ctx);
        return fail("fx_atom_nominal");
    }
    atoms[0] = console;
    atoms[1] = random;
    if (fx_row_closed(ctx, atoms, 2u, &row) != FX_OK)
    {
        fx_ctx_free(ctx);
        return fail("fx_row_closed");
    }
    if (fx_row_atom_count(ctx, row) != 2u)
    {
        fx_ctx_free(ctx);
        return fail("closed row count");
    }
    if (fx_row_membership(ctx, row, console) != FX_TRUE ||
        fx_row_membership(ctx, row, random) != FX_TRUE)
    {
        fx_ctx_free(ctx);
        return fail("membership");
    }
    atoms[0] = random;
    atoms[1] = console;
    if (fx_row_closed(ctx, atoms, 2u, &other) != FX_OK)
    {
        fx_ctx_free(ctx);
        return fail("fx_row_closed other");
    }
    if (!fx_row_equal(ctx, row, other))
    {
        fx_ctx_free(ctx);
        return fail("permutation rows not equal");
    }
    if (fx_require_equal(ctx, row, other, 0) != FX_OK)
    {
        fx_ctx_free(ctx);
        return fail("fx_require_equal");
    }
    st = fx_solve(ctx);
    if (st != FX_OK)
    {
        fx_ctx_free(ctx);
        return fail("fx_solve");
    }
    fx_ctx_free(ctx);
    printf("smoke ok\n");
    return 0;
}
