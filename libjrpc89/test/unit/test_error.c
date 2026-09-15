/* test_error.c - unit tests for jrpc89_error_code_reserved. */
#include <stdio.h>

#include <jrpc89.h>

static int failures;

static void expect_reserved(j89_int code)
{
    if (!jrpc89_error_code_reserved(code))
    {
        fprintf(stderr, "FAIL: %.0f should be reserved\n", code);
        failures = failures + 1;
    }
}

static void expect_application(j89_int code)
{
    if (jrpc89_error_code_reserved(code))
    {
        fprintf(stderr, "FAIL: %.0f should not be reserved\n", code);
        failures = failures + 1;
    }
}

static void test_named_codes(void)
{
    expect_reserved(JRPC89_PARSE_ERROR);
    expect_reserved(JRPC89_INVALID_REQUEST);
    expect_reserved(JRPC89_METHOD_NOT_FOUND);
    expect_reserved(JRPC89_INVALID_PARAMS);
    expect_reserved(JRPC89_INTERNAL_ERROR);
}

static void test_range(void)
{
    expect_application(-32769);
    expect_reserved(-32768);
    expect_reserved(-32500);
    expect_reserved(-32100);
    expect_reserved(JRPC89_RESERVED_MIN);
    expect_reserved(JRPC89_RESERVED_MAX);
    expect_reserved(-32099);
    expect_reserved(-32000);
    expect_application(-31999);
    expect_application(-1);
    expect_application(0);
    expect_application(1);
    expect_application(1234);
    expect_application(2147483648.0);
}

int main(void)
{
    test_named_codes();
    test_range();
    if (failures != 0)
    {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_error: ok\n");
    return 0;
}
