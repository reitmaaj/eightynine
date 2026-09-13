#ifndef BPF_TYPES_H
#define BPF_TYPES_H

/* Exact-width integer model (RFC 9669 section 3.1 requires 64-bit registers
 * and 32-bit immediates). ISO C89 provides no <stdint.h> and no `long long`,
 * so the exact widths are obtained by naming the widest C89 integer types and
 * rejecting any host on which they are not exactly the required sizes. Each
 * assertion makes the array size negative on a wrong-sized host, which is a
 * compile-time constraint violation. */

typedef unsigned char bpf_byte;
typedef signed char bpf_i8;
typedef unsigned short bpf_u16;
typedef signed short bpf_i16;
typedef unsigned int bpf_u32;
typedef int bpf_i32;
typedef unsigned long bpf_u64;
typedef long bpf_i64;

/* Address arithmetic and guest/region offsets are 64-bit unsigned. */
typedef unsigned long bpf_off64;

typedef char bpf_assert_ulong_64[sizeof(unsigned long) == 8 ? 1 : -1];
typedef char bpf_assert_long_64[sizeof(long) == 8 ? 1 : -1];
typedef char bpf_assert_uint_32[sizeof(unsigned int) == 4 ? 1 : -1];
typedef char bpf_assert_int_32[sizeof(int) == 4 ? 1 : -1];
typedef char bpf_assert_ushort_16[sizeof(unsigned short) == 2 ? 1 : -1];
typedef char bpf_assert_uchar_8[sizeof(unsigned char) == 1 ? 1 : -1];

#endif
