from __future__ import print_function
from model import suffix_array
import itertools


def sym_of(T):
    return [b + 1 for b in T] + [0]


def order_deletion(T, m, verbose=True):
    """Represent new-text rotations by their ORIGINAL physical start s != m.
    Compare the old sorted order (by old content) and new sorted order (by new
    content) of the same physical rotations; report how order changes."""
    n = len(T)
    sym = sym_of(T)
    N = n + 1
    sa_o = suffix_array(sym)
    newsym = sym[:m] + sym[m + 1:]
    nn = len(newsym)
    sa_n = suffix_array(newsym)

    def n2o(q):
        return q if q < m else q + 1

    old_order = [sa_o[r] for r in range(N)]          # physical starts, old rows
    new_order = [n2o(sa_n[r]) for r in range(nn)]    # physical starts, new rows
    # rank of each surviving physical start in old order (0-based among survivors)
    survivors = [s for s in range(N) if s != m]
    rank_old = {s: old_order.index(s) for s in survivors}
    rank_new = {s: i for i, s in enumerate(new_order)}
    moved = [s for s in survivors if rank_old[s] != rank_new[s]]
    if verbose:
        print("del T=%r m=%d sym=%r" % (T, m, sym))
        print("  old rows (orig start):", old_order)
        print("  new rows (orig start):", new_order)
        print("  affected starts:", moved)
        print("  sa_old:", sa_o, " sa_new(->orig):", [n2o(x) for x in sa_n])
    return old_order, new_order, moved


def explore(n, vals):
    for T in itertools.product(vals, repeat=n):
        for m in range(n):
            order_deletion(list(T), m, verbose=False)


if __name__ == "__main__":
    order_deletion([1, 2, 1], 0)
    order_deletion([1, 2, 1], 1)
    order_deletion([1, 2, 1], 2)
