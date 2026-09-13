#include "leb.h"

#define W89_LEB_NONTERM 0x80u
#define W89_LEB_PAYLOAD 0x7Fu

static const char *const w89_err_msgs[] = {
    "no error",
    "magic header not detected",
    "unknown binary version",
    "malformed section id",
    "section size mismatch",
    "unexpected content after last section",
    "unexpected end",
    "unexpected end of section or function",
    "length out of bounds",
    "integer too large",
    "integer representation too long",
    "malformed limits flags",
    "malformed mutability",
    "malformed import kind",
    "malformed export kind",
    "malformed value type",
    "malformed reference type",
    "malformed memop flags",
    "END opcode expected",
    "malformed UTF-8 encoding",
    "illegal opcode",
    "alignment must be a power of two",
    "alignment must not be larger than natural",
    "size minimum must not be greater than maximum",
    "constant out of range",
    "function and code section have inconsistent lengths",
    "data count and data section have inconsistent lengths",
    "too many locals",
    "duplicate export name",
    "unsupported feature",
    "out of memory",
    "invalid module",
    "data count section required",
    "invalid error code"
};

const char *w89_err_message(w89_err e)
{
    int ei;

    ei = (int)e;
    if (ei < 0) {
        return "unknown error";
    }
    if (ei >= W89_ERR_COUNT) {
        return "unknown error";
    }
    return w89_err_msgs[ei];
}

w89_err w89_leb_u_err(const w89_byte **pp, const w89_byte *end, int nbits,
                      w89_u64 *out)
{
    w89_u64 result = 0;
    int shift = 0;
    int n = 0;
    int max_bytes;
    const w89_byte *q;
    w89_u64 payload;
    w89_u64 limit;
    w89_i64 outv;

    max_bytes = (nbits + 6) / 7;
    q = *pp;
    for (;;) {
        w89_byte b;
        if (q >= end) {
            return W89_ERR_EOF;
        }
        b = *q;
        q = q + 1;
        n = n + 1;
        if (n > max_bytes) {
            return W89_ERR_LEB_TOO_LONG;
        }
        if (b & W89_LEB_NONTERM) {
            if (shift > 56) {
                return W89_ERR_LEB_TOO_LONG;
            }
            payload = (w89_u64)(b & W89_LEB_PAYLOAD);
            result = result | (payload << shift);
            shift = shift + 7;
        } else {
            if (shift == 63) {
                if ((b & 0x7Eu) != 0) {
                    return W89_ERR_INT_TOO_LARGE;
                }
                payload = (w89_u64)(b & 0x01u);
                result = result | (payload << 63);
            } else {
                payload = (w89_u64)(b & W89_LEB_PAYLOAD);
                result = result | (payload << shift);
            }
            if (nbits < 64) {
                limit = (w89_u64)1;
                limit = limit << nbits;
                if (result >= limit) {
                    return W89_ERR_INT_TOO_LARGE;
                }
            }
            *pp = q;
            outv = (w89_i64)result;
            *out = outv;
            return W89_ERR_NONE;
        }
    }
}

w89_err w89_leb_s_err(const w89_byte **pp, const w89_byte *end, int nbits,
                      w89_i64 *out)
{
    w89_u64 result = 0;
    int shift = 0;
    int n = 0;
    int max_bytes;
    const w89_byte *q;
    w89_u64 payload;
    w89_u64 sign_zone;
    w89_i64 outv;

    max_bytes = (nbits + 6) / 7;
    q = *pp;
    for (;;) {
        w89_byte b;
        if (q >= end) {
            return W89_ERR_EOF;
        }
        b = *q;
        q = q + 1;
        n = n + 1;
        if (n > max_bytes) {
            return W89_ERR_LEB_TOO_LONG;
        }
        if (b & W89_LEB_NONTERM) {
            if (shift > 56) {
                return W89_ERR_LEB_TOO_LONG;
            }
            payload = (w89_u64)(b & W89_LEB_PAYLOAD);
            result = result | (payload << shift);
            shift = shift + 7;
        } else {
            if (shift == 63) {
                if (b == 0x00) {
                    /* ok */
                } else if (b != 0x7F) {
                    return W89_ERR_INT_TOO_LARGE;
                }
                payload = (w89_u64)(b & 0x01u);
                result = result | (payload << 63);
            } else {
                payload = (w89_u64)(b & W89_LEB_PAYLOAD);
                result = result | (payload << shift);
                if ((b >> 6) & 0x01u) {
                    result = result | (~0UL << (shift + 7));
                }
            }
            if (nbits < 64) {
                sign_zone = result >> (nbits - 1);
                if (sign_zone == 0) {
                    /* ok */
                } else {
                    if (sign_zone != (~0UL >> (nbits - 1))) {
                        return W89_ERR_INT_TOO_LARGE;
                    }
                }
            }
            *pp = q;
            outv = (w89_i64)result;
            *out = outv;
            return W89_ERR_NONE;
        }
    }
}

int w89_leb_u(const w89_byte **pp, const w89_byte *end, int nbits,
              w89_u64 *out)
{
    w89_err e;

    e = w89_leb_u_err(pp, end, nbits, out);
    return e == W89_ERR_NONE;
}

int w89_leb_s(const w89_byte **pp, const w89_byte *end, int nbits,
              w89_i64 *out)
{
    w89_err e;

    e = w89_leb_s_err(pp, end, nbits, out);
    return e == W89_ERR_NONE;
}

int w89_leb_u32(const w89_byte **pp, const w89_byte *end, w89_u32 *out)
{
    w89_u64 v;
    w89_u32 outv;
    int ok;

    ok = w89_leb_u(pp, end, 32, &v);
    if (ok == 0) {
        return 0;
    }
    outv = (w89_u32)v;
    *out = outv;
    return 1;
}

int w89_leb_u64(const w89_byte **pp, const w89_byte *end, w89_u64 *out)
{
    int ok;

    ok = w89_leb_u(pp, end, 64, out);
    return ok;
}

int w89_leb_s32(const w89_byte **pp, const w89_byte *end, w89_i32 *out)
{
    w89_i64 v;
    w89_i32 outv;
    int ok;

    ok = w89_leb_s(pp, end, 32, &v);
    if (ok == 0) {
        return 0;
    }
    outv = (w89_i32)v;
    *out = outv;
    return 1;
}

int w89_leb_s64(const w89_byte **pp, const w89_byte *end, w89_i64 *out)
{
    int ok;

    ok = w89_leb_s(pp, end, 64, out);
    return ok;
}
