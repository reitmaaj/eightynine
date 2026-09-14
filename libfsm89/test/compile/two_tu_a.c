/* two_tu_a.c - two translation units may include the header. */

#include <fsm89.h>

int fsm89_compile_b(const fsm89_def *def);

int main(void)
{
    if (fsm89_compile_b(NULL) != FSM89_EDEF)
    {
        return 1;
    }
    return 0;
}
