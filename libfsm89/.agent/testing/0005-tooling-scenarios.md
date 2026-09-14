# 0005 — tooling gate scenarios

These scenarios cover the verification gates themselves. A gate that can
pass vacuously is a defect: it must fail when it cannot prove what it claims.

## Coverage gate

SCENARIO gcov failure
  GIVEN the coverage run cannot produce profile data
  WHEN the coverage gate runs
  THEN it fails instead of reporting full coverage

SCENARIO missing profile file
  GIVEN a source file with no corresponding `.gcov` report
  WHEN the coverage gate runs
  THEN it fails

SCENARIO unexecuted line
  GIVEN a `.gcov` report containing a `#####` line
  WHEN the coverage gate runs
  THEN it fails

SCENARIO unexecuted branch
  GIVEN a `.gcov` report containing a `never executed` branch
  WHEN the coverage gate runs
  THEN it fails

SCENARIO complete coverage
  GIVEN every source has a report with no unexecuted line or branch
  WHEN the coverage gate runs
  THEN it succeeds

SCENARIO no sources
  GIVEN an empty source directory
  WHEN the coverage gate runs
  THEN it fails

## Sanitizer gate

SCENARIO known undefined behavior
  GIVEN a program with signed integer overflow compiled with the sanitize
    flags
  WHEN it runs
  THEN it exits non-zero, proving UBSan is armed and does not recover

SCENARIO known address error
  GIVEN a program with a heap buffer overflow compiled with the sanitize
    flags
  WHEN it runs
  THEN it exits non-zero, proving ASan is armed

## Build gate

SCENARIO deleted source
  GIVEN a source file that was built and then removed
  WHEN the library is rebuilt
  THEN the archive contains no symbol from the removed source

## API coverage gate

SCENARIO missing public prototype
  GIVEN the header does not declare a documented public function
  WHEN the API coverage gate runs
  THEN it fails
