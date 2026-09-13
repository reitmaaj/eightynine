#ifndef U89_H
#define U89_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/* Unicode data version pinned by this build (Unicode 17.0.0). */
#define U89_UNICODE_MAJOR 17
#define U89_UNICODE_MINOR 0
#define U89_UNICODE_PATCH 0

/* Unicode scalar value. Any code point except the surrogate range
   U+D800..U+DFFF. Range U+0000..U+D7FF and U+E000..U+10FFFF. */
typedef unsigned int u89_cp;   /* 32-bit Unicode scalar value */

/* ---- Version ------------------------------------------------------------- */

/* Return "MAJOR.MINOR.PATCH" of the pinned Unicode data (immutable static
   ASCII). Caller must not free or modify the returned pointer. */
const char *u89_unicode_version(void);

/* ---- Scalar semantics ---------------------------------------------------- */

int u89_is_scalar(u89_cp cp);

/* ---- Status codes --------------------------------------------------------- */

/* Status of a positional Unicode operation. U89_OK is 0; every error code is
   negative. U89_EINVAL, U89_EUTF8, U89_ENOSPC, U89_EWORK and U89_EOVERFLOW
   keep the values they had as object-like macros; U89_ERANGE is new. */
typedef enum u89_status {
    U89_OK = 0,
    U89_EINVAL = -1,
    U89_EUTF8 = -2,
    U89_ENOSPC = -3,
    U89_EWORK = -4,
    U89_EOVERFLOW = -5,
    U89_ERANGE = -6
} u89_status;

/* ---- UTF-8 validation ---------------------------------------------------- */

/* Validate that bytes [s, s+n) are well-formed UTF-8: rejects overlong
   encodings, UTF-8-encoded surrogates, code points above U+10FFFF, and
   truncated sequences. Returns 1 if valid, 0 otherwise. NUL is valid. */
int u89_utf8_valid(const unsigned char *s, size_t n);

/* Length in bytes of the UTF-8 sequence encoding cp, or 0 if cp is not a
   scalar. */
int u89_utf8_len(u89_cp cp);

/* Expected byte length (1..4) of the UTF-8 sequence introduced by lead, or 0
   when lead cannot begin a sequence (continuation byte, overlong lead C0/C1,
   or above F4). Continuation validity and overlong/surrogate values are
   still decided by u89_utf8_decode/u89_utf8_valid. */
int u89_utf8_seq_len(unsigned char lead);

/* Encode cp into out (must hold u89_utf8_len(cp) bytes). Returns bytes
   written, or 0 if cp is not a scalar. */
int u89_utf8_encode(u89_cp cp, unsigned char *out);

/* ---- UTF-8 iteration ----------------------------------------------------- */

typedef struct u89_iter {
    const unsigned char *cur;
    const unsigned char *end;
    u89_cp cp;            /* last decoded scalar (valid when err==0) */
    int err;              /* 0 while ok; 1 on malformed input; 2 at end */
    int leading;          /* bytes consumed by the last scalar */
} u89_iter;

/* Initialize an iterator over [s, s+n). */
void u89_iter_init(u89_iter *it, const unsigned char *s, size_t n);

/* Advance to the next scalar. Sets err=2 at end of input, err=1 on
   malformed input (and stops; recovery to U+FFFD is deliberately not
   provided). Returns 0 if a scalar was decoded, nonzero otherwise. */
int u89_iter_next(u89_iter *it);

/* ---- Extended grapheme clusters (UAX #29) -------------------------------- */

/* The input must be valid UTF-8 and pos <= n. Boundaries follow the pinned
   Unicode release (GCB, InCB, Extended_Pictographic). next() returns the
   smallest boundary strictly greater than pos (n at end of input); prev()
   returns the largest boundary strictly less than pos (0 at start);
   boundary() reports whether pos is a boundary. Allocation free. */
size_t u89_grapheme_next(const unsigned char *s, size_t n, size_t pos);
size_t u89_grapheme_prev(const unsigned char *s, size_t n, size_t pos);
int u89_grapheme_boundary(const unsigned char *s, size_t n, size_t pos);

/* ---- Byte/codepoint/UTF-16 mapping (editor offsets) ---------------------- */

/* Decode the scalar beginning at byte position pos. On success writes the
   scalar to *cp and the first byte after it to *next and returns U89_OK.
   Returns U89_ERANGE when pos >= n, U89_EUTF8 on malformed or truncated
   input. cp and next may be null. */
u89_status u89_utf8_decode(const unsigned char *s, size_t n, size_t pos,
                           u89_cp *cp, size_t *next);

/* Find the scalar ending at byte position pos. On success writes the scalar
   to *cp and its starting offset to *prev and returns U89_OK. Returns
   U89_ERANGE when pos == 0 or pos > n, U89_EUTF8 when the bytes before pos
   are malformed or pos does not lie on a scalar boundary. */
u89_status u89_utf8_prev(const unsigned char *s, size_t n, size_t pos,
                         u89_cp *cp, size_t *prev);

/* Number of UTF-16 code units needed to encode cp (1 for BMP, 2 for
   supplementary). Returns 0 if cp is not a scalar. */
int u89_utf16_units(u89_cp cp);

/* Return 1 iff unit is a UTF-16 high-surrogate code unit (0xD800..0xDBFF),
   otherwise 0. Values above 0xFFFF return 0. */
int u89_utf16_is_high_surrogate(unsigned int unit);

/* Return 1 iff unit is a UTF-16 low-surrogate code unit (0xDC00..0xDFFF),
   otherwise 0. Values above 0xFFFF return 0. */
int u89_utf16_is_low_surrogate(unsigned int unit);

/* Decode one UTF-16 surrogate pair. high must lie in 0xD800..0xDBFF and low
   in 0xDC00..0xDFFF. On success writes the corresponding supplementary
   scalar to *cp when cp is non-null and returns 1. On invalid input leaves
   *cp unchanged and returns 0. Passing cp == NULL performs only the validity
   test. Decodes exactly one pair; it does not parse or traverse UTF-16
   strings and does not accept an unpaired BMP unit. */
int u89_utf16_decode_pair(unsigned int high, unsigned int low, u89_cp *cp);

/* ---- Width (East Asian Width + display cells) ---------------------------- */

typedef enum u89_ambig {
    U89_AMBIG_NARROW = 0,  /* EAW "A" resolves to width 1 (classic wcwidth) */
    U89_AMBIG_WIDE = 1     /* EAW "A" resolves to width 2 (CJK terminals)   */
} u89_ambig;

/* Display width in terminal cells of scalar cp: 0 (combining/zero-width/
   control), 1 (narrow/neutral), or 2 (wide/fullwidth/ambiguous-wide).
   Returns -1 for a non-scalar or an unprintable control. */
int u89_width(u89_cp cp, u89_ambig a);

/* Return 1 if cp is a Unicode whitespace scalar (for word wrap). */
int u89_is_whitespace(u89_cp cp);

/* Word-wrap [s, s+n): break only at Unicode whitespace so that lines stay
   within max_cols (unbreakable words may overflow). Writes up to cap break
   positions (byte offsets where a line ends) into breaks; returns the number
   of breaks (may exceed cap). */
int u89_width_wrap(const unsigned char *s, size_t n, int max_cols,
                   u89_ambig a, size_t *breaks, size_t cap);

/* ---- Scalar classification for terminal clients -------------------------- */

/* East Asian Width class. Unassigned code points default to U89_EAW_N. */
typedef enum u89_eaw {
    U89_EAW_N = 0,
    U89_EAW_A,
    U89_EAW_H,
    U89_EAW_W,
    U89_EAW_F,
    U89_EAW_NA
} u89_eaw;

u89_eaw u89_east_asian_width(u89_cp cp);

/* 1 when cp is General_Category Mn, Mc, or Me. */
int u89_is_mark(u89_cp cp);

/* 1 when cp is General_Category Cc (LF, HT, DEL, C1, ESC). */
int u89_is_control(u89_cp cp);

/* 1 when cp carries the Emoji property. */
int u89_is_emoji(u89_cp cp);

/* 1 when cp carries the Emoji_Presentation property. */
int u89_is_emoji_presentation(u89_cp cp);

/* ---- XID / ID (UAX #31) -------------------------------------------------- */

int u89_xid_start(u89_cp cp);
int u89_xid_continue(u89_cp cp);

int u89_id_start(u89_cp cp);
int u89_id_continue(u89_cp cp);

int u89_default_ignorable(u89_cp cp);
int u89_pattern_syntax(u89_cp cp);
int u89_pattern_whitespace(u89_cp cp);
int u89_join_control(u89_cp cp);

/* ---- General_Category ---------------------------------------------------- */

/* General_Category value codes returned by u89_general_category. 0 means the
   General_Category Cn (unassigned) or a non-scalar argument. */
#define U89_GC_Cc 1
#define U89_GC_Cf 2
#define U89_GC_Co 3
#define U89_GC_Cs 4
#define U89_GC_Ll 5
#define U89_GC_Lm 6
#define U89_GC_Lo 7
#define U89_GC_Lt 8
#define U89_GC_Lu 9
#define U89_GC_Mc 10
#define U89_GC_Me 11
#define U89_GC_Mn 12
#define U89_GC_Nd 13
#define U89_GC_Nl 14
#define U89_GC_No 15
#define U89_GC_Pc 16
#define U89_GC_Pd 17
#define U89_GC_Pe 18
#define U89_GC_Pf 19
#define U89_GC_Pi 20
#define U89_GC_Po 21
#define U89_GC_Ps 22
#define U89_GC_Sc 23
#define U89_GC_Sk 24
#define U89_GC_Sm 25
#define U89_GC_So 26
#define U89_GC_Zl 27
#define U89_GC_Zp 28
#define U89_GC_Zs 29

int u89_general_category(u89_cp cp);

/* ---- Case folding --------------------------------------------------------- */

/* u89_casefold mode. Full (default) folding uses CaseFolding.txt status C and
   F; Turkic additionally applies the T status overrides (I -> dotless i). */
#define U89_CASEFOLD_DEFAULT 0
#define U89_CASEFOLD_TURKIC 1

/* Full Unicode case fold of [s, s+n) into dst. One-to-many mappings are
   applied. Returns the number of bytes written, or U89_EUTF8 on malformed
   input, U89_ENOSPC when dst is too small, U89_EINVAL on a bad mode. With
   dst == NULL the exact output byte count is returned. */
int u89_casefold(int mode, const unsigned char *s, size_t n,
                 unsigned char *dst, size_t dst_cap);

/* ---- Normalization ------------------------------------------------------- */

/* Conservative number of u89_cp scalar slots a normalize_ex of [s, s+n) needs
   as workspace. Malformed UTF-8 returns U89_EUTF8. */
int u89_normalize_work_bound(const unsigned char *s, size_t n, size_t *cp_bound);

/* Conservative UTF-8 byte capacity a normalized result can need. */
int u89_normalize_out_bound(const unsigned char *s, size_t n, size_t *byte_bound);

/* Normalize [s, s+n) in mode (0=NFC, 1=NFD, 2=NFKC, 3=NFKD) into dst. work is
   caller-provided scalar scratch of work_cap u89_cp slots (at least
   u89_normalize_work_bound); dst MUST NOT overlap s or work. Returns the number
   of bytes written to dst, or U89_EUTF8 on malformed input, U89_EWORK when the
   workspace is too small, U89_ENOSPC when dst is too small. With dst == NULL
   and dst_cap 0 the exact output byte count is returned (work is still used). */
int u89_normalize_ex(int mode, const unsigned char *s, size_t n,
                     unsigned char *dst, size_t dst_cap,
                     u89_cp *work, size_t work_cap);

#ifdef __cplusplus
}
#endif

#endif
