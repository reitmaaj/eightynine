#ifndef TEST_H
#define TEST_H

/* test.h - tiny C89 test harness for libledger89 test programs.
 * Not part of the library; not green-linted. Must still compile
 * warning-clean as strict C89. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ledger89.h>

static int test_failures;

#define CHECK(cond)                                                            \
    do                                                                         \
    {                                                                          \
        if (!(cond))                                                           \
        {                                                                      \
            fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__,    \
                    #cond);                                                    \
            ++test_failures;                                                   \
        }                                                                      \
    } while (0)

#define CHECK_EQ(a, b)                                                         \
    do                                                                         \
    {                                                                          \
        if ((a) != (b))                                                        \
        {                                                                      \
            fprintf(stderr, "CHECK failed %s:%d: %s != %s\n", __FILE__,        \
                    __LINE__, #a, #b);                                         \
            ++test_failures;                                                   \
        }                                                                      \
    } while (0)

static ledger89_u64 test_u64(unsigned long v)
{
    return ledger89_u64_from_u32((ledger89_u32)v);
}

#define CHECK_U64(a, b)                                                        \
    do                                                                         \
    {                                                                          \
        if (!ledger89_u64_equal((a), (b)))                                     \
        {                                                                      \
            fprintf(stderr, "CHECK failed %s:%d: %s != %s\n", __FILE__,        \
                    __LINE__, #a, #b);                                         \
            ++test_failures;                                                   \
        }                                                                      \
    } while (0)

#define TEST_END                                                               \
    if (test_failures != 0)                                                    \
    {                                                                          \
        fprintf(stderr, "%d check(s) failed\n", test_failures);                \
        return 1;                                                              \
    }                                                                          \
    printf("ok\n");                                                            \
    return 0;

#endif /* TEST_H */
