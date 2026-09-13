#!/bin/sh -eu
# Run the parameterized e2e exchange suite (>=100 passing, >=100 failing).
cli="$1"

python3 test/e2e/suite.py "$cli"
