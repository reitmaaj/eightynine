# Scenarios: published vectors and reference model

## SCENARIO: reference model reproduces published check values

GIVEN the test-only bit-at-a-time reference model
WHEN it computes each frozen algorithm over its published input
THEN it returns the published numeric value

| Algorithm | Input | Value |
| --- | --- | --- |
| CRC-32/ISO-HDLC | `""` | `0x00000000` |
| CRC-32/ISO-HDLC | `"123456789"` | `0xcbf43926` |
| CRC-32C | `""` | `0x00000000` |
| CRC-32C | `"123456789"` | `0xe3069283` |
| CRC-64/NVME | `""` | `0x0000000000000000` |
| CRC-64/NVME | `"123456789"` | `0xae8b14860a799888` |
| INET16 | `""` | `0xffff` |
| INET16 | `"123456789"` | `0xf62a` |
| INET16 | `00 01 f2 03 f4 f5 f6 f7` | `0x220d` (sum `0xddf2`) |

## SCENARIO: production reproduces published vectors

GIVEN the production one-shot functions
WHEN they compute each frozen algorithm over its published input
THEN they return the same values as the reference model

Additional CRC-32C iSCSI vectors (numeric values; RFC 3720 prints the
serialized big-endian bytes `aa36918a`, `62a8ab43`, `46dd794e`):

| Input | Value |
| --- | --- |
| 32 x `0x00` | `0x8a9136aa` |
| 32 x `0xff` | `0x62a8ab43` |
| bytes `0x00..0x1f` | `0x46dd794e` |

Official CRC-64/NVME 4096-byte vectors (numeric; NVMe prints the CRC
little-endian in the codeword):

| Input | Value |
| --- | --- |
| 4096 x `0x00` | `0x6482d367eb22b64e` |
| 4096 x `0xff` | `0xc0ddba7302eca3ac` |
| 4096 x `(i mod 256)` | `0x3e729f5f6750449c` |
| 4096 x `((4095 - i) mod 256)` | `0x9a2df64b8e9e517e` |

## SCENARIO: differential agreement over corpora

GIVEN the reference model and the production implementation
WHEN both process all 256 one-byte inputs, lengths 0..1024, all-zero,
all-one, incrementing, decrementing, and deterministic pseudorandom buffers
THEN the values are identical for every algorithm

## SCENARIO: lookup tables match independent remainders

GIVEN the checked-in CRC tables
WHEN every entry is compared with a Rocksoft MSB-first single-byte
remainder reflected to the production orientation
THEN every entry matches
