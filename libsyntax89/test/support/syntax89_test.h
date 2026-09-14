#ifndef SYNTAX89_TEST_H
#define SYNTAX89_TEST_H

#include <stdio.h>

#include "syntax89.h"

/* Minimal test harness shared by every suite. The support translation unit
 * owns the failure counter; a test program ends with syntax89_test_report. */

extern int syntax89_test_failures;

void syntax89_test_fail(const char *file, int line, const char *msg);
void syntax89_test_eq_ul(unsigned long got, unsigned long want,
                         const char *file, int line);
void syntax89_test_eq_long(long got, long want, const char *file, int line);
int syntax89_test_report(const char *name);

#define T_ASSERT(cond)                                                         \
    do                                                                         \
    {                                                                          \
        if (!(cond))                                                           \
        {                                                                      \
            syntax89_test_fail(__FILE__, __LINE__, #cond);                     \
        }                                                                      \
    } while (0)

#define T_EQ_UL(got, want)                                                     \
    syntax89_test_eq_ul((unsigned long)(got), (unsigned long)(want), __FILE__, \
                        __LINE__)

#define T_EQ_LONG(got, want)                                                   \
    syntax89_test_eq_long((long)(got), (long)(want), __FILE__, __LINE__)

#define T_OK(expr) T_EQ_LONG((long)(expr), (long)SYNTAX89_OK)

#endif
