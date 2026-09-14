/* header_check.cpp - the public header compiles under C++23 and keeps its
 * C ABI shape. */

#include <cstddef>

#include "fsm89.h"

static_assert(sizeof(fsm89_effects) == sizeof(void *) + sizeof(std::size_t),
              "fsm89_effects layout");
static_assert(sizeof(fsm89_step_result) >=
                  sizeof(fsm89_state) + sizeof(fsm89_event) +
                      sizeof(fsm89_state) + 3 * sizeof(fsm89_effects),
              "fsm89_step_result layout");

int main()
{
    fsm89_step_result r;

    r.enter.v = nullptr;
    r.enter.n = 0;
    if (r.enter.n == 0)
    {
        return 0;
    }
    return 1;
}
