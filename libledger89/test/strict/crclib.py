"""CRC-32C (Castagnoli), reflected polynomial 0x82F63B78, seed 0.

Dependency-free so both the harness and the table generator can import it
without loading the shared library.
"""

_TABLE = None


def _table():
    global _TABLE
    if _TABLE is None:
        rows = []
        for i in range(256):
            value = i
            for _ in range(8):
                if value & 1:
                    value = 0x82F63B78 ^ (value >> 1)
                else:
                    value >>= 1
            rows.append(value)
        _TABLE = rows
    return _TABLE


def crc32c(data, crc=0):
    table = _table()
    crc = (~crc) & 0xFFFFFFFF
    for byte in data:
        crc = table[(crc ^ byte) & 0xFF] ^ (crc >> 8)
    return (~crc) & 0xFFFFFFFF
