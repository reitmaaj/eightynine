#ifndef CAT89_FINITE_FIXTURES_H
#define CAT89_FINITE_FIXTURES_H

/* cat89_finite_fixtures.h - small reference categories over the finite
 * backend, used as mathematical oracles by the test suite. */
#include <cat89/cat89.h>

enum cat89_fixture_kind {
    CAT89_FIX_TERMINAL = 0,
    CAT89_FIX_DISCRETE2,
    CAT89_FIX_ARROW,
    CAT89_FIX_COMP2,
    CAT89_FIX_C2_GROUPOID,
    CAT89_FIX_BINPROD,
    CAT89_FIX_BINCOPROD,
    CAT89_FIX_EQUALIZER,
    CAT89_FIX_COEQUALIZER,
    CAT89_FIX_PULLBACK,
    CAT89_FIX_PUSHOUT
};

/* Build the named category plus its eq and enum capabilities. */
cat89_status cat89_fixture_build(enum cat89_fixture_kind kind,
                                 cat89_category **out_category,
                                 cat89_eq **out_eq,
                                 cat89_enum **out_enum);

#endif
