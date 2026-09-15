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

/* True when a libj89 builder operation has failed the arena. */
int jrpc89_builder_failed(j89_arena *a);

/* Return JRPC89_OK when the arena is still clean, or the mapped builder
 * failure status otherwise. */
jrpc89_status jrpc89_builder_check(j89_arena *a);

/* True when a string node holds exactly expect_len bytes of expect. */
int jrpc89_str_is(j89_arena *a, j89_len node, const char *expect,
                  j89_len expect_len);

/* True when an object node carries a member named key. */
int jrpc89_member_has(j89_arena *a, j89_len node, const char *key);

/* Convert one node to an id. Returns JRPC89_OK for integer, string, or
 * null, and JRPC89_EPROTO for every other node kind. */
jrpc89_status jrpc89_id_from_node(j89_arena *a, j89_len node, jrpc89_id *out);

/* Check the fixed parts of a request or response: object root and a "2.0"
 * version member. */
jrpc89_status jrpc89_check_head(j89_arena *a, j89_len node);

/* Set the jsonrpc member at slot. */
jrpc89_status jrpc89_set_version(j89_arena *a, j89_len obj, j89_len slot);

/* Set the id member at slot for a present id. */
jrpc89_status jrpc89_set_id_member(j89_arena *a, j89_len obj, j89_len slot,
                                   const jrpc89_id *id);

#endif
