#ifndef WASM89_NUMERIC_H
#define WASM89_NUMERIC_H

#include "leb.h"

typedef float  w89_f32;
typedef double w89_f64;

#define W89_F32_CANON_NAN 0x7FC00000u
#define W89_F64_CANON_NAN 0x7FF8000000000000UL

w89_u32 w89_f32_bits(w89_f32 x);
w89_f32 w89_bits_f32(w89_u32 u);
w89_u64 w89_f64_bits(w89_f64 x);
w89_f64 w89_bits_f64(w89_u64 u);

w89_u32 w89_i32_add(w89_u32 a, w89_u32 b);
w89_u32 w89_i32_sub(w89_u32 a, w89_u32 b);
w89_u32 w89_i32_mul(w89_u32 a, w89_u32 b);
int w89_i32_div_u(w89_u32 a, w89_u32 b, w89_u32 *out);
int w89_i32_div_s(w89_u32 a, w89_u32 b, w89_u32 *out);
int w89_i32_rem_u(w89_u32 a, w89_u32 b, w89_u32 *out);
int w89_i32_rem_s(w89_u32 a, w89_u32 b, w89_u32 *out);
w89_u32 w89_i32_and(w89_u32 a, w89_u32 b);
w89_u32 w89_i32_or(w89_u32 a, w89_u32 b);
w89_u32 w89_i32_xor(w89_u32 a, w89_u32 b);
w89_u32 w89_i32_shl(w89_u32 a, w89_u32 b);
w89_u32 w89_i32_shr_u(w89_u32 a, w89_u32 b);
w89_u32 w89_i32_shr_s(w89_u32 a, w89_u32 b);
w89_u32 w89_i32_rotl(w89_u32 a, w89_u32 b);
w89_u32 w89_i32_rotr(w89_u32 a, w89_u32 b);
w89_u32 w89_i32_clz(w89_u32 a);
w89_u32 w89_i32_ctz(w89_u32 a);
w89_u32 w89_i32_popcnt(w89_u32 a);
w89_u32 w89_i32_eqz(w89_u32 a);
w89_u32 w89_i32_eq(w89_u32 a, w89_u32 b);
w89_u32 w89_i32_ne(w89_u32 a, w89_u32 b);
w89_u32 w89_i32_lt_u(w89_u32 a, w89_u32 b);
w89_u32 w89_i32_lt_s(w89_u32 a, w89_u32 b);
w89_u32 w89_i32_gt_u(w89_u32 a, w89_u32 b);
w89_u32 w89_i32_gt_s(w89_u32 a, w89_u32 b);
w89_u32 w89_i32_le_u(w89_u32 a, w89_u32 b);
w89_u32 w89_i32_le_s(w89_u32 a, w89_u32 b);
w89_u32 w89_i32_ge_u(w89_u32 a, w89_u32 b);
w89_u32 w89_i32_ge_s(w89_u32 a, w89_u32 b);

w89_u64 w89_i64_add(w89_u64 a, w89_u64 b);
w89_u64 w89_i64_sub(w89_u64 a, w89_u64 b);
w89_u64 w89_i64_mul(w89_u64 a, w89_u64 b);
int w89_i64_div_u(w89_u64 a, w89_u64 b, w89_u64 *out);
int w89_i64_div_s(w89_u64 a, w89_u64 b, w89_u64 *out);
int w89_i64_rem_u(w89_u64 a, w89_u64 b, w89_u64 *out);
int w89_i64_rem_s(w89_u64 a, w89_u64 b, w89_u64 *out);
w89_u64 w89_i64_and(w89_u64 a, w89_u64 b);
w89_u64 w89_i64_or(w89_u64 a, w89_u64 b);
w89_u64 w89_i64_xor(w89_u64 a, w89_u64 b);
w89_u64 w89_i64_shl(w89_u64 a, w89_u64 b);
w89_u64 w89_i64_shr_u(w89_u64 a, w89_u64 b);
w89_u64 w89_i64_shr_s(w89_u64 a, w89_u64 b);
w89_u64 w89_i64_rotl(w89_u64 a, w89_u64 b);
w89_u64 w89_i64_rotr(w89_u64 a, w89_u64 b);
w89_u32 w89_i64_clz(w89_u64 a);
w89_u32 w89_i64_ctz(w89_u64 a);
w89_u32 w89_i64_popcnt(w89_u64 a);
w89_u32 w89_i64_eqz(w89_u64 a);
w89_u32 w89_i64_eq(w89_u64 a, w89_u64 b);
w89_u32 w89_i64_ne(w89_u64 a, w89_u64 b);
w89_u32 w89_i64_lt_u(w89_u64 a, w89_u64 b);
w89_u32 w89_i64_lt_s(w89_u64 a, w89_u64 b);
w89_u32 w89_i64_gt_u(w89_u64 a, w89_u64 b);
w89_u32 w89_i64_gt_s(w89_u64 a, w89_u64 b);
w89_u32 w89_i64_le_u(w89_u64 a, w89_u64 b);
w89_u32 w89_i64_le_s(w89_u64 a, w89_u64 b);
w89_u32 w89_i64_ge_u(w89_u64 a, w89_u64 b);
w89_u32 w89_i64_ge_s(w89_u64 a, w89_u64 b);

w89_f32 w89_f32_add(w89_f32 a, w89_f32 b);
w89_f32 w89_f32_sub(w89_f32 a, w89_f32 b);
w89_f32 w89_f32_mul(w89_f32 a, w89_f32 b);
w89_f32 w89_f32_div(w89_f32 a, w89_f32 b);
w89_f32 w89_f32_min(w89_f32 a, w89_f32 b);
w89_f32 w89_f32_max(w89_f32 a, w89_f32 b);
w89_f32 w89_f32_copysign(w89_f32 a, w89_f32 b);
w89_f32 w89_f32_abs(w89_f32 a);
w89_f32 w89_f32_neg(w89_f32 a);
w89_f32 w89_f32_sqrt(w89_f32 a);
w89_f32 w89_f32_ceil(w89_f32 a);
w89_f32 w89_f32_floor(w89_f32 a);
w89_f32 w89_f32_trunc(w89_f32 a);
w89_f32 w89_f32_nearest(w89_f32 a);
w89_u32 w89_f32_eq(w89_f32 a, w89_f32 b);
w89_u32 w89_f32_ne(w89_f32 a, w89_f32 b);
w89_u32 w89_f32_lt(w89_f32 a, w89_f32 b);
w89_u32 w89_f32_gt(w89_f32 a, w89_f32 b);
w89_u32 w89_f32_le(w89_f32 a, w89_f32 b);
w89_u32 w89_f32_ge(w89_f32 a, w89_f32 b);

w89_f64 w89_f64_add(w89_f64 a, w89_f64 b);
w89_f64 w89_f64_sub(w89_f64 a, w89_f64 b);
w89_f64 w89_f64_mul(w89_f64 a, w89_f64 b);
w89_f64 w89_f64_div(w89_f64 a, w89_f64 b);
w89_f64 w89_f64_min(w89_f64 a, w89_f64 b);
w89_f64 w89_f64_max(w89_f64 a, w89_f64 b);
w89_f64 w89_f64_copysign(w89_f64 a, w89_f64 b);
w89_f64 w89_f64_abs(w89_f64 a);
w89_f64 w89_f64_neg(w89_f64 a);
w89_f64 w89_f64_sqrt(w89_f64 a);
w89_f64 w89_f64_ceil(w89_f64 a);
w89_f64 w89_f64_floor(w89_f64 a);
w89_f64 w89_f64_trunc(w89_f64 a);
w89_f64 w89_f64_nearest(w89_f64 a);
w89_u32 w89_f64_eq(w89_f64 a, w89_f64 b);
w89_u32 w89_f64_ne(w89_f64 a, w89_f64 b);
w89_u32 w89_f64_lt(w89_f64 a, w89_f64 b);
w89_u32 w89_f64_gt(w89_f64 a, w89_f64 b);
w89_u32 w89_f64_le(w89_f64 a, w89_f64 b);
w89_u32 w89_f64_ge(w89_f64 a, w89_f64 b);

w89_u32 w89_i32_wrap_i64(w89_u64 a);
w89_u64 w89_i64_extend_i32_s(w89_u32 a);
w89_u64 w89_i64_extend_i32_u(w89_u32 a);
int w89_i32_trunc_f32_s(w89_f32 z, w89_u32 *out);
int w89_i32_trunc_f32_u(w89_f32 z, w89_u32 *out);
int w89_i32_trunc_f64_s(w89_f64 z, w89_u32 *out);
int w89_i32_trunc_f64_u(w89_f64 z, w89_u32 *out);
int w89_i64_trunc_f32_s(w89_f32 z, w89_u64 *out);
int w89_i64_trunc_f32_u(w89_f32 z, w89_u64 *out);
int w89_i64_trunc_f64_s(w89_f64 z, w89_u64 *out);
int w89_i64_trunc_f64_u(w89_f64 z, w89_u64 *out);
w89_u32 w89_i32_trunc_sat_f32_s(w89_f32 z);
w89_u32 w89_i32_trunc_sat_f32_u(w89_f32 z);
w89_u32 w89_i32_trunc_sat_f64_s(w89_f64 z);
w89_u32 w89_i32_trunc_sat_f64_u(w89_f64 z);
w89_u64 w89_i64_trunc_sat_f32_s(w89_f32 z);
w89_u64 w89_i64_trunc_sat_f32_u(w89_f32 z);
w89_u64 w89_i64_trunc_sat_f64_s(w89_f64 z);
w89_u64 w89_i64_trunc_sat_f64_u(w89_f64 z);
w89_f32 w89_f32_convert_i32_s(w89_u32 a);
w89_f32 w89_f32_convert_i32_u(w89_u32 a);
w89_f32 w89_f32_convert_i64_s(w89_u64 a);
w89_f32 w89_f32_convert_i64_u(w89_u64 a);
w89_f64 w89_f64_convert_i32_s(w89_u32 a);
w89_f64 w89_f64_convert_i32_u(w89_u32 a);
w89_f64 w89_f64_convert_i64_s(w89_u64 a);
w89_f64 w89_f64_convert_i64_u(w89_u64 a);
w89_f32 w89_f32_demote_f64(w89_f64 z);
w89_f64 w89_f64_promote_f32(w89_f32 z);
w89_u32 w89_i32_reinterpret_f32(w89_f32 z);
w89_f32 w89_f32_reinterpret_i32(w89_u32 a);
w89_u64 w89_i64_reinterpret_f64(w89_f64 z);
w89_f64 w89_f64_reinterpret_i64(w89_u64 a);
w89_u32 w89_i32_extend8_s(w89_u32 a);
w89_u32 w89_i32_extend16_s(w89_u32 a);
w89_u64 w89_i64_extend8_s(w89_u64 a);
w89_u64 w89_i64_extend16_s(w89_u64 a);
w89_u64 w89_i64_extend32_s(w89_u64 a);

#endif
