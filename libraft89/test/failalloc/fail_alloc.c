/* fail_alloc.c - --wrap allocation injector. */
#include <stddef.h>

#include "fail_alloc.h"

void *__real_malloc(size_t size);
void *__real_calloc(size_t count, size_t size);
void *__real_realloc(void *ptr, size_t size);
void __real_free(void *ptr);

static unsigned long alloc_count;
static unsigned long fail_index;
static int armed;
static unsigned long live;

void fail_alloc_begin(unsigned long index)
{
    alloc_count = 0ul;
    fail_index = index;
    armed = 1;
}

void fail_alloc_disable(void)
{
    armed = 0;
}

unsigned long fail_alloc_live(void)
{
    return live;
}

static int should_fail(void)
{
    unsigned long current;
    current = alloc_count;
    ++alloc_count;
    if (armed != 0 && current == fail_index)
    {
        return 1;
    }
    return 0;
}

void *__wrap_malloc(size_t size);
void *__wrap_malloc(size_t size)
{
    void *ptr;
    if (should_fail() != 0)
    {
        return NULL;
    }
    ptr = __real_malloc(size);
    if (ptr != NULL)
    {
        ++live;
    }
    return ptr;
}

void *__wrap_calloc(size_t count, size_t size);
void *__wrap_calloc(size_t count, size_t size)
{
    void *ptr;
    if (should_fail() != 0)
    {
        return NULL;
    }
    ptr = __real_calloc(count, size);
    if (ptr != NULL)
    {
        ++live;
    }
    return ptr;
}

void *__wrap_realloc(void *old_ptr, size_t size);
void *__wrap_realloc(void *old_ptr, size_t size)
{
    void *ptr;
    if (should_fail() != 0)
    {
        return NULL;
    }
    ptr = __real_realloc(old_ptr, size);
    if (ptr != NULL && old_ptr == NULL)
    {
        ++live;
    }
    return ptr;
}

void __wrap_free(void *ptr);
void __wrap_free(void *ptr)
{
    if (ptr != NULL)
    {
        --live;
    }
    __real_free(ptr);
}
