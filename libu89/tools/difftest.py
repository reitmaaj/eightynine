#!/usr/bin/env python3
"""Differential test: libu89 u89_normalize vs Python unicodedata.

Compiles/uses tools/u89cli and compares its output to
unicodedata.normalize(form, s) for many generated strings over the
common codepoint set (present in both Python's and libu89's Unicode).

Usage: python3 tools/difftest.py [N]   (N random strings, default 20000)
"""

import os
import random
import subprocess
import sys
import unicodedata

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI = os.path.join(ROOT, "build", "u89cli")

FORMS = {0: "NFC", 1: "NFD", 2: "NFKC", 3: "NFKD"}

# Codepoints used in the differential corpus: restrict to BMP ranges whose
# normalization is stable between Unicode 16.0.0 (Python) and 17.0.0 (libu89).
# Supplementary-plane mappings churn between versions and are excluded.
STARTERS = (list(range(0x20, 0x300)) + list(range(0x1E00, 0x1EFF))
            + list(range(0x1F00, 0x1FFF)))
# common combining marks (Mn) with nonzero ccc
COMBINING = [0x300, 0x301, 0x302, 0x303, 0x304, 0x306, 0x308, 0x30A,
             0x30F, 0x310, 0x313, 0x314, 0x31B, 0x323, 0x324, 0x325,
             0x326, 0x327, 0x331, 0x338, 0x347, 0x34E, 0x35B, 0x35C,
             0x361, 0x364, 0x366, 0x37A, 0x399, 0x1DCE, 0x20D0, 0x20D7,
             0xFE20, 0xFE23]
# compatibility chars (fold under NFKC/NFKD)
COMPAT = [0xA0, 0xA8, 0xAA, 0xAF, 0xB2, 0xB3, 0xB9, 0xBA, 0xBC, 0xBD,
          0xBE, 0x1F1, 0x1F3, 0x2126, 0x212A, 0x212B, 0x2329, 0x232A,
          0x2A0C, 0x2A74, 0xFDFA, 0xFF21, 0xFF2E, 0xFF3A, 0xFF41, 0xFF5A,
          0xFF01, 0xFF10, 0xFF19, 0xFFE0, 0xFFE1, 0xFFE2, 0xFFE3, 0xFFE4,
          0xFFE5, 0xFFE6]
# Hangul jamo and some syllables (all BMP)
HANGUL_JAMO = [0x1100, 0x1102, 0x1103, 0x1105, 0x1106, 0x1161, 0x1163,
               0x1165, 0x116E, 0x11A8, 0x11A9, 0x11AB]
HANGUL_SYL = list(range(0xAC00, 0xAC00 + 11172, 31))
HANGUL_COMPAT = [0x3131, 0x3134, 0x3137, 0x3141, 0x3161, 0x3163]


def rand_scalar(rng):
    pools = [STARTERS, COMBINING, COMPAT, HANGUL_JAMO, HANGUL_SYL,
             HANGUL_COMPAT]
    pool = rng.choice(pools)
    return pool[rng.randrange(len(pool))]


def rand_string(rng, maxlen=12):
    while True:
        n = rng.randrange(1, maxlen + 1)
        chars = []
        for _ in range(n):
            c = rand_scalar(rng)
            if 0xD800 <= c <= 0xDFFF:
                break
            chars.append(c)
        else:
            return "".join(chr(c) for c in chars)


def run_cli_batch(mode, strings):
    """Run u89cli once with many newline-joined strings; returns list of
    per-string hex results (or None for that line on error)."""
    data = "\n".join(s.encode("utf-8").hex() for s in strings) + "\n"
    p = subprocess.run([CLI, str(mode)], input=data,
                       capture_output=True, text=True)
    if p.returncode != 0:
        return None
    out = p.stdout.strip().split("\n")
    return out


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 20000
    if not os.path.exists(CLI):
        os.system("make -C %s build/u89cli" % ROOT)
    rng = random.Random(0)
    failures = 0
    batch = 256
    for start in range(0, n, batch):
        strings = [rand_string(rng) for _ in range(batch)]
        for mode, form in FORMS.items():
            got = run_cli_batch(mode, strings)
            if got is None or len(got) != len(strings):
                failures += 1
                print("batch failure for mode %s" % form)
                if failures >= 15:
                    return 1
                continue
            for s, g in zip(strings, got):
                want = unicodedata.normalize(form, s).encode("utf-8").hex()
                if g != want:
                    failures += 1
                    print("DIFF mode=%s (%r)" % (form, s))
                    print("  want=%s" % want)
                    print("  got =%s" % g)
                    if failures >= 15:
                        print("too many diffs; stopping")
                        return 1
    print("difftest: %d strings, %d differences" % (n, failures))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
