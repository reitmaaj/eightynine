/* test_regress_stack.c - deep linear traversals must terminate without
 * overflowing the call stack (D1). A recursive implementation of these walks
 * would overflow at these depths; the iterative forms must return the correct
 * result. */
#include "fx_fixture.h"

#define DEEP 200000u

static void run_deep_remove_many(void)
{
    fx_ctx *ctx;
    const fx_atom *a;
    const fx_atom *b;
    const fx_atom *c;
    const fx_atom **atoms;
    const fx_row *row;
    const fx_row *out;
    unsigned long i;
    CHECK(fx_fixture_new(&ctx, &a, &b, &c));
    atoms = (const fx_atom **)malloc(DEEP * sizeof(const fx_atom *));
    CHECK(atoms != NULL);
    if (atoms == NULL)
    {
        fx_ctx_free(ctx);
        return;
    }
    for (i = 0; i < DEEP; ++i)
    {
        atoms[i] = a;
    }
    fx_row_closed(ctx, atoms, DEEP, &row);
    out = NULL;
    CHECK(fx_row_remove_many(ctx, row, atoms, DEEP, &out) == FX_OK);
    CHECK(fx_row_atom_count(ctx, out) == 0u);
    free(atoms);
    fx_ctx_free(ctx);
}

int main(void)
{
    run_deep_remove_many();
    TEST_END
}
