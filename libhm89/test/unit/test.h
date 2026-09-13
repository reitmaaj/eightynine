#ifndef TEST_H
#define TEST_H

/* test.h - tiny C89 test harness for libhm89 unit programs.
 * Not part of the library; not green-linted (tests are not under
 * project_roots). Must still compile warning-clean as strict C89. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <hm.h>

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

struct test_capture
{
    char *mem;
    size_t used;
    size_t cap;
};

static int test_capture_write(void *userdata, const char *data, size_t size)
{
    struct test_capture *c;
    char *nm;
    size_t nc;
    size_t i;
    c = userdata;
    if (c->used + size + 1 > c->cap)
    {
        nc = c->used + size + 1;
        nm = realloc(c->mem, nc);
        if (nm == NULL)
        {
            return 0;
        }
        c->mem = nm;
        c->cap = nc;
    }
    for (i = 0; i < size; ++i)
    {
        c->mem[c->used + i] = data[i];
    }
    c->used = c->used + size;
    c->mem[c->used] = '\0';
    return 1;
}

static char *test_type_str(hm_type *type)
{
    struct test_capture c;
    c.mem = NULL;
    c.used = 0;
    c.cap = 0;
    hm_type_write(type, test_capture_write, &c);
    if (c.mem == NULL)
    {
        c.mem = malloc(1);
        if (c.mem != NULL)
        {
            c.mem[0] = '\0';
        }
    }
    return c.mem;
}

static char *test_scheme_str(const hm_scheme *scheme)
{
    struct test_capture c;
    c.mem = NULL;
    c.used = 0;
    c.cap = 0;
    hm_scheme_write(scheme, test_capture_write, &c);
    if (c.mem == NULL)
    {
        c.mem = malloc(1);
        if (c.mem != NULL)
        {
            c.mem[0] = '\0';
        }
    }
    return c.mem;
}

static int test_type_matches(hm_type *type, const char *expected)
{
    char *s;
    int match;
    s = test_type_str(type);
    if (s == NULL)
    {
        return 0;
    }
    match = (strcmp(s, expected) == 0);
    free(s);
    return match;
}

#define CHECK_TYPE(type, expected) \
    do \
    { \
        if (test_type_matches((type), (expected)) == 0) \
        { \
            fprintf(stderr, "type mismatch %s:%d: want [%s]\n", \
                __FILE__, __LINE__, (expected)); \
            ++test_failures; \
        } \
    } while (0)

#define TEST_MAIN \
    int main(void) \
    { \
        if (test_failures != 0) \
        { \
            fprintf(stderr, "%d check(s) failed\n", test_failures); \
            return 1; \
        } \
        printf("ok\n"); \
        return 0; \
    }

#endif /* TEST_H */
