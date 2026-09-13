#ifndef J89_ALG_BRIDGE_H
#define J89_ALG_BRIDGE_H

#include "j89.h"
#include "j89_alg.h"

/* j89_alg_bridge.h - import the practical j89 document model into j89_alg.
 *
 * This is a SEPARATE, non-core layer. It converts a parsed/built j89.h
 * document (arena node tree) into an owned j89_alg JSON value:
 *
 *   J89_NULL/FALSE/TRUE   -> Null / Boolean
 *   J89_INTEGER/FLOAT     -> Number (double carrier)
 *   J89_STRING            -> String (byte-string carrier; embedded NUL kept)
 *   J89_ARRAY             -> Array, order preserved
 *   J89_OBJECT            -> Object bag (practical names are unique, so the
 *                            unique-name subset embeds naturally as a bag)
 *
 * The reverse direction (algebraic bag -> ordered j89 object) is intentionally
 * absent: converting a bag to an ordered JSON object needs an ordering policy
 * and belongs to a caller-provided adapter, not to this import.
 *
 * `root` is a j89 node index valid in `src`. On success *out is an owned
 * j89_alg value the caller must release. On failure *out is set to NULL and
 * the returned status is J89A_NOMEM (or a J89A_CALLBACK from the allocator).
 */

j89a_status j89a_import_j89(j89_arena *src, j89_len root, j89a_json **out,
                            j89a_alloc *al);

#endif
