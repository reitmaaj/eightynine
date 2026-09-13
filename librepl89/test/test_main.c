#include <stdio.h>

#include "test.h"

int failures = 0;
int checks = 0;

void rp_check(int cond, const char *what)
{
    checks = checks + 1;
    if (!cond) {
        failures = failures + 1;
        fprintf(stderr, "FAIL: %s\n", what);
    }
}

void rp_check_ctx(int cond, const char *what, const char *ctx)
{
    checks = checks + 1;
    if (!cond) {
        failures = failures + 1;
        fprintf(stderr, "FAIL: %s [%s]\n", what, ctx);
    }
}

static size_t motion_escape(const char *p, size_t n, size_t i)
{
    size_t j;

    if (i + 2 > n) {
        return 0;
    }
    if (p[i] != 0x1B || p[i + 1] != '[') {
        return 0;
    }
    j = i + 2;
    while (j < n && p[j] >= '0' && p[j] <= '9') {
        j = j + 1;
    }
    if (j >= n) {
        return 0;
    }
    if (p[j] != 'A' && p[j] != 'B' && p[j] != 'C') {
        return 0;
    }
    return j + 1;
}

int rp_only_motion(const char *p, size_t n)
{
    size_t i;
    size_t next;

    i = 0;
    while (i < n) {
        if (p[i] == '\r') {
            i = i + 1;
        } else if (p[i] == 0x1B) {
            next = motion_escape(p, n, i);
            if (next == 0) {
                return 0;
            }
            i = next;
        } else {
            return 0;
        }
    }
    return 1;
}

void test_buf(void);
void test_edit(void);
void test_history(void);
void test_render(void);
void test_key(void);
void test_paste(void);
void test_tty(void);
void test_session(void);
void test_fault(void);
void test_example(void);

int main(void)
{
    test_buf();
    test_edit();
    test_history();
    test_render();
    test_key();
    test_paste();
    test_tty();
    test_session();
    test_fault();
    test_example();
    if (failures) {
        fprintf(stderr, "%d/%d checks FAILED\n", failures, checks);
        return 1;
    }
    printf("all %d checks passed\n", checks);
    return 0;
}
