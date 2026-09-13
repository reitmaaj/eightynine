from __future__ import print_function
from model import bwt, suffix_array
import itertools


def sym_of(T):
    return [b + 1 for b in T] + [0]


def survivor_order_deviation(T, m):
    """Compare (a) new row order from full recompute, labelled by original
    physical start, against (b) 'old order with start m removed' (no re-sort).
    Returns True if they differ (i.e. a genuine reorder among survivors)."""
    n = len(T)
    sym = sym_of(T)
    N = n + 1
    sa_o = suffix_array(sym)
    newsym = sym[:m] + sym[m + 1:]
    nn = len(newsym)
    sa_n = suffix_array(newsym)

    def n2o(q):
        return q if q < m else q + 1

    new_order_true = [n2o(sa_n[r]) for r in range(nn)]
    old_order = [sa_o[r] for r in range(N)]
    new_order_pred = [s for s in old_order if s != m]
    return new_order_true, new_order_pred


def count_deviations(n, vals):
    dev = 0
    tot = 0
    examples = []
    for T in itertools.product(vals, repeat=n):
        for m in range(n):
            t, p = survivor_order_deviation(list(T), m)
            tot += 1
            if t != p:
                dev += 1
                if len(examples) < 8:
                    examples.append((list(T), m, t, p))
    return dev, tot, examples


for n in range(1, 7):
    for vals in ([0, 1], [0, 1, 2]):
        d, t, ex = count_deviations(n, vals)
        if t:
            print("n=%d alpha=%d deviations=%d/%d" % (n, len(vals), d, t))
        if ex:
            for e in ex[:4]:
                print("   T=%r m=%d true=%r pred=%r" % (e[0], e[1], e[2], e[3]))
