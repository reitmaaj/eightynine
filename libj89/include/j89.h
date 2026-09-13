#ifndef J89_H
#define J89_H

#include <stddef.h>

/* libj89: a minimal, strict-C89 JSON parser and builder.
 *
 * Subset: object, array, string (escapes, \uXXXX, UTF-8), integer,
 * fractional/exponent number (FLOAT), true, false, null.
 *
 * String values and object keys stored in a j89_arena are always well-formed
 * UTF-8. This invariant covers both parsed input and values created through
 * the public builder API.
 *
 * Parsing validates raw UTF-8 and JSON \uXXXX escapes. A UTF-16 high
 * surrogate in a \u escape must be followed immediately by a second \u escape
 * containing a low surrogate; lone or malformed surrogates are rejected.
 *
 * The builder validates bytes supplied to j89_string_new() and object keys
 * supplied to j89_object_set(). Invalid UTF-8 is rejected before any bytes are
 * stored.
 *
 * JSON strings are not normalized, case-folded, or otherwise Unicode-
 * transformed. Valid UTF-8 byte sequences are preserved exactly as supplied,
 * except that JSON escape notation is decoded to the UTF-8 bytes it denotes.
 *
 * A single leading UTF-8 BOM in parser input is ignored. Duplicate object keys
 * are rejected by the parser.
 *
 * All numbers are held in IEEE-754 binary64 (`double`), exact for every
 * integer |n| <= 2^53. An integer token whose magnitude exceeds 2^53 is
 * rejected (rather than approximated), so every accepted integer is exact.
 * This range/precision limit is permitted by RFC 8259 section 6, which
 * expects implementations to provide no more precision than `double`.
 *
 * Numbers with a fraction or exponent are held in a `double` as well; a
 * number whose magnitude exceeds DBL_MAX is rejected (no infinity/NaN).
 *
 * Nesting of arrays/objects is limited to J89_MAX_DEPTH to protect the
 * recursive-descent parser from exhausting the call stack. Input nested
 * deeper is rejected with an error rather than terminating the process.
 */

#define J89_MAX_DEPTH 10000

typedef double
    j89_int; /* integer value; exact for |n| <= 2^53, else rejected */
typedef size_t j89_len; /* lengths and node indices */

typedef enum j89_kind
{
    J89_NULL = 0,
    J89_FALSE,
    J89_TRUE,
    J89_INTEGER,
    J89_FLOAT,
    J89_STRING,
    J89_ARRAY,
    J89_OBJECT
} j89_kind;

#define J89_BAD ((j89_len) - 1)

#define J89_ERR_LEN 128

typedef struct j89_arena
{
    void *mem;             /* growable block */
    j89_len cap;           /* bytes allocated */
    j89_len off;           /* next free byte */
    char err[J89_ERR_LEN]; /* last error message, "" if none */
    int failed;            /* allocation/parse failed */
} j89_arena;

void j89_arena_init(j89_arena *a);
void j89_arena_destroy(j89_arena *a);
const char *j89_error(j89_arena *a);
int j89_failed(j89_arena *a);

/* Parse `len` bytes of `buf`. Returns the root node index, or J89_BAD on
 * a syntax error (message recorded via j89_error). */
j89_len j89_parse(const char *buf, j89_len len, j89_arena *a);

/* Node accessors. */
j89_kind j89_kind_of(j89_arena *a, j89_len node);
j89_int j89_int_value(j89_arena *a, j89_len node);
double j89_double_value(j89_arena *a, j89_len node);
/* Returned string bytes and object keys contain well-formed UTF-8 and are
 * NUL-terminated immediately after their reported byte length.
 *
 * A string or key may contain embedded NUL bytes; callers must use
 * j89_string_length / j89_object_key_length rather than strlen(). */
const char *j89_string_value(j89_arena *a, j89_len node);
j89_len j89_string_length(j89_arena *a, j89_len node);
j89_len j89_array_length(j89_arena *a, j89_len node);
j89_len j89_array_get(j89_arena *a, j89_len node, j89_len i);
j89_len j89_object_length(j89_arena *a, j89_len node);
const char *j89_object_key(j89_arena *a, j89_len node, j89_len i);
j89_len j89_object_key_length(j89_arena *a, j89_len node, j89_len i);
j89_len j89_object_value(j89_arena *a, j89_len node, j89_len i);
j89_len j89_object_find(j89_arena *a, j89_len node, const char *key);

/* Render node (or just value) as JSON text into the growable `out`
 * arena. `compact` selects minimal output. Returns 0 on success.
 * Rendering a tree nested beyond J89_MAX_DEPTH fails cleanly with a
 * nonzero return (no stack overflow) and a message via j89_error. */
int j89_render(j89_arena *a, j89_len node, int compact, j89_arena *out);

/* ------------------------------------------------------------------ */
/* Builder API (construction).                                         */
/*                                                                     */
/* Build a JSON tree in a single arena `a`, then emit it with          */
/* j89_render into a separate output arena. `count` is the fixed       */
/* number of members (object) or elements (array); each slot MUST be   */
/* assigned exactly once via j89_object_set / j89_array_set before     */
/* rendering. All allocations share the arena and are released by      */
/* j89_arena_destroy.                                                  */
/*                                                                     */
/* String values and object keys supplied through this API must        */
/* contain well-formed UTF-8; the builder validates them and never     */
/* stores malformed UTF-8. Validation happens before allocation, so    */
/* rejected input does not consume arena storage.                      */
/*                                                                     */
/* On allocation or validation failure the arena is marked failed and  */
/* a message is recorded via j89_error. Once failed, callers must not  */
/* treat later builder results as valid.                               */

j89_len j89_object_new(j89_arena *a, j89_len count);
j89_len j89_array_new(j89_arena *a, j89_len count);

/* Create a JSON string from bytes[0..len).
 *
 * The byte sequence may contain embedded NUL bytes but must contain
 * well-formed UTF-8.
 *
 * On success, returns a string node. The stored byte sequence has exactly len
 * data bytes followed internally by an additional NUL terminator.
 *
 * On malformed UTF-8 or allocation failure, returns J89_BAD, marks the arena
 * failed, and records a message via j89_error. UTF-8 validation happens before
 * allocation, so malformed input does not consume arena storage. */
j89_len j89_string_new(j89_arena *a, const char *bytes, j89_len len);

j89_len j89_integer_new(j89_arena *a, j89_int v);
j89_len j89_double_new(j89_arena *a, double v);
j89_len j89_bool_new(j89_arena *a, int v);

/* Assign child to one array slot. */
void j89_array_set(j89_arena *a, j89_len array, j89_len index, j89_len child);

/* Assign one object member.
 *
 * key[0..keylen) may contain embedded NUL bytes but must contain well-formed
 * UTF-8.
 *
 * The key is validated before allocation or modification of the object slot.
 * On malformed UTF-8 or allocation failure, the arena is marked failed and the
 * target slot is left unchanged.
 *
 * The value node is borrowed from the same arena; ownership remains with the
 * arena. */
void j89_object_set(j89_arena *a, j89_len object, j89_len index,
                    const char *key, j89_len keylen, j89_len value);

#endif
