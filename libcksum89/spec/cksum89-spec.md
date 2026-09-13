# libcksum89 - specification

Normative contract for libcksum89 v1. Sections map to tests under `test/`
and scenarios under `.agent/testing/`. The public surface is
`include/cksum89.h`; the design rationale is in `.agent/design/0000-design.md`.

## 1. Purpose

libcksum89 provides four fixed checksum algorithms with caller-owned
streaming contexts and one-shot wrappers. It owns the mathematics and
nothing around it: no allocation, no I/O, no runtime dispatch, no error
vocabulary, no wire representation.

## 2. Scope

Included: CRC-32/ISO-HDLC, CRC-32C, CRC-64/NVME, RFC 1071 INET16;
incremental and one-shot operation; caller-owned copyable contexts.

Excluded: algorithm registry, custom polynomials, runtime selector,
allocation, allocator integration, error/status enum, destroy/free,
file-descriptor or `FILE *` helpers, scatter/gather, CRC combine, RFC-1624
incremental header editing, verify helpers, hex formatting, byte
serialization, network-byte-order conversion, hardware/SIMD selection, CPU
feature detection, plugins, threads.

## 3. Algorithm parameters

| Algorithm | Width | Poly | Init | RefIn | RefOut | XorOut | Check("123456789") |
| --- | --- | --- | --- | --- | --- | --- | --- |
| CRC-32/ISO-HDLC | 32 | `0x04c11db7` | `0xffffffff` | yes | yes | `0xffffffff` | `0xcbf43926` |
| CRC-32C | 32 | `0x1edc6f41` | `0xffffffff` | yes | yes | `0xffffffff` | `0xe3069283` |
| CRC-64/NVME | 64 | `0xad93d23594c93659` | `0xffffffffffffffff` | yes | yes | `0xffffffffffffffff` | `0xae8b14860a799888` |
| INET16 | 16 | one's-complement sum | 0 | n/a | n/a | complement | `0xf62a` |

INET16 forms big-endian 16-bit words from adjacent octets, pads a final odd
octet with a zero low byte, folds end-around carry, and complements.

## 4. Public API

```c
typedef unsigned short cksum89_u16;
typedef unsigned long  cksum89_u32;

typedef struct cksum89_u64 { cksum89_u32 hi; cksum89_u32 lo; } cksum89_u64;

void cksum89_crc32_iso_hdlc_init(cksum89_crc32_iso_hdlc_ctx *ctx);
void cksum89_crc32_iso_hdlc_update(cksum89_crc32_iso_hdlc_ctx *ctx,
                                   const void *data, size_t len);
cksum89_u32 cksum89_crc32_iso_hdlc_final(
    const cksum89_crc32_iso_hdlc_ctx *ctx);
cksum89_u32 cksum89_crc32_iso_hdlc(const void *data, size_t len);

/* The same init/update/final/one-shot shape for cksum89_crc32c,
 * cksum89_crc64_nvme, and cksum89_inet16. */
```

## 5. Preconditions

For every context pointer: `ctx != NULL`. For every `(data, len)`:
`len == 0` permits `data == NULL`; `len != 0` requires `data` to point to at
least `len` readable bytes. Violations are undefined behavior; they are not
reported.

`update(ctx, NULL, 0)` is valid and has no effect.

## 6. Context semantics

- Contexts are caller-owned value types; they allocate nothing and need no
  destructor.
- They may be copied with ordinary assignment; a copy is an independent
  stream.
- `final` is observational: it does not modify the context, repeated calls
  agree, and the context remains usable for further `update` or `final`.
- One-shot functions mean exactly `init; update; final`.

## 7. Numeric representation

`cksum89_u16` and `cksum89_u32` are value domains (`0..0xffff`,
`0..0xffffffff`) stored in C89 minimum-width types. Returned and retained
values never contain significant bits outside their domain. `cksum89_u64`
is `hi * 2^32 + lo`; it has no byte-order or wire meaning.

## 8. INET16 streaming

An update may end between the two octets of a word. The context retains the
pending octet and pairs it with the first octet of the next update. Only
`final` treats a pending octet as `[octet, 0x00]`.

## 9. Implementation freedom

The specification fixes parameters, semantics, and the public ABI. Table
shape, helper decomposition, and internal representation are free. The
checked-in tables MUST match independently generated reference remainders.

## 10. Conformance

A build is conformant when `just check` passes: the green seven-cell matrix
(C89 and C23, GCC and Clang, semantic checks, canonical format), the shell
and formatting lint, and the full test suite including the portability
probes.
