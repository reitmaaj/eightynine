/* syntax89_test.c - minimal assertion harness. */

#include "syntax89_test.h"

int syntax89_test_failures = 0;

void syntax89_test_fail(const char *file, int line, const char *msg)
{
    syntax89_test_failures += 1;
    fprintf(stderr, "%s:%d: assertion failed: %s\n", file, line, msg);
}

void syntax89_test_eq_ul(unsigned long got, unsigned long want,
                         const char *file, int line)
{
    if (got == want)
    {
        return;
    }
    syntax89_test_failures += 1;
    fprintf(stderr, "%s:%d: expected %lu, got %lu\n", file, line, want, got);
}

void syntax89_test_eq_long(long got, long want, const char *file, int line)
{
    if (got == want)
    {
        return;
    }
    syntax89_test_failures += 1;
    fprintf(stderr, "%s:%d: expected %ld, got %ld\n", file, line, want, got);
}

int syntax89_test_report(const char *name)
{
    if (syntax89_test_failures == 0)
    {
        printf("%s: ok\n", name);
        return 0;
    }
    fprintf(stderr, "%s: %d failure(s)\n", name, syntax89_test_failures);
    return 1;
}
