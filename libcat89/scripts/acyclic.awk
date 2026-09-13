#!/usr/bin/awk -f
# acyclic.awk <edges> - Kahn's algorithm over whitespace-separated "FROM TO"
# lines; prints a pass message and exits 0 if the directed graph is acyclic,
# otherwise prints ARCH FAIL to stderr and exits nonzero.

function die(msg) {
    print "ARCH FAIL: " msg > "/dev/stderr"
    exit 1
}
{
    edge[++m] = $1 "\t" $2
    indeg[$2]++
    isnode[$1] = 1
    isnode[$2] = 1
}
END {
    for (k in isnode) {
        if (indeg[k] == 0) {
            q[++qh] = k
        }
    }
    seen = 0
    for (h = 1; h <= qh; h++) {
        cur = q[h]
        seen++
        for (e = 1; e <= m; e++) {
            split(edge[e], p, "\t")
            if (p[1] == cur) {
                indeg[p[2]]--
                if (indeg[p[2]] == 0) {
                    q[++qh] = p[2]
                }
            }
        }
    }
    if (seen != length(isnode)) {
        die("public-header include graph contains a cycle")
    }
    exit 0
}
