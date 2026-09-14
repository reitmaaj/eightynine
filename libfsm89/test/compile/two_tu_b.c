/* two_tu_b.c - second translation unit including the header. */

#include <fsm89.h>

int fsm89_compile_b(const fsm89_def *def)
{
    return (int)fsm89_validate(def);
}
