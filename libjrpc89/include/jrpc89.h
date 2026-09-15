#ifndef JRPC89_H
#define JRPC89_H

#include <j89.h>

/* libjrpc89: a green-compliant JSON-RPC 2.0 client in strict ISO C89.
 *
 * Builds request and notification objects, decodes responses into a checked
 * result-or-error view, and classifies error codes. All JSON processing is
 * delegated to libj89; the caller owns the j89_arena memory.
 *
 * The protocol core declared here is ISO C89 and POSIX-free. The optional
 * transport profile in <jrpc89_io.h> is POSIX-specific.
 *
 * Scope (V1): request, notification, response, and error objects; string,
 * integer, and null ids; notifications omit the id member. No batching.
 *
 * A decoded response never carries JRPC89_ID_NONE; NONE is legal only for
 * request construction, where it means "notification".
 */

/* ------------------------------------------------------------------ */
/* status                                                              */
/* ------------------------------------------------------------------ */

typedef enum
{
    JRPC89_OK = 0,
    JRPC89_EINVAL,   /* bad argument or precondition */
    JRPC89_ENOMEM,   /* allocation failure in libj89 */
    JRPC89_EPROTO,   /* structurally invalid JSON-RPC message */
    JRPC89_EOF,      /* transport: EOF before any frame byte */
    JRPC89_ETOOLONG, /* transport: frame exceeds cap-1 */
    JRPC89_ETRUNC,   /* transport: EOF inside a frame */
    JRPC89_EIO       /* transport: syscall failure */
} jrpc89_status;

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

/* Structural id equality: same kind and exact value or bytes. Total: NULL
 * operands compare unequal; NONE equals NONE. String comparison uses the
 * reported lengths, so embedded NUL bytes compare correctly. */
int jrpc89_id_equal(const jrpc89_id *x, const jrpc89_id *y);

/* ------------------------------------------------------------------ */
/* request building                                                    */
/* ------------------------------------------------------------------ */

/* Build a request object {jsonrpc, method, params?, id?} node in arena a
 * and store the object node in *out. The method is given as bytes plus a
 * length, so embedded NUL bytes are preserved and a zero-length method is
 * allowed; method may be NULL only when method_len is zero. params must be
 * J89_BAD to omit the member, or an array/object node. id must be non-NULL;
 * kind JRPC89_ID_NONE builds a notification without an id member.
 *
 * Precondition: a has no failed flag and no pending error message.
 * *out is unchanged on any non-OK return. */
jrpc89_status jrpc89_request_new(j89_arena *a, const char *method,
                                 j89_len method_len, j89_len params,
                                 const jrpc89_id *id, j89_len *out);

/* ------------------------------------------------------------------ */
/* response decoding                                                   */
/* ------------------------------------------------------------------ */

typedef enum
{
    JRPC89_RESPONSE_RESULT,
    JRPC89_RESPONSE_ERROR
} jrpc89_response_kind;

typedef struct
{
    j89_int code;
    const char *message; /* never NULL after JRPC89_OK */
    j89_len message_len; /* embedded NUL bytes allowed */
    j89_len data;        /* J89_BAD when absent */
} jrpc89_error;

typedef struct
{
    jrpc89_response_kind kind;
    jrpc89_id id;       /* never JRPC89_ID_NONE after decode */
    j89_len result;     /* J89_BAD when kind == JRPC89_RESPONSE_ERROR */
    jrpc89_error error; /* zeroed when kind == JRPC89_RESPONSE_RESULT */
} jrpc89_response;

/* Decode one parsed response node: jsonrpc must be "2.0", exactly one of
 * result or error must be present, the error object must carry an integer
 * code and a string message, and the id must be an integer, string, or
 * null.
 *
 * Invariant: when JRPC89_OK is returned, every field permitted by
 * out->kind can be used without further structural checks:
 *   RESULT: result != J89_BAD; error is zeroed
 *   ERROR:  result == J89_BAD; error.message != NULL; error.data is a node
 *           or J89_BAD
 * Node indices and string bytes remain valid while a lives.
 *
 * Precondition: a is clean; node is J89_BAD or a valid node index in a.
 * *out is unchanged on any non-OK return. */
jrpc89_status jrpc89_response_decode(j89_arena *a, j89_len node,
                                     jrpc89_response *out);

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
    JRPC89_RESERVED_MIN = -32768,
    JRPC89_RESERVED_MAX = -32000
};

/* True for codes in the reserved range -32768..-32000, including the gaps
 * between the named codes; false for application-defined codes. */
int jrpc89_error_code_reserved(j89_int code);

/* ------------------------------------------------------------------ */
/* framing (POSIX; moves to <jrpc89_io.h>)                             */
/* ------------------------------------------------------------------ */

/* Write one frame: len bytes plus a trailing newline. Rejects a negative
 * fd, a NULL buffer, a zero-length frame, and any raw '\n' in json with
 * JRPC89_EINVAL, writing nothing. Returns JRPC89_OK on success and
 * JRPC89_EIO on a write failure. Once any byte reaches fd, a later failure
 * can leave a partial frame; the stream cannot be rolled back. The caller
 * must configure its SIGPIPE policy to observe JRPC89_EIO instead of
 * process termination. */
jrpc89_status jrpc89_write_frame(int fd, const char *json, j89_len len);

/* Read one newline-terminated NDJSON frame into buf (capacity cap bytes).
 * On JRPC89_OK the frame bytes (without the newline) are in buf,
 * NUL-terminated, with *out_len set. A payload of up to cap-1 bytes is
 * accepted.
 *
 * On every non-OK return *out_len is 0 and buf[0] is '\0':
 *   JRPC89_EINVAL   NULL buffer or length pointer, or cap == 0 (untouched)
 *   JRPC89_EOF      EOF before any frame byte
 *   JRPC89_ETRUNC   EOF after payload bytes without a newline
 *   JRPC89_ETOOLONG frame exceeds cap-1; the reader drains through the next
 *                   newline so the next call starts on a frame boundary
 *   JRPC89_EIO      read failure */
jrpc89_status jrpc89_read_frame(int fd, char *buf, j89_len cap,
                                j89_len *out_len);

#endif
