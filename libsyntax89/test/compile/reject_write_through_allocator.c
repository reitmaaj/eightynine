/* reject_write_through_allocator.c - this file must NOT compile: a const
 * allocator is not writable through the public type. */

#include "syntax89.h"

int main(void)
{
    const syntax89_allocator *a;

    a = NULL;
    a->alloc = NULL;
    return 0;
}
