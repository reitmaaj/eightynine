#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test.h"

int failures = 0;
int checks = 0;

void u89_check(int cond, const char *what)
{
    checks++;
    if (!cond) {
        failures++;
        fprintf(stderr, "FAIL: %s\n", what);
    }
}

void u89_check_ctx(int cond, const char *what, const char *ctx)
{
    checks++;
    if (!cond) {
        failures++;
        fprintf(stderr, "FAIL: %s [%s]\n", what, ctx);
    }
}

void test_utf8(void);
void test_grapheme(void);
void test_xid(void);
void test_id(void);
void test_props(void);
void test_casefold(void);
void test_width(void);
void test_nfc(void);
void test_consistency(void);

int main(void)
{
    test_utf8();
    test_grapheme();
    test_xid();
    test_id();
    test_props();
    test_casefold();
    test_width();
    test_nfc();
    test_consistency();
    if (failures) {
        fprintf(stderr, "%d/%d checks FAILED\n", failures, checks);
        return 1;
    }
    printf("all %d checks passed\n", checks);
    return 0;
}
