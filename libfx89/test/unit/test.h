#ifndef TEST_H
#define TEST_H

/* test.h - tiny C89 test harness for libfx89 unit programs.
 * Not part of the library; not green-linted. Must still compile
 * warning-clean as strict C89. */

#include <stdio.h>
#include <stdlib.h>

#include <fx89.h>

static int test_failures;

#define CHECK(cond) \
    do \
    { \
        if (!(cond)) \
        { \
            fprintf(stderr, "CHECK failed %s:%d: %s\n", \
                __FILE__, __LINE__, #cond); \
            ++test_failures; \
        } \
    } while (0)

#define CHECK_EQ(a, b) \
    do \
    { \
        if ((a) != (b)) \
        { \
            fprintf(stderr, "CHECK failed %s:%d: %s != %s\n", \
                __FILE__, __LINE__, #a, #b); \
            ++test_failures; \
        } \
    } while (0)

#define TEST_END \
    if (test_failures != 0) \
    { \
        fprintf(stderr, "%d check(s) failed\n", test_failures); \
        return 1; \
    } \
    printf("ok\n"); \
    return 0;

#endif /* TEST_H */
