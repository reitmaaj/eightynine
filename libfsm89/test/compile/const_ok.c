/* const_ok.c - const definitions are accepted everywhere. */

#include <fsm89.h>

static const fsm89_effect fx[] = {1};

static const fsm89_state_def states[] = {{0, {fx, 1}, {NULL, 0}, 0}};

static const fsm89_def def = {states, 1, NULL, 0, 0};

int main(void)
{
    fsm89_step_result r;

    if (fsm89_validate(&def) != FSM89_OK)
    {
        return 1;
    }
    if (fsm89_step(&def, 0, 0, &r) != FSM89_NO_TRANSITION)
    {
        return 1;
    }
    return 0;
}
