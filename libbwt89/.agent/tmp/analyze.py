from __future__ import print_function
from model import suffix_array


def sym_of(T):
    return [b + 1 for b in T] + [0]


def analyze_del(T, m):
    """m = text index (0..n-1) of the char being deleted."""
    n = len(T)
    sym = sym_of(T)
    N = n + 1
    newsym = sym[:m] + sym[m + 1:]
    nn = len(newsym)
    sa_o = suffix_array(sym)
    sa_n = suffix_array(newsym)
    p_o = next(r for r in range(N) if sa_o[r] == 0)
    p_n = next(r for r in range(nn) if sa_n[r] == 0)
    # L value of a row given sa and arr
    Lo = [sym[(x - 1) % N] for x in sa_o]
    Ln = [newsym[(x - 1) % nn] for x in sa_n]
    # physical original start of each row
    def n2o(q):
        return q if q < m else q + 1
    # row -> text-position-of-L (the char it holds as L): for old
    # L[row] = sym[sa_o[row]-1]; text position of that char:
    # text index = (sa_o[row]-1) if (sa_o[row]-1)<=n-1 else None(sentinel)
    print("del T=%r at m=%d" % (T, m))
    print("  old rows (row:L:start:holdsTextPos):")
    for r in range(N):
        prev = (sa_o[r] - 1) % N
        tp = prev if prev < n else 'S'
        print("   r=%2d L=%2d start=%2d holds=%s" % (r, Lo[r], sa_o[r], tp))
    print("  p_old=%d  p_new=%d" % (p_o, p_n))
    print("  new rows (row:L_origvalue):")
    for r in range(nn):
        prev = (sa_n[r] - 1) % nn
        ov = newsym[prev]
        print("   r=%2d L=%3d(orig sym val) start(new)=%2d" % (r, ov, sa_n[r]))
