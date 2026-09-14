/* reject_write_through_result.c - this file must NOT compile: effect spans in
 * a step result point to const storage. */

#include <fsm89.h>

int main(void)
{
    fsm89_step_result r;

    r.enter.v = NULL;
    r.enter.n = 1;
    r.enter.v[0] = 123;
    return 0;
}
