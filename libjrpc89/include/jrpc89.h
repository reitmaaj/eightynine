#ifndef JRPC89_H
#define JRPC89_H

#include <j89.h>

/* libjrpc89: a green-compliant JSON-RPC 2.0 protocol core in strict ISO C89.
 *
 * Builds and decodes request, notification, response, and error objects, and
 * classifies error codes. All JSON processing is delegated to libj89; the
 * caller owns the j89_arena memory.
 *
 * The protocol core declared here is ISO C89 and POSIX-free. The optional
 * transport profile in <jrpc89_io.h> is POSIX-specific.
 *
 * Scope (V1): request construction and decoding; notification construction
 * and decoding; response construction and decoding; string, integer, and
 * null ids; error-code classification. No batching.
 *
 * Out of scope: method dispatch, method registries, connections/listeners,
 * authentication/authorization, threading, event loops, retry policy,
 * request correlation tables, and JSON-RPC batching.
 *
 * A decoded response never carries JRPC89_ID_NONE; NONE is legal only for
 * request construction and decoding, where it means "notification". */

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
/* request decoding                                                    */
/* ------------------------------------------------------------------ */

typedef struct
{
    /* Borrowed bytes in arena a; never NULL after JRPC89_OK. */
    const char *method;
    j89_len method_len;

    /* J89_BAD when the params member is absent; otherwise an array or an
     * object node. */
    j89_len params;

    /* JRPC89_ID_NONE when the id member is absent (a notification);
     * otherwise an integer, string, or null id. */
    jrpc89_id id;
} jrpc89_request;

/* Decode one parsed JSON-RPC request or notification node:
 *
 *   the root must be an object;
 *   jsonrpc must exist and equal "2.0";
 *   method must exist and be a string;
 *   params must be absent, an array, or an object;
 *   id must be absent, an integer, a string, or null.
 *
 * Unknown additional object members are ignored. Duplicate object keys are
 * rejected by libj89's parser, so no duplicate protocol member is ever
 * observed here.
 *
 * Missing id:     out->id.kind == JRPC89_ID_NONE
 * Missing params: out->params == J89_BAD
 *
 * Borrowed strings and node indices remain valid while a lives.
 *
 * Precondition: a is clean; node is J89_BAD or a valid node index in a.
 * *out is unchanged on any non-OK return. */
jrpc89_status jrpc89_request_decode(j89_arena *a, j89_len node,
                                    jrpc89_request *out);

/* ------------------------------------------------------------------ */
/* response construction                                               */
/* ------------------------------------------------------------------ */

/* Build {"jsonrpc":"2.0","result":RESULT,"id":ID} in arena a and store the
 * object node in *out.
 *
 * result must name a valid JSON node and may contain any JSON value.
 * id must be an integer, a string, or null; JRPC89_ID_NONE is invalid for
 * responses.
 *
 * Precondition: a has no failed flag and no pending error message.
 * *out is unchanged on any non-OK return. */
jrpc89_status jrpc89_response_result_new(j89_arena *a, const jrpc89_id *id,
                                         j89_len result, j89_len *out);

/* Build
 *
 *   {
 *     "jsonrpc":"2.0",
 *     "error":{"code":CODE,"message":MESSAGE,"data":DATA},
 *     "id":ID
 *   }
 *
 * and store the object node in *out. The data member is omitted when
 * data == J89_BAD; otherwise data names a valid JSON node.
 *
 * code must be an exact integer in libj89's representable domain.
 * message may be NULL only when message_len is zero; embedded NUL bytes are
 * preserved by exact length.
 * id must be an integer, a string, or null; JRPC89_ID_NONE is invalid.
 *
 * Reserved and application-defined error codes are both accepted.
 *
 * Precondition: a has no failed flag and no pending error message.
 * *out is unchanged on any non-OK return. */
jrpc89_status jrpc89_response_error_new(j89_arena *a, const jrpc89_id *id,
                                        j89_int code, const char *message,
                                        j89_len message_len, j89_len data,
                                        j89_len *out);

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

#endif
