#ifndef JRPC89_H
#define JRPC89_H

#include <j89.h>

/* libjrpc89: a green-compliant JSON-RPC 2.0 client in strict ISO C89.
 *
 * Builds request and notification objects, parses and validates response
 * and error objects, and moves newline-delimited JSON frames over an
 * already-open Unix socket file descriptor. All JSON processing is delegated
 * to libj89; the caller owns the j89_arena memory.
 *
 * Transport contract: the library does not connect, accept, or close
 * sockets. The caller provides an open int fd (AF_UNIX, SOCK_STREAM). The
 * wire framing is newline-delimited JSON (NDJSON): one message per line.
 *
 * Scope (V1): request, notification, response, and error objects; string,
 * integer, and null ids echoed back; notifications omit the id member. No
 * batching.
 */

/* ------------------------------------------------------------------ */
/* general utility                                                     */
/* ------------------------------------------------------------------ */

/* True when a node index refers to a real node (not J89_BAD). Useful for
 * checking the results of accessors that return J89_BAD on absence. */
int jrpc89_has_node(j89_len n);

/* ------------------------------------------------------------------ */
/* ids                                                                 */
/* ------------------------------------------------------------------ */

typedef enum
{
    JRPC89_ID_NONE,
    JRPC89_ID_INT,
    JRPC89_ID_STRING,
    JRPC89_ID_NULL
} jrpc89_id_kind;

typedef struct
{
    jrpc89_id_kind kind;
    j89_int num;     /* value when kind == JRPC89_ID_INT */
    const char *str; /* bytes when kind == JRPC89_ID_STRING */
    j89_len len;     /* byte length when kind == JRPC89_ID_STRING */
} jrpc89_id;

/* True when the id is present in a request (not a notification). */
int jrpc89_id_present(const jrpc89_id *id);

/* True when two ids are structurally equal (same kind and value). */
int jrpc89_id_matches(const jrpc89_id *a, const jrpc89_id *b);

/* ------------------------------------------------------------------ */
/* request building                                                    */
/* ------------------------------------------------------------------ */

/* Build a request object {jsonrpc, method, params?, id?} node. Pass
 * J89_BAD as params to omit the params member; pass an id whose kind is
 * JRPC89_ID_NONE (or NULL) to build a notification without an id member.
 * Returns the object node, or J89_BAD on failure (NULL, empty, or NUL
 * method, or an allocation failure reported via j89_error). */
j89_len jrpc89_request_new(j89_arena *a, const char *method, j89_len params,
                           const jrpc89_id *id);

/* ------------------------------------------------------------------ */
/* response parsing                                                    */
/* ------------------------------------------------------------------ */

/* Validate a parsed response node: jsonrpc must be "2.0", exactly one of
 * result or error must be present, and an id member must be present.
 * Returns 0 when valid, nonzero otherwise (message via j89_error). */
int jrpc89_response_validate(j89_arena *a, j89_len resp);

/* True when the response carries an error member. */
int jrpc89_is_error(j89_arena *a, j89_len resp);

/* The result node of a response, or J89_BAD when absent. */
j89_len jrpc89_result_node(j89_arena *a, j89_len resp);

/* Extract the id of a response into out. Returns 0 on success, nonzero if
 * the response has no id member. */
int jrpc89_id_of_response(j89_arena *a, j89_len resp, jrpc89_id *out);

/* ------------------------------------------------------------------ */
/* errors                                                              */
/* ------------------------------------------------------------------ */

enum
{
    JRPC89_PARSE_ERROR = -32700,
    JRPC89_INVALID_REQUEST = -32600,
    JRPC89_METHOD_NOT_FOUND = -32601,
    JRPC89_INVALID_PARAMS = -32602,
    JRPC89_INTERNAL_ERROR = -32603,
    JRPC89_SERVER_ERROR_MIN = -32099,
    JRPC89_SERVER_ERROR_MAX = -32000
};

/* True for codes in the reserved ranges (-32000..-32099 and the standard
 * codes); false for application-defined codes. */
int jrpc89_error_is_reserved(int code);

/* The error object node of a response, or J89_BAD when absent. */
j89_len jrpc89_error_object(j89_arena *a, j89_len resp);

/* Members of the error object. Returns are undefined when the response has
 * no error object. */
int jrpc89_error_code(j89_arena *a, j89_len resp);
const char *jrpc89_error_message(j89_arena *a, j89_len resp);
j89_len jrpc89_error_message_length(j89_arena *a, j89_len resp);
j89_len jrpc89_error_data(j89_arena *a, j89_len resp);

/* ------------------------------------------------------------------ */
/* framing                                                             */
/* ------------------------------------------------------------------ */

/* Write json bytes plus a trailing newline as one NDJSON frame. Returns 0
 * on success, nonzero on a write error. */
int jrpc89_write_frame(int fd, const char *json, j89_len len);

/* Read one newline-terminated NDJSON frame into buf (capacity cap bytes).
 * On success the frame bytes (without the newline) are written to buf,
 * NUL-terminated, *out_len is set, and 0 is returned. A payload of up to
 * cap-1 bytes is accepted.
 *
 * Nonzero returns:
 *   -1  EOF with no data read
 *   -2  frame too long for the buffer (a payload byte arrived with no room)
 *   -3  a read error
 *   -4  truncated frame (peer closed before a newline, after partial data) */
int jrpc89_read_frame(int fd, char *buf, j89_len cap, j89_len *out_len);

#endif
