#include <limits.h>
#include <stddef.h>

/* libbwt89 - Burrows-Wheeler transform variants in strict ISO C89.
 *
 * Caller-owned, length-prefixed byte buffers throughout. The input and output
 * data are supplied as explicit length + pointer pairs, so binary data
 * including embedded NUL bytes is fully supported.
 *
 * Naming: bwt = regular forward, ibwt = regular inverse, bbwt = bijective
 * forward, ibbwt = bijective inverse. Regular forward reports the 0-based
 * row index of the original text so that ibwt can recover it without adding
 * any sentinel byte to the output.
 *
 * Length bound. Every transform works on an internal alphabet of size at
 * most INT_MAX + 1 symbols held in C `int` arrays, so public lengths are
 * capped at BWT89_MAX_N. Any call with n > BWT89_MAX_N returns BWT89_TOO_LARGE
 * before the input is read or any scratch is allocated. The shared cap leaves
 * one symbol of headroom below INT_MAX so no internal int arithmetic can
 * overflow on any accepted input.
 *
 * Aliasing. Exact in-place operation (passing the same buffer for `out` and
 * the input `s` / `b`) is supported and yields the same result as disjoint
 * buffers for every transform and inverse. No other overlapping input/output
 * ranges are supported.
 *
 * Allocation. Transforms may allocate O(n) temporary scratch internally and
 * report BWT89_NOMEM on failure; a failed call leaves caller output buffers
 * unmodified.
 */

#ifndef BWT89_H
#define BWT89_H

#define BWT89_MAX_N ((size_t)INT_MAX - 1)

enum bwt89_status
{
    BWT89_OK = 0,
    BWT89_NULL_ARG,
    BWT89_NOMEM,
    BWT89_BAD_INDEX,
    BWT89_BAD_EDIT,
    BWT89_TOO_LARGE,
    BWT89_BAD_DATA
};

/* Regular forward transform. Writes exactly n bytes to `out` and stores the
 * 0-based index of the row holding the original text in `*index`.
 * Supports exact in-place operation (out == s). */
enum bwt89_status bwt89_bwt(const unsigned char *s, size_t n, size_t *index,
                            unsigned char *out);

/* Regular inverse transform. Recovers the n original bytes from `b`, the
 * transformed bytes, using `index` as reported by bwt89_bwt.
 * An index outside [0, n] is BWT89_BAD_INDEX. If (b, index) is not a valid
 * transform (the LF walk does not consume exactly n real symbols before the
 * sentinel) the call returns BWT89_BAD_DATA and leaves `out` unmodified.
 * Supports exact in-place operation (out == b). */
enum bwt89_status bwt89_ibwt(const unsigned char *b, size_t n, size_t index,
                             unsigned char *out);

/* Bijective forward transform. Writes exactly n bytes to `out`; no index is
 * needed because the map is a permutation of the input.
 * Supports exact in-place operation (out == s). */
enum bwt89_status bwt89_bbwt(const unsigned char *s, size_t n,
                             unsigned char *out);

/* Bijective inverse transform. Recovers the n original bytes from `b`.
 * Supports exact in-place operation (out == b). */
enum bwt89_status bwt89_ibbwt(const unsigned char *b, size_t n,
                              unsigned char *out);

/* --- Dynamic (Salson incremental-edit) module ---------------------------
 *
 * Maintains a Burrows-Wheeler transform that is updated under single-character
 * text edits (insert / delete / substitute) without a full recompute. The
 * handle owns its state; see .agent/design/0002 for the intended algorithm.
 * Implemented in src/bwt89_dyn.c (not yet built on main).
 *
 * Positions range over [0, len]: insert may target pos == len (append);
 * delete and substitute require pos < len; delete on an empty handle is
 * BWT89_BAD_EDIT. */

struct bwt89_ed;

/* Open a handle on the initial text s[0..n-1] (copied by the library).
 * BWT89_NOMEM if the handle cannot be allocated. */
enum bwt89_status bwt89_ed_open(struct bwt89_ed **ed, const unsigned char *s,
                                size_t n);

/* Insert c before position pos (pos == len appends). */
enum bwt89_status bwt89_ed_insert(struct bwt89_ed *ed, size_t pos,
                                  unsigned char c);

/* Delete the byte at position pos. */
enum bwt89_status bwt89_ed_delete(struct bwt89_ed *ed, size_t pos);

/* Replace the byte at position pos with c (delete then insert). */
enum bwt89_status bwt89_ed_substitute(struct bwt89_ed *ed, size_t pos,
                                      unsigned char c);

/* Report the current text length into *len. */
enum bwt89_status bwt89_ed_length(const struct bwt89_ed *ed, size_t *len);

/* Write the current transform bytes to `out` and store the original-row
 * index in *index, matching a fresh bwt89_bwt of the current text. */
enum bwt89_status bwt89_ed_bwt(const struct bwt89_ed *ed, size_t *index,
                               unsigned char *out);

/* Release all storage owned by the handle. */
enum bwt89_status bwt89_ed_close(struct bwt89_ed *ed);

#endif /* BWT89_H */
