/* alloc_main.c - deterministic allocation-failure sweep for the response
 * builders. Every allocation point must fail cleanly: JRPC89_ENOMEM, an
 * unchanged *out, and no usable node. */

#include <stdio.h>
#include <string.h>

#include <jrpc89.h>

#include "alloc_shim.h"

static int failures;

static void fail(const char *msg)
{
    fprintf(stderr, "FAIL: %s\n", msg);
    failures = failures + 1;
}

static void set_id_int(jrpc89_id *id, j89_int v)
{
    id->kind = JRPC89_ID_INT;
    id->num = v;
    id->str = NULL;
    id->len = 0;
}

static void sweep_result(void)
{
    int i;

    i = 0;
    for (;;)
    {
        j89_arena a;
        j89_len res;
        j89_len out;
        jrpc89_id id;
        jrpc89_status st;
        int fired;

        j89_arena_init(&a);
        set_id_int(&id, 1);
        res = j89_integer_new(&a, 42);
        out = 12345;
        alloc_arm(i);
        st = jrpc89_response_result_new(&a, &id, res, &out);
        fired = alloc_fired();
        alloc_disarm();
        if (fired == 0)
        {
            if (st != JRPC89_OK)
            {
                fail("result final attempt");
            }
            j89_arena_destroy(&a);
            break;
        }
        if (st != JRPC89_ENOMEM)
        {
            fail("result not ENOMEM");
        }
        if (out != 12345)
        {
            fail("result out changed");
        }
        j89_arena_destroy(&a);
        i = i + 1;
    }
    if (i == 0)
    {
        fail("result has no allocation point");
    }
}

static void sweep_error(void)
{
    int i;

    i = 0;
    for (;;)
    {
        j89_arena a;
        j89_len out;
        jrpc89_id id;
        jrpc89_status st;
        int fired;

        j89_arena_init(&a);
        set_id_int(&id, 1);
        out = 12345;
        alloc_arm(i);
        st = jrpc89_response_error_new(&a, &id, -32601, "message", 7, J89_BAD,
                                       &out);
        fired = alloc_fired();
        alloc_disarm();
        if (fired == 0)
        {
            if (st != JRPC89_OK)
            {
                fail("error final attempt");
            }
            j89_arena_destroy(&a);
            break;
        }
        if (st != JRPC89_ENOMEM)
        {
            fail("error not ENOMEM");
        }
        if (out != 12345)
        {
            fail("error out changed");
        }
        if (!j89_failed(&a))
        {
            fail("error arena not failed");
        }
        j89_arena_destroy(&a);
        i = i + 1;
    }
    if (i == 0)
    {
        fail("error has no allocation point");
    }
}

int main(void)
{
    sweep_result();
    sweep_error();
    if (failures != 0)
    {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("alloc_main: ok\n");
    return 0;
}
