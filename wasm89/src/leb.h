#ifndef WASM89_LEB_H
#define WASM89_LEB_H

typedef unsigned char w89_byte;
typedef unsigned int  w89_u32;
typedef unsigned long w89_u64;
typedef int           w89_i32;
typedef long          w89_i64;

typedef char w89_assert_u64_is_8[(sizeof(w89_u64) == 8) ? 1 : -1];
typedef char w89_assert_u32_is_4[(sizeof(w89_u32) == 4) ? 1 : -1];

typedef enum w89_err {
    W89_ERR_NONE = 0,
    W89_ERR_MAGIC,
    W89_ERR_VERSION,
    W89_ERR_SECTION_ID,
    W89_ERR_SECTION_SIZE,
    W89_ERR_TRAILING,
    W89_ERR_EOF,
    W89_ERR_EOF_SECTION,
    W89_ERR_LEN_OUT_OF_BOUNDS,
    W89_ERR_INT_TOO_LARGE,
    W89_ERR_LEB_TOO_LONG,
    W89_ERR_MALFORMED_LIMITS,
    W89_ERR_MALFORMED_MUT,
    W89_ERR_MALFORMED_IMPORT_KIND,
    W89_ERR_MALFORMED_EXPORT_KIND,
    W89_ERR_MALFORMED_TYPE,
    W89_ERR_MALFORMED_REF,
    W89_ERR_MEMOP_FLAGS,
    W89_ERR_END_EXPECTED,
    W89_ERR_UTF8,
    W89_ERR_UNKNOWN_OPCODE,
    W89_ERR_ALIGNMENT,
    W89_ERR_ALIGNMENT_TOO_BIG,
    W89_ERR_MIN_GT_MAX,
    W89_ERR_CONST_RANGE,
    W89_ERR_FUNC_CODE_LEN,
    W89_ERR_DATA_COUNT,
    W89_ERR_TOO_MANY_LOCALS,
    W89_ERR_DUP_EXPORT,
    W89_ERR_UNSUPPORTED,
    W89_ERR_OUT_OF_MEMORY,
    W89_ERR_INVALID,
    W89_ERR_DATA_COUNT_REQUIRED,
    W89_ERR_COUNT
} w89_err;

const char *w89_err_message(w89_err e);

w89_err w89_leb_u_err(const w89_byte **pp, const w89_byte *end, int nbits,
                      w89_u64 *out);
w89_err w89_leb_s_err(const w89_byte **pp, const w89_byte *end, int nbits,
                      w89_i64 *out);

int w89_leb_u(const w89_byte **pp, const w89_byte *end, int nbits,
              w89_u64 *out);
int w89_leb_s(const w89_byte **pp, const w89_byte *end, int nbits,
              w89_i64 *out);

int w89_leb_u32(const w89_byte **pp, const w89_byte *end, w89_u32 *out);
int w89_leb_u64(const w89_byte **pp, const w89_byte *end, w89_u64 *out);
int w89_leb_s32(const w89_byte **pp, const w89_byte *end, w89_i32 *out);
int w89_leb_s64(const w89_byte **pp, const w89_byte *end, w89_i64 *out);

#endif
