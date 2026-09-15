#ifndef JRPC89_INTERNAL_H
#define JRPC89_INTERNAL_H

#include <jrpc89.h>

/* True when a node index refers to a real node (not J89_BAD). */
int jrpc89_has_node(j89_len n);

/* True when the arena is unusable for construction or decoding: it is
 * marked failed or carries a pending error message. */
int jrpc89_arena_dirty(j89_arena *a);

/* Map a libj89 builder failure to a jrpc89 status. The arena must have been
 * clean before the failing operation: a recorded diagnostic means invalid
 * input, otherwise the failure was an allocation. */
jrpc89_status jrpc89_status_from_arena(j89_arena *a);

#endif
