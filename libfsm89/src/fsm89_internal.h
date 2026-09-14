#ifndef FSM89_INTERNAL_H
#define FSM89_INTERNAL_H

#include "fsm89.h"

/* 1 when the span is empty or owns storage. Pure. */
int fsm89__span_ok(fsm89_effects span);

/* Borrowed state definition, or NULL when id is absent. Pure. */
const fsm89_state_def *fsm89__find_state(const fsm89_def *def, fsm89_state id);

/* Borrowed edge for the unique (from, event) key, or NULL. Pure. */
const fsm89_edge *fsm89__find_edge(const fsm89_def *def, fsm89_state from,
                                   fsm89_event event);

#endif
