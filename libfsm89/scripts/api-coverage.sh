#!/bin/sh -eu
# api-coverage.sh - every public fsm89_* function is named by a test.
#
# The header is the API contract; the acceptance gate requires each public
# function to have success and failure tests. This check proves only that a
# test names the function, not that the tests are complete.

root="$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)"

grep -o 'fsm89_[a-z_]*(' "$root/include/fsm89.h" | sed 's/($//' | sort -u |
    while read -r name; do
        case "$name" in
            *_fn)
                continue
                ;;
        esac
        if ! grep -rq -- "$name" "$root/test"; then
            echo "api-coverage: no test names $name" >&2
            exit 1
        fi
    done

echo "api-coverage: clean"
