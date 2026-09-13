#ifndef CAT89_ALLOC_H
#define CAT89_ALLOC_H

#include <stddef.h>

/* cat89_allocator.h - injectable allocation for libcat89. */

typedef struct cat89_allocator
{
    void *ctx;

    void *(*alloc)(void *ctx, size_t size);

    void *(*realloc)(void *ctx, void *ptr, size_t size);

    void (*free)(void *ctx, void *ptr);
} cat89_allocator;

/* Return the process-default allocator (malloc/free based). */
const cat89_allocator *cat89_allocator_default(void);

#endif
