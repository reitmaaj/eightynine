#ifndef JRPC89_INTERNAL_H
#define JRPC89_INTERNAL_H

#include <jrpc89.h>

/* Report a message via the arena error buffer. Does not mark the arena
 * failed; callers that return J89_BAD are responsible for control flow. */
void jrpc89_set_error(j89_arena *a, const char *msg);

#endif
