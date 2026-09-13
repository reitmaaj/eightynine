from __future__ import print_function
from model import bwt, reported_bytes
import itertools


def ascii_alphabet_syms(a):
    return a  # symbols are already internal ranks (byte+1) when bytes are 0..x


def gen_texts(n, vals):
    """vals = list of byte values (0..255). Yield all texts length n."""
    for tup in itertools.product(vals, repeat=n):
        yield list(tup)


def run_delete(delete_fn, max_len, vals, verbose=True):
    bad = 0
    total = 0
    for n in range(1, max_len + 1):
        for T in gen_texts(n, vals):
            L, p = bwt(T)
            for m in range(n):
                T2 = T[:m] + T[m + 1:]
                L2e, p2e = bwt(T2)
                try:
                    L2, p2 = delete_fn(list(T), list(L), p, m)
                except Exception as e:
                    bad += 1
                    total += 1
                    if verbose and bad <= 5:
                        print("EXC", T, m, e)
                    continue
                total += 1
                if L2 != L2e or p2 != p2e:
                    bad += 1
                    if verbose and bad <= 20:
                        print("DEL-MISMATCH T=%r m=%d" % (T, m))
                        print("  L    =", L, "p=", p)
                        print("  expect L'=", L2e, "p'=", p2e)
                        print("  got    L'=", L2, "p'=", p2)
    return bad, total


def run_insert(insert_fn, max_len, vals, verbose=True):
    bad = 0
    total = 0
    for n in range(0, max_len):
        for T in gen_texts(n, vals):
            L, p = bwt(T)
            for m in range(n + 1):
                for c in vals:
                    T2 = T[:m] + [c] + T[m:]
                    L2e, p2e = bwt(T2)
                    try:
                        L2, p2 = insert_fn(list(T), list(L), p, m, c)
                    except Exception as e:
                        bad += 1
                        total += 1
                        if verbose and bad <= 5:
                            print("EXC", T, m, c, e)
                        continue
                    total += 1
                    if L2 != L2e or p2 != p2e:
                        bad += 1
                        if verbose and bad <= 20:
                            print("INS-MISMATCH T=%r m=%d c=%d" % (T, m, c))
                            print("  L    =", L, "p=", p)
                            print("  expect L'=", L2e, "p'=", p2e)
                            print("  got    L'=", L2, "p'=", p2)
    return bad, total
