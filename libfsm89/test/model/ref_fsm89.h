#ifndef REF_FSM89_H
#define REF_FSM89_H

#include "fsm89.h"

/* Independent reference model for differential testing. It shares no helper
 * with src/ and is deliberately written as a direct linear scan. */

typedef struct ref_step
{
    int found; /* -1 unknown state, 0 missing transition, 1 transition */
    fsm89_state to;
    const fsm89_state_def *source;
    const fsm89_edge *edge;
    const fsm89_state_def *dest;
} ref_step;

int ref_validate(const fsm89_def *def);
void ref_step_eval(const fsm89_def *def, fsm89_state state, fsm89_event event,
                   ref_step *out);
int ref_accepting(const fsm89_def *def, fsm89_state state);

#endif
