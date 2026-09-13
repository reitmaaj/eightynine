#!/usr/bin/env python3
"""Generate libu89 Unicode 17.0.0 data tables from the vendored UCD.

Reads the pinned UCD under unicode-testdata/17.0.0 (or the UCD env var) and
emits src/u89_tables.h and src/u89_tables.c. Output is deterministic strict
C89. The consumed files and their hashes are recorded in
unicode-testdata/17.0.0/SOURCES.txt.
"""

import os
import sys

UCD = os.environ.get("UCD", "unicode-testdata/17.0.0")

EOL = "\n"

def field(line):
    return line.split("#", 1)[0].strip()

def parse_ranges(lines, val_index=1):
    """Parse 'X..Y ; VALUE' or 'X ; VALUE' lines. Returns list[(lo,hi,val)]."""
    out = []
    for line in lines:
        f = field(line)
        if not f:
            continue
        left, _, right = f.partition(";")
        val = right.strip()
        left = left.strip()
        if ".." in left:
            lo, _, hi = left.partition("..")
            out.append((int(lo, 16), int(hi, 16), val))
        else:
            cp = int(left, 16)
            out.append((cp, cp, val))
    return out

def read(path):
    with open(path, "r", encoding="utf-8") as fh:
        return fh.readlines()

# ---- UnicodeData -----------------------------------------------------------

GENERAL_CATEGORY = {}
DECOMP = {}      # cp -> (compat:bool, (cps...))
CCC = {}         # cp -> int combining class

for line in read(os.path.join(UCD, "UnicodeData.txt")):
    f = field(line)
    if not f:
        continue
    parts = f.split(";")
    cp = int(parts[0], 16)
    GENERAL_CATEGORY[cp] = parts[2]
    CCC[cp] = int(parts[3]) if parts[3] else 0
    d = parts[5]
    if d:
        if d.startswith("<"):
            tag, _, rest = d.partition(">")
            seq = tuple(int(x, 16) for x in rest.strip().split())
            DECOMP[cp] = (True, seq)
        else:
            seq = tuple(int(x, 16) for x in d.split())
            DECOMP[cp] = (False, seq)

# ---- Composition exclusions ------------------------------------------------
EXCL = set()
for line in read(os.path.join(UCD, "CompositionExclusions.txt")):
    f = field(line)
    if not f:
        continue
    EXCL.add(int(f.partition(";")[0].strip(), 16))

# ---- XID -------------------------------------------------------------------

def uax31(prop):
    lines = read(os.path.join(UCD, "DerivedCoreProperties.txt"))
    out = []
    for line in lines:
        f = field(line)
        if not f:
            continue
        if not f.endswith("; " + prop):
            continue
        left = f.partition(";")[0].strip()
        if ".." in left:
            lo, _, hi = left.partition("..")
            out.append((int(lo, 16), int(hi, 16)))
        else:
            cp = int(left, 16)
            out.append((cp, cp))
    return out

XID_START = uax31("XID_Start")
XID_CONT = uax31("XID_Continue")
ID_START = uax31("ID_Start")
ID_CONT = uax31("ID_Continue")

def prop_ranges(path, prop):
    """Read 'CP[..CP] ; prop' records from any UCD file. Returns [(lo,hi)]."""
    out = []
    for line in read(os.path.join(UCD, path)):
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
    return out

DEFAULT_IGNORABLE = prop_ranges("DerivedCoreProperties.txt",
                                "Default_Ignorable_Code_Point")
PATTERN_WS = prop_ranges("PropList.txt", "Pattern_White_Space")
PATTERN_SYNTAX = prop_ranges("PropList.txt", "Pattern_Syntax")
JOIN_CONTROL = prop_ranges("PropList.txt", "Join_Control")

# ---- East Asian Width ------------------------------------------------------

EAW = {}
for (lo, hi, val) in parse_ranges(read(os.path.join(UCD, "EastAsianWidth.txt"))):
    for cp in range(lo, hi + 1):
        EAW[cp] = val

# EAW value codes, in the order of the public u89_eaw enum. Unlisted code
# points default to N.
EAW_CODES = ["N", "A", "H", "W", "F", "Na"]
EAW_INDEX = {c: i for i, c in enumerate(EAW_CODES)}

# ---- Grapheme cluster break (auxiliary/GraphemeBreakProperty.txt) ----------

GCB_CODES = ["Other", "CR", "LF", "Control", "Extend", "ZWJ",
             "Regional_Indicator", "Prepend", "SpacingMark",
             "L", "V", "T", "LV", "LVT"]
GCB_INDEX = {c: i for i, c in enumerate(GCB_CODES)}

GCB_MAP = {}
for (lo, hi, val) in parse_ranges(
        read(os.path.join(UCD, "auxiliary/GraphemeBreakProperty.txt"))):
    for cp in range(lo, hi + 1):
        GCB_MAP[cp] = GCB_INDEX.get(val, 0)

# ---- Indic conjunct break (InCB) from DerivedCoreProperties.txt ------------

INCB_CODES = ["None", "Linker", "Consonant", "Extend"]
INCB_INDEX = {c: i for i, c in enumerate(INCB_CODES)}

INCB_MAP = {}
for line in read(os.path.join(UCD, "DerivedCoreProperties.txt")):
    f = field(line)
    if not f:
        continue
    parts = [p.strip() for p in f.split(";")]
    if len(parts) < 3:
        continue
    if parts[1] != "InCB":
        continue
    left = parts[0]
    val = INCB_INDEX.get(parts[2], 0)
    if ".." in left:
        lo, _, hi = left.partition("..")
        for cp in range(int(lo, 16), int(hi, 16) + 1):
            INCB_MAP[cp] = val
    else:
        INCB_MAP[int(left, 16)] = val

# ---- Emoji properties (emoji/emoji-data.txt) -------------------------------

EMOJI = prop_ranges("emoji/emoji-data.txt", "Emoji")
EMOJI_PRES = prop_ranges("emoji/emoji-data.txt", "Emoji_Presentation")
EXT_PICT = prop_ranges("emoji/emoji-data.txt", "Extended_Pictographic")


def value_ranges(values, default):
    """Build maximal (lo, hi, value) runs over all code points from a map."""
    out = []
    prev = default
    start = 0
    for cp in range(0x110000):
        v = values.get(cp, default)
        if v != prev:
            if prev != default:
                out.append((start, cp - 1, prev))
            start = cp
            prev = v
    if prev != default:
        out.append((start, 0x10FFFF, prev))
    return out


EAW_MAP = {cp: EAW_INDEX.get(v, 0) for cp, v in EAW.items()}
EAW_RANGES = value_ranges(EAW_MAP, 0)
GCB_RANGES = value_ranges(GCB_MAP, 0)
INCB_RANGES = value_ranges(INCB_MAP, 0)

# ---- General_Category (full range, value = 1-based index into GC_CODES) -----

GC_CODES = ["Cc", "Cf", "Co", "Cs", "Ll", "Lm", "Lo", "Lt", "Lu", "Mc", "Me",
            "Mn", "Nd", "Nl", "No", "Pc", "Pd", "Pe", "Pf", "Pi", "Po", "Ps",
            "Sc", "Sk", "Sm", "So", "Zl", "Zp", "Zs"]
GC_INDEX = {c: i + 1 for i, c in enumerate(GC_CODES)}


def gc_value(cp):
    if 0xD800 <= cp <= 0xDFFF:
        return GC_INDEX.get("Cs", 0)
    g = GENERAL_CATEGORY.get(cp)
    if g is None:
        return 0
    return GC_INDEX.get(g, 0)


GC_RANGES = []
prev = None
start = 0
for cp in range(0x110000):
    v = gc_value(cp)
    if v != prev:
        if prev is not None:
            GC_RANGES.append((start, cp - 1, prev))
        start = cp
        prev = v
GC_RANGES.append((start, 0x10FFFF, prev))

# ---- Case folding (CaseFolding.txt) ----------------------------------------
# Default full folding uses status C and F; Turkic replaces those with status T
# rows for the code points that carry a T mapping (I and I-with-dot).

def parse_casefold():
    """Return (default_rows, turkic_rows) as sorted (cp, seq) lists.

    A code point appears once per status; within a status a later row wins."""
    default = {}
    turkic = {}
    for line in read(os.path.join(UCD, "CaseFolding.txt")):
        f = field(line)
        if not f:
            continue
        parts = [p.strip() for p in f.split(";")]
        cp = int(parts[0], 16)
        status = parts[1]
        seq = tuple(int(x, 16) for x in parts[2].split())
        if status == "C" or status == "F":
            default[cp] = seq
        if status == "T":
            turkic[cp] = seq
    tr = dict(default)
    for cp, seq in turkic.items():
        tr[cp] = seq
    d_rows = sorted((cp, seq) for cp, seq in default.items() if seq != (cp,))
    t_rows = sorted((cp, seq) for cp, seq in tr.items() if seq != (cp,))
    return d_rows, t_rows

def width_type(cp):
    gc = GENERAL_CATEGORY.get(cp, "")
    if gc == "Cc":
        return 0          # control
    if gc in ("Mn", "Me", "Cf"):
        return 1          # zero-width
    eaw = EAW.get(cp, "N")
    if eaw in ("W", "F"):
        return 2          # wide
    if eaw == "A":
        return 3          # ambiguous
    return 4              # narrow/neutral

def merge_ranges(pairs):
    """pairs: list[(lo,hi,val)] (sorted by lo). Merge adjacent same-val."""
    pairs = sorted(pairs)
    out = []
    for lo, hi, val in pairs:
        if out and out[-1][1] + 1 == lo and out[-1][2] == val:
            out[-1] = (out[-1][0], hi, val)
        else:
            out.append((lo, hi, val))
    return out

# build width type per scalar, then merge into ranges
width_start = {}
for cp in range(0x110000):
    wt = width_type(cp)
    if cp == 0:
        width_start[cp] = wt
    elif wt != width_type(cp - 1):
        width_start[cp] = wt
WIDTH_RANGES = [(cp, cp, width_type(cp)) for cp in width_start]
for i in range(len(WIDTH_RANGES) - 1):
    WIDTH_RANGES[i] = (WIDTH_RANGES[i][0], WIDTH_RANGES[i + 1][0] - 1,
                       WIDTH_RANGES[i][2])
WIDTH_RANGES[-1] = (WIDTH_RANGES[-1][0], 0x10FFFF, WIDTH_RANGES[-1][2])
WIDTH_RANGES = merge_ranges(WIDTH_RANGES)

# ---- Normalization ---------------------------------------------------------

def full_decompose(cp, compat):
    """Return full decomposition sequence (recursively), or identity.

    `path` tracks only the current recursion path so that a character that
    legitimately appears at multiple points of a decomposition is expanded
    each time, while genuine decomposition cycles still terminate."""
    out = []
    path = set()

    def rec(c):
        if c in path:
            return
        path.add(c)
        e = DECOMP.get(c)
        if e is None:
            out.append(c)
        else:
            is_compat, seq = e
            if is_compat and not compat:
                out.append(c)
            else:
                for x in seq:
                    rec(x)
        path.remove(c)

    rec(cp)
    return tuple(out)

# decomposition table: for each cp with any decomposition, store full sequence
# (canonical recursion for compat=False; full recursion for compat=True).
DECOMP_ENTRIES = []
for cp in sorted(DECOMP):
    canonical = full_decompose(cp, compat=False)
    compat_seq = full_decompose(cp, compat=True)
    d_canon = canonical if canonical != (cp,) else None
    d_compat = compat_seq if compat_seq != (cp,) else None
    # store one entry with both variants
    DECOMP_ENTRIES.append((cp, d_canon, d_compat))

# Canonical decomp entries (used by NFC/NFD)
DECOMP_CANON = [(cp, seq) for (cp, seq, _) in DECOMP_ENTRIES if seq is not None]
# Compatibility-fold entries for NFKC/NFKD: entries whose canonical differs, plus
# purely-compat entries. For NFKC we use the full (compat) sequence when it
# differs from the identity.
DECOMP_ALL = [(cp, seq) for (cp, seq) in
              sorted((cp, full_decompose(cp, True))
                     for cp in DECOMP if full_decompose(cp, True) != (cp,))]

# CCC ranges (nonzero)
# CCC runs (exact maximal runs of constant NONZERO ccc; gaps are ccc 0).
CCC_RANGES = []
in_run = False
run_lo = 0
run_val = 0
for cp in range(0x110000):
    v = CCC.get(cp, 0)
    if v == 0:
        if in_run:
            CCC_RANGES.append((run_lo, cp - 1, run_val))
            in_run = False
    else:
        if in_run:
            if v != run_val:
                CCC_RANGES.append((run_lo, cp - 1, run_val))
                run_lo = cp
                run_val = v
        else:
            run_lo = cp
            run_val = v
            in_run = True
if in_run:
    CCC_RANGES.append((run_lo, 0x10FFFF, run_val))

# Composition: primary composites. A primary composite is a code point whose
# DIRECT (one-step) canonical decomposition has length 2, has ccc 0, and is
# not in CompositionExclusions. Use the direct mapping, not the full
# recursive decomposition (which would be length >2 and skip valid composites).
COMP = []
for cp in sorted(DECOMP):
    is_compat, seq = DECOMP[cp]
    if is_compat:
        continue
    if len(seq) != 2:
        continue
    if cp in EXCL:
        continue
    if CCC.get(cp, 0) != 0:
        continue
    COMP.append((seq[0], seq[1], cp))
COMP = sorted(COMP)

# ---- Emit ------------------------------------------------------------------

def emit_header():
    return """/* Generated by tools/gen_unicode_tables.py -- Unicode 17.0.0. */
#ifndef U89_TABLES_H
#define U89_TABLES_H

#include "../include/u89.h"

/* width type: 0 control, 1 zero-width, 2 wide, 3 ambiguous, 4 narrow */
typedef struct u89_wrange { u89_cp lo; u89_cp hi; unsigned char type; } u89_wrange;
extern const u89_wrange u89_width_ranges[];
extern const size_t u89_width_count;

typedef struct u89_range { u89_cp lo; u89_cp hi; } u89_range;
extern const u89_range u89_xid_start_ranges[];
extern const size_t u89_xid_start_count;
extern const u89_range u89_xid_cont_ranges[];
extern const size_t u89_xid_cont_count;
extern const u89_range u89_id_start_ranges[];
extern const size_t u89_id_start_count;
extern const u89_range u89_id_cont_ranges[];
extern const size_t u89_id_cont_count;
extern const u89_range u89_defign_ranges[];
extern const size_t u89_defign_count;
extern const u89_range u89_patws_ranges[];
extern const size_t u89_patws_count;
extern const u89_range u89_patsyn_ranges[];
extern const size_t u89_patsyn_count;
extern const u89_range u89_join_ranges[];
extern const size_t u89_join_count;

typedef struct u89_prop_range { u89_cp lo; u89_cp hi; unsigned short value; } u89_prop_range;
extern const u89_prop_range u89_gc_ranges[];
extern const size_t u89_gc_count;
extern const u89_prop_range u89_gcb_ranges[];
extern const size_t u89_gcb_count;
extern const u89_prop_range u89_incb_ranges[];
extern const size_t u89_incb_count;
extern const u89_prop_range u89_eaw_ranges[];
extern const size_t u89_eaw_count;

extern const u89_range u89_emoji_ranges[];
extern const size_t u89_emoji_count;
extern const u89_range u89_emoji_pres_ranges[];
extern const size_t u89_emoji_pres_count;
extern const u89_range u89_extpict_ranges[];
extern const size_t u89_extpict_count;

typedef struct u89_crange { u89_cp lo; u89_cp hi; unsigned char ccc; } u89_crange;
extern const u89_crange u89_ccc_ranges[];
extern const size_t u89_ccc_count;

typedef struct u89_mapping { u89_cp cp; unsigned long offset; unsigned short length; } u89_mapping;
extern const u89_cp u89_mapping_pool[];
extern const size_t u89_mapping_pool_count;
extern const u89_mapping u89_canon[];
extern const size_t u89_canon_count;
extern const u89_mapping u89_compat[];
extern const size_t u89_compat_count;
extern const u89_mapping u89_casefold_map[];
extern const size_t u89_casefold_map_count;
extern const u89_mapping u89_casefold_turkic_map[];
extern const size_t u89_casefold_turkic_map_count;

typedef struct u89_comp { u89_cp first; u89_cp second; u89_cp result; } u89_comp;
extern const u89_comp u89_comp_tbl[];
extern const size_t u89_comp_count;

#endif
"""

def c_array(name, ctype, rows, items_per_line=8):
    out = ["static const %s %s[] = {" % (ctype, name)]
    line = []
    for r in rows:
        line.append("{ %s }," % ", ".join(r))
        if len(line) >= items_per_line:
            out.append("    " + " ".join(line))
            line = []
    if line:
        out.append("    " + " ".join(line))
    out.append("};")
    return EOL.join(out)

def main():
    parts = []
    parts.append("/* Generated by tools/gen_unicode_tables.py -- Unicode 17.0.0. */")
    parts.append('#include "u89_tables.h"')
    parts.append("")

    # width
    wrows = ["{ %dUL, %dUL, %d }" % (lo, hi, t) for (lo, hi, t) in WIDTH_RANGES]
    parts.append("const u89_wrange u89_width_ranges[] = {")
    parts.append(_lines(wrows, "u89_wrange"))
    parts.append("};")
    parts.append("const size_t u89_width_count = %d;" % len(WIDTH_RANGES))
    parts.append("")

    # xid
    xrows = ["{ %dUL, %dUL }" % (lo, hi) for (lo, hi) in XID_START]
    parts.append("const u89_range u89_xid_start_ranges[] = {")
    parts.append(_lines(xrows, "u89_range"))
    parts.append("};")
    parts.append("const size_t u89_xid_start_count = %d;" % len(XID_START))
    parts.append("")
    crows = ["{ %dUL, %dUL }" % (lo, hi) for (lo, hi) in XID_CONT]
    parts.append("const u89_range u89_xid_cont_ranges[] = {")
    parts.append(_lines(crows, "u89_range"))
    parts.append("};")
    parts.append("const size_t u89_xid_cont_count = %d;" % len(XID_CONT))
    parts.append("")

    # id_start / id_continue
    irows = ["{ %dUL, %dUL }" % (lo, hi) for (lo, hi) in ID_START]
    parts.append("const u89_range u89_id_start_ranges[] = {")
    parts.append(_lines(irows, "u89_range"))
    parts.append("};")
    parts.append("const size_t u89_id_start_count = %d;" % len(ID_START))
    parts.append("")
    jrows = ["{ %dUL, %dUL }" % (lo, hi) for (lo, hi) in ID_CONT]
    parts.append("const u89_range u89_id_cont_ranges[] = {")
    parts.append(_lines(jrows, "u89_range"))
    parts.append("};")
    parts.append("const size_t u89_id_cont_count = %d;" % len(ID_CONT))
    parts.append("")

    def emit_binary(parts, name, entries):
        rows = ["{ %dUL, %dUL }" % (lo, hi) for (lo, hi) in entries]
        parts.append("const u89_range %s_ranges[] = {" % name)
        parts.append(_lines(rows, "u89_range"))
        parts.append("};")
        parts.append("const size_t %s_count = %d;" % (name, len(entries)))
        parts.append("")

    emit_binary(parts, "u89_defign", DEFAULT_IGNORABLE)
    emit_binary(parts, "u89_patws", PATTERN_WS)
    emit_binary(parts, "u89_patsyn", PATTERN_SYNTAX)
    emit_binary(parts, "u89_join", JOIN_CONTROL)

    # General_Category enumerated ranges
    grows = ["{ %dUL, %dUL, %d }" % (lo, hi, v) for (lo, hi, v) in GC_RANGES]
    parts.append("const u89_prop_range u89_gc_ranges[] = {")
    parts.append(_lines(grows, "u89_prop_range"))
    parts.append("};")
    parts.append("const size_t u89_gc_count = %d;" % len(GC_RANGES))
    parts.append("")

    # segmentation and width property ranges
    for pname, prows in (("u89_gcb", GCB_RANGES), ("u89_incb", INCB_RANGES),
                         ("u89_eaw", EAW_RANGES)):
        vrows = ["{ %dUL, %dUL, %d }" % (lo, hi, v) for (lo, hi, v) in prows]
        parts.append("const u89_prop_range %s_ranges[] = {" % pname)
        parts.append(_lines(vrows, "u89_prop_range"))
        parts.append("};")
        parts.append("const size_t %s_count = %d;" % (pname, len(prows)))
        parts.append("")

    emit_binary(parts, "u89_emoji", EMOJI)
    emit_binary(parts, "u89_emoji_pres", EMOJI_PRES)
    emit_binary(parts, "u89_extpict", EXT_PICT)

    # ccc
    erows = ["{ %dUL, %dUL, %d }" % (lo, hi, c) for (lo, hi, c) in CCC_RANGES]
    parts.append("const u89_crange u89_ccc_ranges[] = {")
    parts.append(_lines(erows, "u89_crange"))
    parts.append("};")
    parts.append("const size_t u89_ccc_count = %d;" % len(CCC_RANGES))
    parts.append("")

    # decomposition mapping pool + index rows (canonical and compatibility)
    CANON = [(cp, full_decompose(cp, False)) for cp in sorted(DECOMP)
             if full_decompose(cp, False) != (cp,)]
    COMPAT = [(cp, full_decompose(cp, True)) for cp in sorted(DECOMP)
              if full_decompose(cp, True) != (cp,)]

    def pool_add(pool, seq):
        off = len(pool)
        pool.extend(seq)
        return off

    pool = []
    canon_rows = []
    for cp, seq in CANON:
        off = pool_add(pool, seq)
        canon_rows.append((cp, off, len(seq)))
    compat_rows = []
    for cp, seq in COMPAT:
        off = pool_add(pool, seq)
        compat_rows.append((cp, off, len(seq)))

    cf_rows, cft_rows = parse_casefold()
    casefold_rows = []
    for cp, seq in cf_rows:
        off = pool_add(pool, seq)
        casefold_rows.append((cp, off, len(seq)))
    turkic_rows = []
    for cp, seq in cft_rows:
        off = pool_add(pool, seq)
        turkic_rows.append((cp, off, len(seq)))

    prow = ["%dUL" % x for x in pool]
    parts.append("const u89_cp u89_mapping_pool[] = {")
    parts.append(_lines(prow, "u89_cp"))
    parts.append("};")
    parts.append("const size_t u89_mapping_pool_count = %d;" % len(pool))
    parts.append("")

    crows = ["{ %dUL, %dUL, %d }" % (cp, off, ln) for (cp, off, ln) in canon_rows]
    parts.append("const u89_mapping u89_canon[] = {")
    parts.append(_lines(crows, "u89_mapping"))
    parts.append("};")
    parts.append("const size_t u89_canon_count = %d;" % len(canon_rows))
    parts.append("")
    krows = ["{ %dUL, %dUL, %d }" % (cp, off, ln) for (cp, off, ln) in compat_rows]
    parts.append("const u89_mapping u89_compat[] = {")
    parts.append(_lines(krows, "u89_mapping"))
    parts.append("};")
    parts.append("const size_t u89_compat_count = %d;" % len(compat_rows))
    parts.append("")

    frows = ["{ %dUL, %dUL, %d }" % (cp, off, ln)
             for (cp, off, ln) in casefold_rows]
    parts.append("const u89_mapping u89_casefold_map[] = {")
    parts.append(_lines(frows, "u89_mapping"))
    parts.append("};")
    parts.append("const size_t u89_casefold_map_count = %d;" % len(casefold_rows))
    parts.append("")
    tro = ["{ %dUL, %dUL, %d }" % (cp, off, ln)
           for (cp, off, ln) in turkic_rows]
    parts.append("const u89_mapping u89_casefold_turkic_map[] = {")
    parts.append(_lines(tro, "u89_mapping"))
    parts.append("};")
    parts.append("const size_t u89_casefold_turkic_map_count = %d;" % len(turkic_rows))
    parts.append("")

    # comp
    crows2 = ["{ %dUL, %dUL, %dUL }" % (a, b, c) for (a, b, c) in COMP]
    parts.append("const u89_comp u89_comp_tbl[] = {")
    parts.append(_lines(crows2, "u89_comp"))
    parts.append("};")
    parts.append("const size_t u89_comp_count = %d;" % len(COMP))
    parts.append("")

    src = EOL.join(parts) + EOL
    hdr = emit_header()
    with open("src/u89_tables.c", "w", encoding="utf-8") as fh:
        fh.write(src)
    with open("src/u89_tables.h", "w", encoding="utf-8") as fh:
        fh.write(hdr)
    print("emitted src/u89_tables.c and src/u89_tables.h")
    print("  width ranges: %d" % len(WIDTH_RANGES))
    print("  xid_start: %d, xid_cont: %d" % (len(XID_START), len(XID_CONT)))
    print("  ccc: %d, canon: %d, compat: %d, comp: %d, casefold: %d" % (
        len(CCC_RANGES), len(CANON), len(COMPAT), len(COMP), len(casefold_rows)))
    print("  mapping pool: %d" % len(pool))

def _lines(items, _ctype, per=6):
    out = []
    line = []
    for it in items:
        line.append(it)
        if len(line) >= per:
            out.append("    " + ", ".join(line) + ",")
            line = []
    if line:
        out.append("    " + ", ".join(line) + ",")
    return EOL.join(out)

if __name__ == "__main__":
    main()
