#ifndef CAT89_TEST_H
#define CAT89_TEST_H

/* cat89_test.h - minimal dependency-free test harness (shared by unit tests).
 */

#include <stdio.h>

extern int cat89_test_failures;

#define T_START() cat89_test_failures = 0
#define T_END() (cat89_test_failures == 0)

#define T_ASSERT(expr)                                                         \
    do                                                                         \
    {                                                                          \
        if (!(expr))                                                           \
        {                                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);    \
            cat89_test_failures = cat89_test_failures + 1;                     \
        }                                                                      \
    } while (0)

#define T_STATUS(expr, expected)                                               \
    do                                                                         \
    {                                                                          \
        cat89_status cat89_st_ = (expr);                                       \
        if (cat89_st_ != (expected))                                           \
        {                                                                      \
            fprintf(stderr, "FAIL %s:%d: %s => %s (expected %s)\n", __FILE__,  \
                    __LINE__, #expr, cat89_status_name(cat89_st_),             \
                    cat89_status_name(expected));                              \
            cat89_test_failures = cat89_test_failures + 1;                     \
        }                                                                      \
    } while (0)

#define T_EQ_UL(a, b)                                                          \
    do                                                                         \
    {                                                                          \
        unsigned long cat89_a_ = (unsigned long)(a);                           \
        unsigned long cat89_b_ = (unsigned long)(b);                           \
        if (cat89_a_ != cat89_b_)                                              \
        {                                                                      \
            fprintf(stderr, "FAIL %s:%d: %s==%s (%lu != %lu)\n", __FILE__,     \
                    __LINE__, #a, #b, cat89_a_, cat89_b_);                     \
            cat89_test_failures = cat89_test_failures + 1;                     \
        }                                                                      \
    } while (0)

#endif
