/* repl89_mem.c - fault-injectable allocation hooks. */

#include <stdlib.h>

#include "repl89_internal.h"

static void *default_alloc(size_t n)
{
    void *p;

    p = malloc(n);
    return p;
}

static void *default_grow(void *p, size_t n)
{
    void *q;

    q = realloc(p, n);
    return q;
}

static void default_release(void *p)
{
    free(p);
}

static void *(*g_alloc)(size_t) = default_alloc;
static void *(*g_grow)(void *, size_t) = default_grow;
static void (*g_release)(void *) = default_release;

void *repl89_mem_alloc(size_t n)
{
    void *p;

    p = g_alloc(n);
    return p;
}

void *repl89_mem_realloc(void *p, size_t n)
{
    void *q;

    q = g_grow(p, n);
    return q;
}

void repl89_mem_free(void *p)
{
    g_release(p);
}

void repl89_mem_set_hooks(void *(*alloc)(size_t), void *(*grow)(void *, size_t),
                          void (*release)(void *))
{
    g_alloc = alloc;
    g_grow = grow;
    g_release = release;
}

void repl89_mem_reset_hooks(void)
{
    g_alloc = default_alloc;
    g_grow = default_grow;
    g_release = default_release;
}
