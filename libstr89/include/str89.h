#ifndef STR89_H
#define STR89_H

#include <stddef.h>

#include <u89.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* libstr89 - owning, validated UTF-8 strings for strict C89.
 *
 * A str89 string is an exact sequence of Unicode scalar values encoded as
 * well-formed UTF-8, preserving the original UTF-8 byte representation. Each
 * scalar has one canonical encoding, so preserving the bytes preserves the
 * scalar sequence exactly.
 *
 * Consequences: byte length is explicit; embedded U+0000 is an ordinary
 * scalar; there is no strlen() semantics and no required trailing NUL; no
 * normalization, case folding, locale, or collation is applied; equality is
 * byte equality; offsets are UTF-8 byte offsets; operations that split text
 * require a Unicode scalar boundary. Grapheme boundaries remain a libu89
 * concern.
 *
 * Three types:
 *   str89_view  borrowed validated UTF-8 bytes
 *   str89       owning finalized UTF-8 bytes
 *   str89_buf   owning mutable UTF-8 builder
 *
 * All Unicode facts (UTF-8 validity, scalar validity, encoding, boundaries)
 * come from libu89. libstr89 contains no Unicode tables and no UTF-8 codec.
 */

/* Offset returned by str89_view_find when the needle is absent. */
#define STR89_NPOS ((size_t)-1)

    typedef enum str89_status
    {
        STR89_OK = 0,
        STR89_EINVAL = -1,
        STR89_ENOMEM = -2,
        STR89_EUTF8 = -3,
        STR89_ERANGE = -4,
        STR89_EBOUND = -5
    } str89_status;

    /* ---- Allocator ----------------------------------------------------------
     *
     * Owning values allocate through the caller-supplied allocator and never
     * store it. A NULL allocator means the C library malloc/realloc/free. The
     * allocator used to destroy a value MUST be the allocator that allocated
     * it. A failed realloc leaves the original block owned by the value
     * unchanged.
     */

    typedef void *(*str89_malloc_fn)(void *ctx, size_t size);
    typedef void *(*str89_realloc_fn)(void *ctx, void *ptr, size_t size);
    typedef void (*str89_free_fn)(void *ctx, void *ptr);

    typedef struct str89_alloc
    {
        void *ctx;
        str89_malloc_fn malloc_fn;
        str89_realloc_fn realloc_fn;
        str89_free_fn free_fn;
    } str89_alloc;

    /* ---- Types
     * --------------------------------------------------------------- */

    typedef struct str89_view
    {
        const unsigned char *data;
        size_t len;
    } str89_view;

    typedef struct str89
    {
        unsigned char *data;
        size_t len;
    } str89;

    typedef struct str89_buf
    {
        unsigned char *data;
        size_t len;
        size_t cap;
    } str89_buf;

    /* ---- Views
     * ---------------------------------------------------------------
     *
     * str89_view_init is the validation boundary: it validates the complete
     * byte sequence with libu89. After success, a view means validated Unicode
     * text. A view fabricated directly by a caller is the caller's
     * responsibility.
     *
     *   data == NULL && len == 0   -> empty view, STR89_OK
     *   data == NULL && len != 0   -> STR89_EINVAL
     *   malformed UTF-8            -> STR89_EUTF8
     *
     * On failure *out is unchanged.
     */

    int str89_view_init(str89_view *out, const unsigned char *data, size_t len);

    /* Borrowed views of owning values. */
    str89_view str89_view_of(const str89 *s);
    str89_view str89_buf_view(const str89_buf *s);

    /* 1 when offset is a scalar boundary of s. Out-of-range offsets return 0
     * and read no memory. */
    int str89_view_is_boundary(str89_view s, size_t offset);

    /* Subview [offset, offset + len). Both ends must be scalar boundaries.
     * Returns STR89_ERANGE for an out-of-range range and STR89_EBOUND for a
     * split scalar. On failure *out is unchanged. */
    int str89_view_sub(str89_view s, size_t offset, size_t len,
                       str89_view *out);

    /* Exact byte equality: no normalization, no case folding, no collation. */
    int str89_view_equal(str89_view a, str89_view b);

    /* Unsigned-byte lexicographical comparison. Returns a negative value, 0, or
     * a positive value; only the sign is meaningful. */
    int str89_view_compare(str89_view a, str89_view b);

    /* First occurrence of needle in haystack at or after byte offset `from`.
     * `from` must be <= haystack.len and on a scalar boundary. On STR89_OK the
     * match offset is written to *offset, or STR89_NPOS when absent. An empty
     * needle matches at `from`. */
    int str89_view_find(str89_view haystack, str89_view needle, size_t from,
                        size_t *offset);

    /* ---- Owning strings
     * ------------------------------------------------------ */

    void str89_init(str89 *s);

    /* Copy a validated view into a fresh allocation. `*out` must be an
     * initialized empty string. Empty input performs no allocation. On failure
     * *out is unchanged. */
    int str89_from_view(str89 *out, const str89_alloc *alloc, str89_view src);

    /* Copy an owning string. `*out` must be an initialized empty string;
     * copying a string onto itself is a no-op. */
    int str89_copy(str89 *out, const str89_alloc *alloc, const str89 *src);

    /* Release the block and reset to the empty representation. Repeated calls
     * are safe. The allocator must match the one used to allocate. */
    void str89_free(str89 *s, const str89_alloc *alloc);

    /* ---- Mutable strings
     * ----------------------------------------------------- */

    void str89_buf_init(str89_buf *s);

    /* Set len = 0; the allocation is retained. */
    void str89_buf_clear(str89_buf *s);

    /* Release the block and reset to {NULL, 0, 0}. Repeated calls are safe. */
    void str89_buf_free(str89_buf *s, const str89_alloc *alloc);

    /* Ensure cap >= capacity. Never changes bytes or len, never shrinks, and
     * leaves the buffer unchanged on failure. */
    int str89_buf_reserve(str89_buf *s, const str89_alloc *alloc,
                          size_t capacity);

    /* Replace the contents with src. */
    int str89_buf_set(str89_buf *s, const str89_alloc *alloc, str89_view src);

    /* Append src. */
    int str89_buf_append(str89_buf *s, const str89_alloc *alloc,
                         str89_view src);

    /* Insert src at byte offset `offset`, which must be a scalar boundary. */
    int str89_buf_insert(str89_buf *s, const str89_alloc *alloc, size_t offset,
                         str89_view src);

    /* Remove [offset, offset + len). Both ends must be scalar boundaries. */
    int str89_buf_erase(str89_buf *s, size_t offset, size_t len);

    /* Replace [offset, offset + len) with src. Both ends must be scalar
     * boundaries. */
    int str89_buf_replace(str89_buf *s, const str89_alloc *alloc, size_t offset,
                          size_t len, str89_view src);

    /* Append or insert one Unicode scalar value. A non-scalar returns
     * STR89_EINVAL. */
    int str89_buf_append_cp(str89_buf *s, const str89_alloc *alloc, u89_cp cp);
    int str89_buf_insert_cp(str89_buf *s, const str89_alloc *alloc,
                            size_t offset, u89_cp cp);

    /* Transfer builder ownership into a finalized string. `out` must be an
     * initialized empty string; `src` becomes an initialized empty buffer. No
     * allocation, copying, or allocator call occurs. */
    int str89_take(str89 *out, str89_buf *src);

#ifdef __cplusplus
}
#endif

#endif
