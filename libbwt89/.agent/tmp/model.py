"""Ground-truth model of the library BWT semantics and Salson reverse-engineering.

Matches libbwt89 exactly:
  sym = [b+1 for b in T] + [0]; N = n+1; sentinel = 0 unique min at index n.
  sa = suffix array of sym (rows = rotations sorted).
  orig p = row whose sa == 0.
  L[r] = sym[(sa[r]-1) % N]   (last column, length N; L[p] == 0).
  reported B = L with the cell at row p removed, in row order.
"""
from __future__ import print_function


def suffix_array(s):
    """SA-IS-free: use doubling for correctness in the model."""
    n = len(s)
    sa = sorted(range(n), key=lambda i: s[i:])
    return sa


def bwt(T):
    """Return (L, p) for the library semantics."""
    n = len(T)
    sym = [b + 1 for b in T] + [0]
    N = n + 1
    sa = suffix_array(sym)
    p = next(r for r in range(N) if sa[r] == 0)
    L = [sym[(sa[r] - 1) % N] for r in range(N)]
    return L, p


def reported_bytes(L, p):
    return [x for r, x in enumerate(L) if r != p]


def rot_contents(T):
    """List of rotations (as lists of internal symbols) in sorted row order."""
    n = len(T)
    sym = [b + 1 for b in T] + [0]
    N = n + 1
    sa = suffix_array(sym)
    return [[sym[(sa[r] + k) % N] for k in range(N)] for r in range(N)]


def text_of(L, p, n):
    """Recover text (0..n-1 bytes) from L,p via LF, matching bwt89_ibwt.
    Returns list of byte values T[0..n-1]."""
    sym = list(L)
    N = n + 1
    freq = [0] * (257)
    for c in sym:
        freq[c] += 1
    less = [0] * 257
    acc = 0
    for c in range(257):
        less[c] = acc
        acc += freq[c]
    occ = [0] * 257
    row_of = [0] * N
    for i in range(N):
        c = sym[i]
        row_of[i] = less[c] + occ[c]
        occ[c] += 1
    row = p
    out = [0] * n
    for i in range(n - 1, -1, -1):
        row = row_of[row]
        c = sym[row]
        out[i] = c - 1
    return out
