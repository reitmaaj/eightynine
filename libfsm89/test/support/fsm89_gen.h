#ifndef FSM89_GEN_H
#define FSM89_GEN_H

#include "fsm89.h"

#define FSM89_GEN_MAX_STATES 5
#define FSM89_GEN_MAX_EVENTS 4
#define FSM89_GEN_MAX_EDGES 20
#define FSM89_GEN_MAX_FX 2

/* Deterministic random valid machine. Pointers in def reference the arrays
 * inside this object, so it must not be copied. */
typedef struct fsm89_gen
{
    fsm89_state_def states[FSM89_GEN_MAX_STATES];
    fsm89_edge edges[FSM89_GEN_MAX_EDGES];
    fsm89_effect state_fx[FSM89_GEN_MAX_STATES][2][FSM89_GEN_MAX_FX];
    fsm89_effect edge_fx[FSM89_GEN_MAX_EDGES][FSM89_GEN_MAX_FX];
    fsm89_def def;
} fsm89_gen;

/* Build a valid machine with nstates (1..MAX) and nevents (1..MAX). Every
 * state ID is in [0, nstates), every edge key is unique, and the initial
 * state exists. */
void fsm89_gen_random(fsm89_gen *g, unsigned long *seed, size_t nstates,
                      size_t nevents);

/* Exhaustive enumeration: `code` in base (nstates + 1) over the nstates *
 * nevents cells; digit 0 means no transition, digit d > 0 means the target
 * d - 1. Deterministic effects exercise empty and nonempty spans. */
void fsm89_gen_from_code(fsm89_gen *g, size_t nstates, size_t nevents,
                         unsigned long code);

/* Copy src with state and edge tables reversed. */
void fsm89_gen_reorder(fsm89_gen *dst, const fsm89_gen *src, size_t nstates);

/* Copy src with state IDs, event IDs, and effect IDs shifted by
 * state_delta, event_delta, and effect_delta. */
void fsm89_gen_rename(fsm89_gen *dst, const fsm89_gen *src, size_t nstates,
                      size_t nevents, unsigned long state_delta,
                      unsigned long event_delta, unsigned long effect_delta);

/* Compare production results against the reference model for every
 * (state, event) pair in [0, nstates] x [0, nevents] plus one unknown state
 * and event probe. */
void fsm89_gen_compare(const fsm89_def *def, size_t nstates, size_t nevents,
                       const char *label);

#endif
