#ifndef JRPC89_ID_H
#define JRPC89_ID_H

#include <jrpc89.h>

/* Build the JSON node (integer, string, or null) for an id. Undefined for
 * JRPC89_ID_NONE; returns J89_BAD on failure. */
j89_len jrpc89_id_node(j89_arena *a, const jrpc89_id *id);

#endif
