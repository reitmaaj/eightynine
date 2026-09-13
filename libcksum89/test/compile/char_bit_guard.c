/*
 * char_bit_guard.c - compile-failure probe for the header's machine model.
 *
 * This translation unit MUST NOT compile: it overrides CHAR_BIT to 16
 * before including cksum89.h. `just guard` asserts the failure.
 */

#include <limits.h>

#undef CHAR_BIT
#define CHAR_BIT 16

#include "cksum89.h"

int main(void)
{
    return 0;
}
