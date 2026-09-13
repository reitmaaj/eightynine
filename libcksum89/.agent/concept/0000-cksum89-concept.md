# libcksum89 concept

## What it is

`libcksum89` is a leaf library of four fixed checksum algorithms:

| Algorithm | Width | Use |
| --- | --- | --- |
| CRC-32/ISO-HDLC | 32 | zlib/gzip/PNG-style integrity |
| CRC-32C | 32 | iSCSI, NVMe, ext4-style integrity |
| CRC-64/NVME | 64 | NVMe NVM command-set integrity |
| INET16 | 16 | RFC 1071 Internet checksum |

Each algorithm exposes exactly one caller-owned context type and one
`init` / `update` / `final` triple. A one-shot wrapper performs exactly
`init; update; final` with no separate implementation path.

## Hard boundaries

- **No allocation.** Contexts are caller storage. There is no `destroy`.
- **No I/O.** No file descriptors, no `FILE *`, no buffered helpers.
- **No runtime dispatch.** No algorithm registry, no generic context, no
  function pointers, no algorithm enum, no runtime selector.
- **No error vocabulary.** Every operation is total under the documented
  pointer preconditions; there is no status type and no `errno`.
- **No wire representation.** Values are numeric. Byte order and framing
  belong to the protocol or format layer that owns the bytes.
- **No policy.** No hex formatting, no verify helpers, no CRC combine, no
  incremental RFC-1624 header editing, no hardware/SIMD selection.

## Why a leaf library

Consumers (`libledger89`, `libraft89`, `libj89`, storage and protocol code)
need exact checksum primitives with state they own, without inheriting a
framework. The context API gives exactly the state the mathematics needs and
nothing more: contexts are copyable value types, so a caller can branch a
stream with ordinary assignment.

## Portability

Strict ISO C89. The only machine-model requirement beyond C89 is
`CHAR_BIT == 8`. 64-bit values are two 32-bit limbs, so no C99
`unsigned long long` or implementation-specific 64-bit type enters the ABI.
