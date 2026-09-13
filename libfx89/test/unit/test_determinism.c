/* test_determinism.c - identical scenarios yield identical results. */
#include "fx_fixture.h"

static void scenario(fx_ctx *ctx, const fx_atom *a, const fx_atom *b,
                     const fx_atom *cc, const fx_row **out)
{
    const fx_atom *atoms[2];
    const fx_row *x;
    const fx_row *y;
    const fx_row *u;
    atoms[0] = a;
    atoms[1] = b;
    fx_row_closed(ctx, atoms, 2u, &x);
    atoms[0] = b;
    atoms[1] = cc;
    fx_row_closed(ctx, atoms, 2u, &y);
    CHECK(fx_row_union(ctx, x, y, &u) == FX_OK);
    *out = u;
}

static void run_same_outcome(void)
{
    fx_ctx *c1;
    fx_ctx *c2;
    const fx_atom *a1;
    const fx_atom *b1;
    const fx_atom *c1a;
    const fx_atom *a2;
    const fx_atom *b2;
    const fx_atom *c2a;
    const fx_row *u1;
    const fx_row *u2;
    unsigned long n1;
    unsigned long n2;
    CHECK(fx_fixture_new(&c1, &a1, &b1, &c1a));
    CHECK(fx_fixture_new(&c2, &a2, &b2, &c2a));
    scenario(c1, a1, b1, c1a, &u1);
    scenario(c2, a2, b2, c2a, &u2);
    n1 = fx_row_atom_count(c1, u1);
    n2 = fx_row_atom_count(c2, u2);
    CHECK(n1 == 3u);
    CHECK(n1 == n2);
    fx_ctx_free(c1);
    fx_ctx_free(c2);
}

int main(void)
{
    run_same_outcome();
    TEST_END
}
