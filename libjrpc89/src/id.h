#ifndef JRPC89_ID_H
#define JRPC89_ID_H

#include <jrpc89.h>

/* True when the id is present (not a notification). */
int jrpc89_id_present(const jrpc89_id *id);

/* True when v is an exact integer in libj89's representable domain. */
int jrpc89_int_exact(j89_int v);

/* True when the id is a usable id value: a known kind, with a non-NULL
 * string pointer for string ids and an exact integer for integer ids.
 * JRPC89_ID_NONE is valid here (a notification). */
int jrpc89_id_valid(const jrpc89_id *id);

/* Build the JSON node (integer, string, or null) for an id. Undefined for
 * JRPC89_ID_NONE; returns J89_BAD on failure. */
j89_len jrpc89_id_node(j89_arena *a, const jrpc89_id *id);

#endif
