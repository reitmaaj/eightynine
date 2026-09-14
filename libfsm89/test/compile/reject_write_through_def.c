/* reject_write_through_def.c - this file must NOT compile: a const definition
 * is not writable through the public API. */

#include <fsm89.h>

static const fsm89_state_def states[] = {{0, {NULL, 0}, {NULL, 0}, 0}};

static const fsm89_def def = {states, 1, NULL, 0, 0};

int main(void)
{
    def.initial = 1;
    return 0;
}
