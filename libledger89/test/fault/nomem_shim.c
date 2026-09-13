/* nomem_shim.c - allocation failure injection via GNU ld --wrap. */

#include <stddef.h>
#include <stdlib.h>

#include "nomem_shim.h"

static int armed;
static int countdown;
static int fired;

void nomem_arm(int skip)
{
    armed = 1;
    countdown = skip;
    fired = 0;
}

void nomem_disarm(void)
{
    armed = 0;
}

int nomem_fired(void)
{
    return fired;
}

static int nomem_hit(void)
{
    if (armed == 0)
    {
        return 0;
    }
    if (countdown > 0)
    {
        --countdown;
        return 0;
    }
    armed = 0;
    fired = 1;
    return 1;
}

void *__real_malloc(size_t n);
void *__real_calloc(size_t n, size_t m);
void *__real_realloc(void *p, size_t n);

void *__wrap_malloc(size_t n);
void *__wrap_calloc(size_t n, size_t m);
void *__wrap_realloc(void *p, size_t n);

void *__wrap_malloc(size_t n)
{
    if (nomem_hit() != 0)
    {
        return NULL;
    }
    return __real_malloc(n);
}

void *__wrap_calloc(size_t n, size_t m)
{
    if (nomem_hit() != 0)
    {
        return NULL;
    }
    return __real_calloc(n, m);
}

void *__wrap_realloc(void *p, size_t n)
{
    if (nomem_hit() != 0)
    {
        return NULL;
    }
    return __real_realloc(p, n);
}
