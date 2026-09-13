#include <math.h>
#include <string.h>
#include "numeric.h"

#define W89_U32_MSB 0x80000000u
#define W89_U32_SIGN 0x80000000u
#define W89_U64_SIGN 0x8000000000000000UL
#define W89_I32_MIN (0x80000000u)
#define W89_I32_NEG1 0xFFFFFFFFu
#define W89_I64_MIN 0x8000000000000000UL
#define W89_I64_NEG1 0xFFFFFFFFFFFFFFFFUL

static w89_i64 w89_sx32(w89_u32 a)
{
    w89_i64 u;
    w89_i64 v;

    u = (w89_i64)a;
    v = u & 0x80000000L;
    v = v << 1;
    return u - v;
}

static w89_i64 w89_sx64(w89_u64 a)
{
    w89_u64 hi;
    w89_u64 d;
    w89_i64 lo;
    w89_i64 h;

    hi = a & W89_U64_SIGN;
    if (hi == 0) {
        return (w89_i64)a;
    }
    d = a - hi;
    lo = (w89_i64)d;
    h = (w89_i64)hi;
    return lo + h;
}

static w89_u32 w89_clz64(w89_u64 x)
{
    w89_u32 n = 0;
    if ((x >> 32) == 0) {
        n = n + 32;
        x = x << 32;
    }
    if ((x >> 48) == 0) {
        n = n + 16;
        x = x << 16;
    }
    if ((x >> 56) == 0) {
        n = n + 8;
        x = x << 8;
    }
    if ((x >> 60) == 0) {
        n = n + 4;
        x = x << 4;
    }
    if ((x >> 62) == 0) {
        n = n + 2;
        x = x << 2;
    }
    if ((x >> 63) == 0) {
        n = n + 1;
    }
    return n;
}

static w89_u32 w89_ctz64(w89_u64 x)
{
    w89_u32 n = 0;
    if ((x & 0xFFFFFFFFUL) == 0) {
        n = n + 32;
        x = x >> 32;
    }
    if ((x & 0xFFFFUL) == 0) {
        n = n + 16;
        x = x >> 16;
    }
    if ((x & 0xFFUL) == 0) {
        n = n + 8;
        x = x >> 8;
    }
    if ((x & 0x0FUL) == 0) {
        n = n + 4;
        x = x >> 4;
    }
    if ((x & 0x03UL) == 0) {
        n = n + 2;
        x = x >> 2;
    }
    if ((x & 0x01UL) == 0) {
        n = n + 1;
    }
    return n;
}

static w89_u32 w89_popcnt64(w89_u64 x)
{
    w89_u64 v;

    x = x - ((x >> 1) & 0x5555555555555555UL);
    x = (x & 0x3333333333333333UL) + ((x >> 2) & 0x3333333333333333UL);
    x = (x + (x >> 4)) & 0x0F0F0F0F0F0F0F0FUL;
    v = x * 0x0101010101010101UL;
    v = v >> 56;
    return (w89_u32)v;
}

w89_u32 w89_i32_add(w89_u32 a, w89_u32 b) { return a + b; }
w89_u32 w89_i32_sub(w89_u32 a, w89_u32 b) { return a - b; }
w89_u32 w89_i32_mul(w89_u32 a, w89_u32 b) { return a * b; }

int w89_i32_div_u(w89_u32 a, w89_u32 b, w89_u32 *out)
{
    w89_u32 v;
    if (b == 0) {
        return 0;
    }
    v = a / b;
    *out = v;
    return 1;
}

int w89_i32_div_s(w89_u32 a, w89_u32 b, w89_u32 *out)
{
    w89_i64 r;
    w89_i64 da;
    w89_i64 db;
    w89_u32 ov;
    if (b == 0) {
        return 0;
    }
    if (a == W89_I32_MIN) {
        if (b == W89_I32_NEG1) {
            return 0;
        }
    }
    da = w89_sx32(a);
    db = w89_sx32(b);
    r = da / db;
    ov = (w89_u32)r;
    *out = ov;
    return 1;
}

int w89_i32_rem_u(w89_u32 a, w89_u32 b, w89_u32 *out)
{
    w89_u32 v;
    if (b == 0) {
        return 0;
    }
    v = a % b;
    *out = v;
    return 1;
}

int w89_i32_rem_s(w89_u32 a, w89_u32 b, w89_u32 *out)
{
    w89_i64 r;
    w89_i64 da;
    w89_i64 db;
    w89_u32 ov;
    if (b == 0) {
        return 0;
    }
    if (a == W89_I32_MIN) {
        if (b == W89_I32_NEG1) {
            *out = 0;
            return 1;
        }
    }
    da = w89_sx32(a);
    db = w89_sx32(b);
    r = da % db;
    ov = (w89_u32)r;
    *out = ov;
    return 1;
}

w89_u32 w89_i32_and(w89_u32 a, w89_u32 b) { return a & b; }
w89_u32 w89_i32_or(w89_u32 a, w89_u32 b) { return a | b; }
w89_u32 w89_i32_xor(w89_u32 a, w89_u32 b) { return a ^ b; }

w89_u32 w89_i32_shl(w89_u32 a, w89_u32 b) { return a << (b & 31); }

w89_u32 w89_i32_shr_u(w89_u32 a, w89_u32 b) { return a >> (b & 31); }

w89_u32 w89_i32_shr_s(w89_u32 a, w89_u32 b)
{
    w89_u32 k;
    k = b & 31;
    if (k == 0) {
        return a;
    }
    return (a >> k) | ((0u - ((a >> 31) & 1u)) << (32 - k));
}

w89_u32 w89_i32_rotl(w89_u32 a, w89_u32 b)
{
    w89_u32 k;
    k = b & 31;
    if (k == 0) {
        return a;
    }
    return (a << k) | (a >> (32 - k));
}

w89_u32 w89_i32_rotr(w89_u32 a, w89_u32 b)
{
    w89_u32 k;
    k = b & 31;
    if (k == 0) {
        return a;
    }
    return (a >> k) | (a << (32 - k));
}

w89_u32 w89_i32_clz(w89_u32 a)
{
    w89_u32 n;
    if (a == 0) {
        return 32;
    }
    n = w89_clz64(a);
    return n - 32;
}

w89_u32 w89_i32_ctz(w89_u32 a)
{
    if (a == 0) {
        return 32;
    }
    return w89_ctz64(a);
}

w89_u32 w89_i32_popcnt(w89_u32 a) { return w89_popcnt64(a); }

w89_u32 w89_i32_eqz(w89_u32 a) { return a == 0; }

w89_u32 w89_i32_eq(w89_u32 a, w89_u32 b) { return a == b; }
w89_u32 w89_i32_ne(w89_u32 a, w89_u32 b) { return a != b; }
w89_u32 w89_i32_lt_u(w89_u32 a, w89_u32 b) { return a < b; }
w89_u32 w89_i32_lt_s(w89_u32 a, w89_u32 b) { return (a ^ W89_U32_SIGN) < (b ^ W89_U32_SIGN); }
w89_u32 w89_i32_gt_u(w89_u32 a, w89_u32 b) { return a > b; }
w89_u32 w89_i32_gt_s(w89_u32 a, w89_u32 b) { return (a ^ W89_U32_SIGN) > (b ^ W89_U32_SIGN); }
w89_u32 w89_i32_le_u(w89_u32 a, w89_u32 b) { return a <= b; }
w89_u32 w89_i32_le_s(w89_u32 a, w89_u32 b) { return (a ^ W89_U32_SIGN) <= (b ^ W89_U32_SIGN); }
w89_u32 w89_i32_ge_u(w89_u32 a, w89_u32 b) { return a >= b; }
w89_u32 w89_i32_ge_s(w89_u32 a, w89_u32 b) { return (a ^ W89_U32_SIGN) >= (b ^ W89_U32_SIGN); }

w89_u64 w89_i64_add(w89_u64 a, w89_u64 b) { return a + b; }
w89_u64 w89_i64_sub(w89_u64 a, w89_u64 b) { return a - b; }
w89_u64 w89_i64_mul(w89_u64 a, w89_u64 b) { return a * b; }

int w89_i64_div_u(w89_u64 a, w89_u64 b, w89_u64 *out)
{
    w89_u64 v;
    if (b == 0) {
        return 0;
    }
    v = a / b;
    *out = v;
    return 1;
}

int w89_i64_div_s(w89_u64 a, w89_u64 b, w89_u64 *out)
{
    w89_i64 r;
    w89_i64 da;
    w89_i64 db;
    w89_u64 ov;
    if (b == 0) {
        return 0;
    }
    if (a == W89_I64_MIN) {
        if (b == W89_I64_NEG1) {
            return 0;
        }
    }
    da = w89_sx64(a);
    db = w89_sx64(b);
    r = da / db;
    ov = (w89_u64)r;
    *out = ov;
    return 1;
}

int w89_i64_rem_u(w89_u64 a, w89_u64 b, w89_u64 *out)
{
    w89_u64 v;
    if (b == 0) {
        return 0;
    }
    v = a % b;
    *out = v;
    return 1;
}

int w89_i64_rem_s(w89_u64 a, w89_u64 b, w89_u64 *out)
{
    w89_i64 r;
    w89_i64 da;
    w89_i64 db;
    w89_u64 ov;
    if (b == 0) {
        return 0;
    }
    if (a == W89_I64_MIN) {
        if (b == W89_I64_NEG1) {
            *out = 0;
            return 1;
        }
    }
    da = w89_sx64(a);
    db = w89_sx64(b);
    r = da % db;
    ov = (w89_u64)r;
    *out = ov;
    return 1;
}

w89_u64 w89_i64_and(w89_u64 a, w89_u64 b) { return a & b; }
w89_u64 w89_i64_or(w89_u64 a, w89_u64 b) { return a | b; }
w89_u64 w89_i64_xor(w89_u64 a, w89_u64 b) { return a ^ b; }

w89_u64 w89_i64_shl(w89_u64 a, w89_u64 b) { return a << (b & 63); }

w89_u64 w89_i64_shr_u(w89_u64 a, w89_u64 b) { return a >> (b & 63); }

w89_u64 w89_i64_shr_s(w89_u64 a, w89_u64 b)
{
    w89_u64 k;
    k = b & 63;
    if (k == 0) {
        return a;
    }
    return (a >> k) | ((0UL - ((a >> 63) & 1UL)) << (64 - k));
}

w89_u64 w89_i64_rotl(w89_u64 a, w89_u64 b)
{
    w89_u64 k;
    k = b & 63;
    if (k == 0) {
        return a;
    }
    return (a << k) | (a >> (64 - k));
}

w89_u64 w89_i64_rotr(w89_u64 a, w89_u64 b)
{
    w89_u64 k;
    k = b & 63;
    if (k == 0) {
        return a;
    }
    return (a >> k) | (a << (64 - k));
}

w89_u32 w89_i64_clz(w89_u64 a)
{
    if (a == 0) {
        return 64;
    }
    return w89_clz64(a);
}

w89_u32 w89_i64_ctz(w89_u64 a)
{
    if (a == 0) {
        return 64;
    }
    return w89_ctz64(a);
}

w89_u32 w89_i64_popcnt(w89_u64 a) { return w89_popcnt64(a); }

w89_u32 w89_i64_eqz(w89_u64 a) { return a == 0; }

w89_u32 w89_i64_eq(w89_u64 a, w89_u64 b) { return a == b; }
w89_u32 w89_i64_ne(w89_u64 a, w89_u64 b) { return a != b; }
w89_u32 w89_i64_lt_u(w89_u64 a, w89_u64 b) { return a < b; }
w89_u32 w89_i64_lt_s(w89_u64 a, w89_u64 b) { return (a ^ W89_U64_SIGN) < (b ^ W89_U64_SIGN); }
w89_u32 w89_i64_gt_u(w89_u64 a, w89_u64 b) { return a > b; }
w89_u32 w89_i64_gt_s(w89_u64 a, w89_u64 b) { return (a ^ W89_U64_SIGN) > (b ^ W89_U64_SIGN); }
w89_u32 w89_i64_le_u(w89_u64 a, w89_u64 b) { return a <= b; }
w89_u32 w89_i64_le_s(w89_u64 a, w89_u64 b) { return (a ^ W89_U64_SIGN) <= (b ^ W89_U64_SIGN); }
w89_u32 w89_i64_ge_u(w89_u64 a, w89_u64 b) { return a >= b; }
w89_u32 w89_i64_ge_s(w89_u64 a, w89_u64 b) { return (a ^ W89_U64_SIGN) >= (b ^ W89_U64_SIGN); }

w89_u32 w89_f32_bits(w89_f32 x)
{
    w89_u32 u;
    size_t sz;
    sz = sizeof(w89_u32);
    memcpy(&u, &x, sz);
    return u;
}

w89_f32 w89_bits_f32(w89_u32 u)
{
    w89_f32 x;
    size_t sz;
    sz = sizeof(w89_f32);
    memcpy(&x, &u, sz);
    return x;
}

w89_u64 w89_f64_bits(w89_f64 x)
{
    w89_u64 u;
    size_t sz;
    sz = sizeof(w89_u64);
    memcpy(&u, &x, sz);
    return u;
}

w89_f64 w89_bits_f64(w89_u64 u)
{
    w89_f64 x;
    size_t sz;
    sz = sizeof(w89_f64);
    memcpy(&x, &u, sz);
    return x;
}

static int w89_is_negzero32(w89_f32 x)
{
    w89_u32 b;
    b = w89_f32_bits(x);
    return b == 0x80000000u;
}

static int w89_is_poszero32(w89_f32 x)
{
    w89_u32 b;
    b = w89_f32_bits(x);
    return b == 0x00000000u;
}

static int w89_is_negzero64(w89_f64 x)
{
    w89_u64 b;
    b = w89_f64_bits(x);
    return b == 0x8000000000000000UL;
}

static int w89_is_poszero64(w89_f64 x)
{
    w89_u64 b;
    b = w89_f64_bits(x);
    return b == 0x0000000000000000UL;
}

static w89_f32 w89_canon32(w89_f32 r)
{
    if (r != r) {
        return w89_bits_f32(W89_F32_CANON_NAN);
    }
    return r;
}

static w89_f64 w89_canon64(w89_f64 r)
{
    if (r != r) {
        return w89_bits_f64(W89_F64_CANON_NAN);
    }
    return r;
}

w89_f32 w89_f32_add(w89_f32 a, w89_f32 b)
{
    w89_f32 s;
    w89_f32 c;
    s = a + b;
    c = w89_canon32(s);
    return c;
}

w89_f32 w89_f32_sub(w89_f32 a, w89_f32 b)
{
    w89_f32 s;
    w89_f32 c;
    s = a - b;
    c = w89_canon32(s);
    return c;
}

w89_f32 w89_f32_mul(w89_f32 a, w89_f32 b)
{
    w89_f32 s;
    w89_f32 c;
    s = a * b;
    c = w89_canon32(s);
    return c;
}

w89_f32 w89_f32_div(w89_f32 a, w89_f32 b)
{
    w89_f32 s;
    w89_f32 c;
    s = a / b;
    c = w89_canon32(s);
    return c;
}

w89_f32 w89_f32_min(w89_f32 a, w89_f32 b)
{
    int az;
    int bz;
    if (a != a) {
        return w89_bits_f32(W89_F32_CANON_NAN);
    }
    if (b != b) {
        return w89_bits_f32(W89_F32_CANON_NAN);
    }
    az = w89_is_negzero32(a);
    if (az != 0) {
        bz = w89_is_poszero32(b);
        if (bz != 0) {
            return -0.0f;
        }
    }
    az = w89_is_negzero32(b);
    if (az != 0) {
        bz = w89_is_poszero32(a);
        if (bz != 0) {
            return -0.0f;
        }
    }
    if (a <= b) {
        return a;
    }
    return b;
}

w89_f32 w89_f32_max(w89_f32 a, w89_f32 b)
{
    int az;
    int bz;
    if (a != a) {
        return w89_bits_f32(W89_F32_CANON_NAN);
    }
    if (b != b) {
        return w89_bits_f32(W89_F32_CANON_NAN);
    }
    az = w89_is_negzero32(a);
    if (az != 0) {
        bz = w89_is_poszero32(b);
        if (bz != 0) {
            return 0.0f;
        }
    }
    az = w89_is_negzero32(b);
    if (az != 0) {
        bz = w89_is_poszero32(a);
        if (bz != 0) {
            return 0.0f;
        }
    }
    if (a >= b) {
        return a;
    }
    return b;
}

w89_f32 w89_f32_copysign(w89_f32 a, w89_f32 b)
{
    w89_u32 ab;
    w89_u32 bb;
    w89_u32 bits;
    ab = w89_f32_bits(a);
    bb = w89_f32_bits(b);
    ab = ab & 0x7FFFFFFFu;
    bb = bb & 0x80000000u;
    bits = ab | bb;
    return w89_bits_f32(bits);
}

w89_f32 w89_f32_abs(w89_f32 a)
{
    w89_u32 bits;
    bits = w89_f32_bits(a);
    bits = bits & 0x7FFFFFFFu;
    return w89_bits_f32(bits);
}

w89_f32 w89_f32_neg(w89_f32 a)
{
    w89_u32 bits;
    bits = w89_f32_bits(a);
    bits = bits ^ 0x80000000u;
    return w89_bits_f32(bits);
}

w89_f32 w89_f32_sqrt(w89_f32 a)
{
    w89_f32 r;
    w89_f32 c;
    r = __builtin_sqrtf(a);
    c = w89_canon32(r);
    return c;
}

w89_f32 w89_f32_ceil(w89_f32 a)
{
    w89_f32 r;
    w89_f32 c;
    r = __builtin_ceilf(a);
    c = w89_canon32(r);
    return c;
}

w89_f32 w89_f32_floor(w89_f32 a)
{
    w89_f32 r;
    w89_f32 c;
    r = __builtin_floorf(a);
    c = w89_canon32(r);
    return c;
}

w89_f32 w89_f32_trunc(w89_f32 a)
{
    if (a != a) {
        return w89_bits_f32(W89_F32_CANON_NAN);
    }
    if (a >= 0.0f) {
        return __builtin_floorf(a);
    }
    return __builtin_ceilf(a);
}

static w89_u32 w89_f32_is_even_int(w89_f32 r)
{
    w89_f32 half;
    w89_f32 fl;
    half = r / 2.0f;
    fl = __builtin_floorf(half);
    return fl == half;
}

w89_f32 w89_f32_nearest(w89_f32 a)
{
    w89_f32 r;
    w89_f32 d;
    w89_u32 bits;
    int even;
    if (a != a) {
        return w89_bits_f32(W89_F32_CANON_NAN);
    }
    if (a >= -0.5f) {
        if (a <= 0.5f) {
            bits = w89_f32_bits(a);
            bits = bits & 0x80000000u;
            return w89_bits_f32(bits);
        }
    }
    r = __builtin_floorf(a);
    d = a - r;
    if (d < 0.5f) {
        return r;
    }
    if (d > 0.5f) {
        return r + 1.0f;
    }
    even = w89_f32_is_even_int(r);
    if (even != 0) {
        return r;
    }
    return r + 1.0f;
}

w89_u32 w89_f32_eq(w89_f32 a, w89_f32 b) { return a == b; }
w89_u32 w89_f32_ne(w89_f32 a, w89_f32 b) { return a != b; }
w89_u32 w89_f32_lt(w89_f32 a, w89_f32 b) { return a < b; }
w89_u32 w89_f32_gt(w89_f32 a, w89_f32 b) { return a > b; }
w89_u32 w89_f32_le(w89_f32 a, w89_f32 b) { return a <= b; }
w89_u32 w89_f32_ge(w89_f32 a, w89_f32 b) { return a >= b; }

w89_f64 w89_f64_add(w89_f64 a, w89_f64 b)
{
    w89_f64 s;
    w89_f64 c;
    s = a + b;
    c = w89_canon64(s);
    return c;
}

w89_f64 w89_f64_sub(w89_f64 a, w89_f64 b)
{
    w89_f64 s;
    w89_f64 c;
    s = a - b;
    c = w89_canon64(s);
    return c;
}

w89_f64 w89_f64_mul(w89_f64 a, w89_f64 b)
{
    w89_f64 s;
    w89_f64 c;
    s = a * b;
    c = w89_canon64(s);
    return c;
}

w89_f64 w89_f64_div(w89_f64 a, w89_f64 b)
{
    w89_f64 s;
    w89_f64 c;
    s = a / b;
    c = w89_canon64(s);
    return c;
}

w89_f64 w89_f64_min(w89_f64 a, w89_f64 b)
{
    int az;
    int bz;
    if (a != a) {
        return w89_bits_f64(W89_F64_CANON_NAN);
    }
    if (b != b) {
        return w89_bits_f64(W89_F64_CANON_NAN);
    }
    az = w89_is_negzero64(a);
    if (az != 0) {
        bz = w89_is_poszero64(b);
        if (bz != 0) {
            return -0.0;
        }
    }
    az = w89_is_negzero64(b);
    if (az != 0) {
        bz = w89_is_poszero64(a);
        if (bz != 0) {
            return -0.0;
        }
    }
    if (a <= b) {
        return a;
    }
    return b;
}

w89_f64 w89_f64_max(w89_f64 a, w89_f64 b)
{
    int az;
    int bz;
    if (a != a) {
        return w89_bits_f64(W89_F64_CANON_NAN);
    }
    if (b != b) {
        return w89_bits_f64(W89_F64_CANON_NAN);
    }
    az = w89_is_negzero64(a);
    if (az != 0) {
        bz = w89_is_poszero64(b);
        if (bz != 0) {
            return 0.0;
        }
    }
    az = w89_is_negzero64(b);
    if (az != 0) {
        bz = w89_is_poszero64(a);
        if (bz != 0) {
            return 0.0;
        }
    }
    if (a >= b) {
        return a;
    }
    return b;
}

w89_f64 w89_f64_copysign(w89_f64 a, w89_f64 b)
{
    w89_u64 ab;
    w89_u64 bb;
    w89_u64 bits;
    ab = w89_f64_bits(a);
    bb = w89_f64_bits(b);
    ab = ab & 0x7FFFFFFFFFFFFFFFUL;
    bb = bb & 0x8000000000000000UL;
    bits = ab | bb;
    return w89_bits_f64(bits);
}

w89_f64 w89_f64_abs(w89_f64 a)
{
    w89_u64 bits;
    bits = w89_f64_bits(a);
    bits = bits & 0x7FFFFFFFFFFFFFFFUL;
    return w89_bits_f64(bits);
}

w89_f64 w89_f64_neg(w89_f64 a)
{
    w89_u64 bits;
    bits = w89_f64_bits(a);
    bits = bits ^ 0x8000000000000000UL;
    return w89_bits_f64(bits);
}

w89_f64 w89_f64_sqrt(w89_f64 a)
{
    w89_f64 r;
    w89_f64 c;
    r = sqrt(a);
    c = w89_canon64(r);
    return c;
}

w89_f64 w89_f64_ceil(w89_f64 a)
{
    w89_f64 r;
    w89_f64 c;
    r = ceil(a);
    c = w89_canon64(r);
    return c;
}

w89_f64 w89_f64_floor(w89_f64 a)
{
    w89_f64 r;
    w89_f64 c;
    r = floor(a);
    c = w89_canon64(r);
    return c;
}

w89_f64 w89_f64_trunc(w89_f64 a)
{
    if (a != a) {
        return w89_bits_f64(W89_F64_CANON_NAN);
    }
    if (a >= 0.0) {
        return floor(a);
    }
    return ceil(a);
}

static w89_u32 w89_f64_is_even_int(w89_f64 r)
{
    w89_f64 half;
    w89_f64 fl;
    half = r / 2.0;
    fl = floor(half);
    return fl == half;
}

w89_f64 w89_f64_nearest(w89_f64 a)
{
    w89_f64 r;
    w89_f64 d;
    w89_u64 bits;
    int even;
    if (a != a) {
        return w89_bits_f64(W89_F64_CANON_NAN);
    }
    if (a >= -0.5) {
        if (a <= 0.5) {
            bits = w89_f64_bits(a);
            bits = bits & 0x8000000000000000UL;
            return w89_bits_f64(bits);
        }
    }
    r = floor(a);
    d = a - r;
    if (d < 0.5) {
        return r;
    }
    if (d > 0.5) {
        return r + 1.0;
    }
    even = w89_f64_is_even_int(r);
    if (even != 0) {
        return r;
    }
    return r + 1.0;
}

w89_u32 w89_f64_eq(w89_f64 a, w89_f64 b) { return a == b; }
w89_u32 w89_f64_ne(w89_f64 a, w89_f64 b) { return a != b; }
w89_u32 w89_f64_lt(w89_f64 a, w89_f64 b) { return a < b; }
w89_u32 w89_f64_gt(w89_f64 a, w89_f64 b) { return a > b; }
w89_u32 w89_f64_le(w89_f64 a, w89_f64 b) { return a <= b; }
w89_u32 w89_f64_ge(w89_f64 a, w89_f64 b) { return a >= b; }

w89_u32 w89_i32_wrap_i64(w89_u64 a) { return (w89_u32)a; }

w89_u64 w89_i64_extend_i32_s(w89_u32 a)
{
    w89_u64 v;
    if (a & 0x80000000u) {
        v = (w89_u64)a;
        v = v | 0xFFFFFFFF00000000UL;
        return v;
    }
    return a;
}

w89_u64 w89_i64_extend_i32_u(w89_u32 a) { return a; }

static w89_f64 w89_trunc_z(w89_f64 z)
{
    if (z >= 0.0) {
        return floor(z);
    }
    return ceil(z);
}

int w89_i32_trunc_f32_s(w89_f32 z, w89_u32 *out)
{
    w89_f64 t;
    w89_f64 zd;
    w89_i64 iv;
    w89_u32 ov;
    zd = (w89_f64)z;
    t = w89_trunc_z(zd);
    if (t != t) {
        return 0;
    }
    if (t < -2147483648.0) {
        return 0;
    }
    if (t > 2147483647.0) {
        return 0;
    }
    iv = (w89_i64)t;
    ov = (w89_u32)iv;
    *out = ov;
    return 1;
}

int w89_i32_trunc_f32_u(w89_f32 z, w89_u32 *out)
{
    w89_f64 t;
    w89_f64 zd;
    w89_i64 iv;
    w89_u32 ov;
    zd = (w89_f64)z;
    t = w89_trunc_z(zd);
    if (t != t) {
        return 0;
    }
    if (t < 0.0) {
        return 0;
    }
    if (t > 4294967295.0) {
        return 0;
    }
    iv = (w89_i64)t;
    ov = (w89_u32)iv;
    *out = ov;
    return 1;
}

int w89_i32_trunc_f64_s(w89_f64 z, w89_u32 *out)
{
    w89_f64 t;
    w89_i64 iv;
    w89_u32 ov;
    t = w89_trunc_z(z);
    if (t != t) {
        return 0;
    }
    if (t < -2147483648.0) {
        return 0;
    }
    if (t > 2147483647.0) {
        return 0;
    }
    iv = (w89_i64)t;
    ov = (w89_u32)iv;
    *out = ov;
    return 1;
}

int w89_i32_trunc_f64_u(w89_f64 z, w89_u32 *out)
{
    w89_f64 t;
    w89_i64 iv;
    w89_u32 ov;
    t = w89_trunc_z(z);
    if (t != t) {
        return 0;
    }
    if (t < 0.0) {
        return 0;
    }
    if (t > 4294967295.0) {
        return 0;
    }
    iv = (w89_i64)t;
    ov = (w89_u32)iv;
    *out = ov;
    return 1;
}

int w89_i64_trunc_f32_s(w89_f32 z, w89_u64 *out)
{
    w89_f64 t;
    w89_f64 zd;
    w89_i64 iv;
    w89_u64 ov;
    zd = (w89_f64)z;
    t = w89_trunc_z(zd);
    if (t != t) {
        return 0;
    }
    if (t < -9223372036854775808.0) {
        return 0;
    }
    if (t >= 9223372036854775808.0) {
        return 0;
    }
    iv = (w89_i64)t;
    ov = (w89_u64)iv;
    *out = ov;
    return 1;
}

int w89_i64_trunc_f32_u(w89_f32 z, w89_u64 *out)
{
    w89_f64 t;
    w89_f64 zd;
    w89_f64 d;
    w89_u64 ov;
    zd = (w89_f64)z;
    t = w89_trunc_z(zd);
    if (t != t) {
        return 0;
    }
    if (t < 0.0) {
        return 0;
    }
    if (t >= 18446744073709551616.0) {
        return 0;
    }
    if (t >= 9223372036854775808.0) {
        d = t - 9223372036854775808.0;
        ov = (w89_u64)d;
        ov = ov | 0x8000000000000000UL;
        *out = ov;
    } else {
        ov = (w89_u64)t;
        *out = ov;
    }
    return 1;
}

int w89_i64_trunc_f64_s(w89_f64 z, w89_u64 *out)
{
    w89_f64 t;
    w89_i64 iv;
    w89_u64 ov;
    t = w89_trunc_z(z);
    if (t != t) {
        return 0;
    }
    if (t < -9223372036854775808.0) {
        return 0;
    }
    if (t >= 9223372036854775808.0) {
        return 0;
    }
    iv = (w89_i64)t;
    ov = (w89_u64)iv;
    *out = ov;
    return 1;
}

int w89_i64_trunc_f64_u(w89_f64 z, w89_u64 *out)
{
    w89_f64 t;
    w89_f64 d;
    w89_u64 ov;
    t = w89_trunc_z(z);
    if (t != t) {
        return 0;
    }
    if (t < 0.0) {
        return 0;
    }
    if (t >= 18446744073709551616.0) {
        return 0;
    }
    if (t >= 9223372036854775808.0) {
        d = t - 9223372036854775808.0;
        ov = (w89_u64)d;
        ov = ov | 0x8000000000000000UL;
        *out = ov;
    } else {
        ov = (w89_u64)t;
        *out = ov;
    }
    return 1;
}

w89_u32 w89_i32_trunc_sat_f32_s(w89_f32 z)
{
    w89_f64 t;
    w89_f64 zd;
    w89_i64 iv;
    w89_u32 ov;
    zd = (w89_f64)z;
    t = w89_trunc_z(zd);
    if (t != t) {
        return 0;
    }
    if (t < -2147483648.0) {
        return 0x80000000u;
    }
    if (t > 2147483647.0) {
        return 0x7FFFFFFFu;
    }
    iv = (w89_i64)t;
    ov = (w89_u32)iv;
    return ov;
}

w89_u32 w89_i32_trunc_sat_f32_u(w89_f32 z)
{
    w89_f64 t;
    w89_f64 zd;
    w89_i64 iv;
    w89_u32 ov;
    zd = (w89_f64)z;
    t = w89_trunc_z(zd);
    if (t != t) {
        return 0;
    }
    if (t < 0.0) {
        return 0;
    }
    if (t > 4294967295.0) {
        return 0xFFFFFFFFu;
    }
    iv = (w89_i64)t;
    ov = (w89_u32)iv;
    return ov;
}

w89_u32 w89_i32_trunc_sat_f64_s(w89_f64 z)
{
    w89_f64 t;
    w89_i64 iv;
    w89_u32 ov;
    t = w89_trunc_z(z);
    if (t != t) {
        return 0;
    }
    if (t < -2147483648.0) {
        return 0x80000000u;
    }
    if (t > 2147483647.0) {
        return 0x7FFFFFFFu;
    }
    iv = (w89_i64)t;
    ov = (w89_u32)iv;
    return ov;
}

w89_u32 w89_i32_trunc_sat_f64_u(w89_f64 z)
{
    w89_f64 t;
    w89_i64 iv;
    w89_u32 ov;
    t = w89_trunc_z(z);
    if (t != t) {
        return 0;
    }
    if (t < 0.0) {
        return 0;
    }
    if (t > 4294967295.0) {
        return 0xFFFFFFFFu;
    }
    iv = (w89_i64)t;
    ov = (w89_u32)iv;
    return ov;
}

w89_u64 w89_i64_trunc_sat_f32_s(w89_f32 z)
{
    w89_f64 t;
    w89_f64 zd;
    w89_i64 iv;
    w89_u64 ov;
    zd = (w89_f64)z;
    t = w89_trunc_z(zd);
    if (t != t) {
        return 0;
    }
    if (t < -9223372036854775808.0) {
        return 0x8000000000000000UL;
    }
    if (t >= 9223372036854775808.0) {
        return 0x7FFFFFFFFFFFFFFFUL;
    }
    iv = (w89_i64)t;
    ov = (w89_u64)iv;
    return ov;
}

w89_u64 w89_i64_trunc_sat_f32_u(w89_f32 z)
{
    w89_f64 t;
    w89_f64 zd;
    w89_f64 d;
    w89_u64 ov;
    zd = (w89_f64)z;
    t = w89_trunc_z(zd);
    if (t != t) {
        return 0;
    }
    if (t < 0.0) {
        return 0;
    }
    if (t >= 18446744073709551616.0) {
        return 0xFFFFFFFFFFFFFFFFUL;
    }
    if (t >= 9223372036854775808.0) {
        d = t - 9223372036854775808.0;
        ov = (w89_u64)d;
        ov = ov | 0x8000000000000000UL;
        return ov;
    }
    ov = (w89_u64)t;
    return ov;
}

w89_u64 w89_i64_trunc_sat_f64_s(w89_f64 z)
{
    w89_f64 t;
    w89_i64 iv;
    w89_u64 ov;
    t = w89_trunc_z(z);
    if (t != t) {
        return 0;
    }
    if (t < -9223372036854775808.0) {
        return 0x8000000000000000UL;
    }
    if (t >= 9223372036854775808.0) {
        return 0x7FFFFFFFFFFFFFFFUL;
    }
    iv = (w89_i64)t;
    ov = (w89_u64)iv;
    return ov;
}

w89_u64 w89_i64_trunc_sat_f64_u(w89_f64 z)
{
    w89_f64 t;
    w89_f64 d;
    w89_u64 ov;
    t = w89_trunc_z(z);
    if (t != t) {
        return 0;
    }
    if (t < 0.0) {
        return 0;
    }
    if (t >= 18446744073709551616.0) {
        return 0xFFFFFFFFFFFFFFFFUL;
    }
    if (t >= 9223372036854775808.0) {
        d = t - 9223372036854775808.0;
        ov = (w89_u64)d;
        ov = ov | 0x8000000000000000UL;
        return ov;
    }
    ov = (w89_u64)t;
    return ov;
}

w89_f32 w89_f32_convert_i32_s(w89_u32 a)
{
    w89_i32 iv;
    w89_f32 fv;
    iv = (w89_i32)a;
    fv = (w89_f32)iv;
    return fv;
}

w89_f32 w89_f32_convert_i32_u(w89_u32 a) { return (w89_f32)a; }
w89_f32 w89_f32_convert_i64_s(w89_u64 a)
{
    w89_i64 iv;
    w89_f32 fv;
    iv = (w89_i64)a;
    fv = (w89_f32)iv;
    return fv;
}

w89_f32 w89_f32_convert_i64_u(w89_u64 a) { return (w89_f32)a; }
w89_f64 w89_f64_convert_i32_s(w89_u32 a)
{
    w89_i32 iv;
    w89_f64 fv;
    iv = (w89_i32)a;
    fv = (w89_f64)iv;
    return fv;
}

w89_f64 w89_f64_convert_i32_u(w89_u32 a) { return (w89_f64)a; }
w89_f64 w89_f64_convert_i64_s(w89_u64 a)
{
    w89_i64 iv;
    w89_f64 fv;
    iv = (w89_i64)a;
    fv = (w89_f64)iv;
    return fv;
}

w89_f64 w89_f64_convert_i64_u(w89_u64 a) { return (w89_f64)a; }

w89_f32 w89_f32_demote_f64(w89_f64 z)
{
    w89_f32 f;
    w89_f32 c;
    f = (w89_f32)z;
    c = w89_canon32(f);
    return c;
}

w89_f64 w89_f64_promote_f32(w89_f32 z)
{
    w89_f64 f;
    w89_f64 c;
    f = (w89_f64)z;
    c = w89_canon64(f);
    return c;
}

w89_u32 w89_i32_reinterpret_f32(w89_f32 z) { return w89_f32_bits(z); }

w89_f32 w89_f32_reinterpret_i32(w89_u32 a) { return w89_bits_f32(a); }

w89_u64 w89_i64_reinterpret_f64(w89_f64 z) { return w89_f64_bits(z); }

w89_f64 w89_f64_reinterpret_i64(w89_u64 a) { return w89_bits_f64(a); }

w89_u32 w89_i32_extend8_s(w89_u32 a)
{
    a = a & 0xFFu;
    if (a & 0x80u) {
        return a | 0xFFFFFF00u;
    }
    return a;
}

w89_u32 w89_i32_extend16_s(w89_u32 a)
{
    a = a & 0xFFFFu;
    if (a & 0x8000u) {
        return a | 0xFFFF0000u;
    }
    return a;
}

w89_u64 w89_i64_extend8_s(w89_u64 a)
{
    a = a & 0xFFUL;
    if (a & 0x80UL) {
        return a | 0xFFFFFFFFFFFFFF00UL;
    }
    return a;
}

w89_u64 w89_i64_extend16_s(w89_u64 a)
{
    a = a & 0xFFFFUL;
    if (a & 0x8000UL) {
        return a | 0xFFFFFFFFFFFF0000UL;
    }
    return a;
}

w89_u64 w89_i64_extend32_s(w89_u64 a)
{
    a = a & 0xFFFFFFFFUL;
    if (a & 0x80000000UL) {
        return a | 0xFFFFFFFF00000000UL;
    }
    return a;
}
