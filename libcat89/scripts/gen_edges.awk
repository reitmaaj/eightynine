#!/usr/bin/awk -f
# gen_edges.awk <header-dir> - emit "FROM TO" edges for the public-header
# include graph (FROM, TO are header basenames under include/cat89).

BEGIN {
    dir = ARGV[1]
    ARGV[1] = ""
    while (("ls " dir) | getline f) {
        if (f ~ /\.h$/) {
            files[++n] = f
        }
    }
    close("ls " dir)
    for (i = 1; i <= n; i++) {
        file = dir "/" files[i]
        while ((getline line < file) > 0) {
            if (line ~ /#include <cat89\//) {
                sub(/^.*#include <cat89\//, "", line)
                sub(/>.*$/, "", line)
                print files[i], line
            }
        }
        close(file)
    }
}
