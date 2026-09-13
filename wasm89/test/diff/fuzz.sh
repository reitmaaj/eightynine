#!/bin/sh -eu
# Bounded multi-seed differential fuzzing (see `just diff-fuzz`).
#
# Runs the differential sweep across several integer seeds so a broad,
# random sample of numeric programs and template modules is exercised.
# Any seed that reports a mismatch or an error fails the whole run.
#
# Tuning via environment:
#   DIFF_SEEDS   whitespace-separated seed list (default 0 1 2 3 4 5 6 7)
#   DIFF_MODULES random numeric modules per seed (default 16)

seeds="${DIFF_SEEDS:-0 1 2 3 4 5 6 7}"
modules="${DIFF_MODULES:-16}"
here=$(dirname -- "$0")
sweep="$here/sweep.py"

failed=0
for seed in $seeds; do
    echo "== seed $seed =="
    if ! python3 "$sweep" --seed "$seed" --modules "$modules" \
        --funcs 6 --args 8 --template-args 6 --quiet; then
        failed=1
    fi
done
if [ "$failed" -ne 0 ]; then
    echo "diff-fuzz: one or more seeds reported failures" >&2
    exit 1
fi
echo "diff-fuzz: all seeds clean"
