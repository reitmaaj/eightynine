#!/usr/bin/env python3
"""Independent generator oracle for libu89 property tables.

Parses the vendored Unicode 17.0.0 data directly (a code path independent of
tools/gen_unicode_tables.py) to derive expected range lists, then compares them
to the committed src/u89_tables.c arrays. Exits nonzero on any mismatch.

Usage: gen_check.py [UCD-DIR]
"""

import os
import re
import sys

UCD = sys.argv[1] if len(sys.argv) > 1 else "unicode-testdata/17.0.0"

GC_CODES = ["Cc", "Cf", "Co", "Cs", "Ll", "Lm", "Lo", "Lt", "Lu", "Mc", "Me",
            "Mn", "Nd", "Nl", "No", "Pc", "Pd", "Pe", "Pf", "Pi", "Po", "Ps",
            "Sc", "Sk", "Sm", "So", "Zl", "Zp", "Zs"]
GC_IDX = {c: i + 1 for i, c in enumerate(GC_CODES)}


def field(line):
    return line.split("#", 1)[0].strip()


def read(name):
    with open(os.path.join(UCD, name), encoding="utf-8") as fh:
        return fh.readlines()


def parse_ranges(name, prop):
    """Return sorted merged [(lo, hi)] for 'CP[..CP] ; prop' lines in name."""
    out = []
    for line in read(name):
        f = field(line)
        if not f:
            continue
        left, _, right = f.partition(";")
        if right.strip() != prop:
            continue
        left = left.strip()
        if ".." in left:
            lo, _, hi = left.partition("..")
            out.append((int(lo, 16), int(hi, 16)))
        else:
            cp = int(left, 16)
            out.append((cp, cp))
    out.sort()
    merged = []
    for lo, hi in out:
        if merged and merged[-1][1] + 1 == lo:
            merged[-1] = (merged[-1][0], hi)
        else:
            merged.append((lo, hi))
    return merged


def read_c_ranges(name):
    """Parse '{ loUL, hiUL }' rows of const array <name>_ranges in tables.c."""
    txt = open("src/u89_tables.c", encoding="utf-8").read()
    m = re.search(r"const u89_range %s_ranges\[\] = \{(.*?)\};" % name, txt,
                  re.S)
    if not m:
        sys.exit("missing array " + name)
    rows = []
    for lo, hi in re.findall(r"\{ (\d+)UL, (\d+)UL \}", m.group(1)):
        rows.append((int(lo), int(hi)))
    return rows


def read_gc_ranges():
    return read_prop_ranges("u89_gc")


def read_prop_ranges(name):
    txt = open("src/u89_tables.c", encoding="utf-8").read()
    m = re.search(r"const u89_prop_range %s_ranges\[\] = \{(.*?)\};" % name,
                  txt, re.S)
    if not m:
        sys.exit("missing array " + name)
    rows = []
    for lo, hi, v in re.findall(r"\{ (\d+)UL, (\d+)UL, (\d+) \}", m.group(1)):
        rows.append((int(lo), int(hi), int(v)))
    return rows


GCB_CODES = ["Other", "CR", "LF", "Control", "Extend", "ZWJ",
             "Regional_Indicator", "Prepend", "SpacingMark",
             "L", "V", "T", "LV", "LVT"]
GCB_IDX = {c: i for i, c in enumerate(GCB_CODES)}

INCB_CODES = ["None", "Linker", "Consonant", "Extend"]
INCB_IDX = {c: i for i, c in enumerate(INCB_CODES)}

EAW_CODES = ["N", "A", "H", "W", "F", "Na"]
EAW_IDX = {c: i for i, c in enumerate(EAW_CODES)}


def value_ranges(pairs, default=0):
    """Merge (lo, hi, val) pairs into maximal runs, omitting the default."""
    vals = {}
    for lo, hi, v in pairs:
        for cp in range(lo, hi + 1):
            vals[cp] = v
    out = []
    prev = default
    start = 0
    for cp in range(0x110000):
        v = vals.get(cp, default)
        if v != prev:
            if prev != default:
                out.append((start, cp - 1, prev))
            start = cp
            prev = v
    if prev != default:
        out.append((start, 0x10FFFF, prev))
    return out


def expected_gcb():
    pairs = []
    for line in read("auxiliary/GraphemeBreakProperty.txt"):
        f = field(line)
        if not f:
            continue
        left, _, right = f.partition(";")
        left = left.strip()
        v = GCB_IDX.get(right.strip(), 0)
        if ".." in left:
            lo, _, hi = left.partition("..")
            pairs.append((int(lo, 16), int(hi, 16), v))
        else:
            cp = int(left, 16)
            pairs.append((cp, cp, v))
    return value_ranges(pairs)


def expected_incb():
    pairs = []
    for line in read("DerivedCoreProperties.txt"):
        f = field(line)
        if not f:
            continue
        parts = [p.strip() for p in f.split(";")]
        if len(parts) < 3:
            continue
        if parts[1] != "InCB":
            continue
        v = INCB_IDX.get(parts[2], 0)
        left = parts[0]
        if ".." in left:
            lo, _, hi = left.partition("..")
            pairs.append((int(lo, 16), int(hi, 16), v))
        else:
            cp = int(left, 16)
            pairs.append((cp, cp, v))
    return value_ranges(pairs)


def expected_eaw():
    pairs = []
    for line in read("EastAsianWidth.txt"):
        f = field(line)
        if not f:
            continue
        left, _, right = f.partition(";")
        left = left.strip()
        v = EAW_IDX.get(right.strip(), 0)
        if ".." in left:
            lo, _, hi = left.partition("..")
            pairs.append((int(lo, 16), int(hi, 16), v))
        else:
            cp = int(left, 16)
            pairs.append((cp, cp, v))
    return value_ranges(pairs)


def check_value_table(name, expected, failures):
    actual = read_prop_ranges(name)
    if merge(expected) != merge(actual):
        print("MISMATCH %s_ranges" % name)
        e = set(merge(expected))
        a = set(merge(actual))
        print("  only-expected: %s" % sorted(e - a)[:6])
        print("  only-actual:   %s" % sorted(a - e)[:6])
        return failures + 1
    return failures


def merge(rows):
    """Merge adjacent (lo, hi, val) rows that share val into maximal runs."""
    rows = sorted(rows, key=lambda r: (r[0], r[1]))
    out = []
    for lo, hi, val in rows:
        if out and out[-1][1] + 1 == lo and out[-1][2] == val:
            out[-1] = (out[-1][0], hi, val)
        else:
            out.append((lo, hi, val))
    return out


def expected_gc():
    cat = {}
    for line in read("UnicodeData.txt"):
        f = field(line)
        if not f:
            continue
        p = f.split(";")
        cat[int(p[0], 16)] = p[2]
    out = []
    prev = None
    start = 0
    for cp in range(0x110000):
        if 0xD800 <= cp <= 0xDFFF:
            v = GC_IDX.get("Cs", 0)
        else:
            g = cat.get(cp)
            v = GC_IDX.get(g, 0) if g else 0
        if v != prev:
            if prev is not None:
                out.append((start, cp - 1, prev))
            start = cp
            prev = v
    out.append((start, 0x10FFFF, prev))
    return out


BIN = {
    "u89_xid_start": ("DerivedCoreProperties.txt", "XID_Start"),
    "u89_xid_cont": ("DerivedCoreProperties.txt", "XID_Continue"),
    "u89_id_start": ("DerivedCoreProperties.txt", "ID_Start"),
    "u89_id_cont": ("DerivedCoreProperties.txt", "ID_Continue"),
    "u89_defign": ("DerivedCoreProperties.txt", "Default_Ignorable_Code_Point"),
    "u89_patws": ("PropList.txt", "Pattern_White_Space"),
    "u89_patsyn": ("PropList.txt", "Pattern_Syntax"),
    "u89_join": ("PropList.txt", "Join_Control"),
}


def main():
    failures = 0
    for cname, (src, prop) in BIN.items():
        expected = [(lo, hi, 1) for (lo, hi) in parse_ranges(src, prop)]
        actual = [(lo, hi, 1) for (lo, hi) in read_c_ranges(cname)]
        if merge(expected) != merge(actual):
            failures += 1
            print("MISMATCH %s (prop %s)" % (cname, prop))
            e = set(merge(expected))
            a = set(merge(actual))
            print("  only-expected: %s" % sorted(e - a)[:6])
            print("  only-actual:   %s" % sorted(a - e)[:6])
    if merge(expected_gc()) != merge(read_gc_ranges()):
        failures += 1
        print("MISMATCH u89_gc_ranges")
    failures = check_value_table("u89_gcb", expected_gcb(), failures)
    failures = check_value_table("u89_incb", expected_incb(), failures)
    failures = check_value_table("u89_eaw", expected_eaw(), failures)
    for cname, prop in (("u89_emoji", "Emoji"),
                        ("u89_emoji_pres", "Emoji_Presentation"),
                        ("u89_extpict", "Extended_Pictographic")):
        expected = [(lo, hi, 1) for (lo, hi) in
                    parse_ranges("emoji/emoji-data.txt", prop)]
        actual = [(lo, hi, 1) for (lo, hi) in read_c_ranges(cname)]
        if merge(expected) != merge(actual):
            failures += 1
            print("MISMATCH %s (prop %s)" % (cname, prop))
    if failures:
        print("%d property tables mismatch" % failures)
        return 1
    print("gen_check: all %d property tables match UCD" % (len(BIN) + 7))
    return 0


if __name__ == "__main__":
    sys.exit(main())
