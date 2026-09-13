#include <stdio.h>
#include <string.h>

#include "print.h"

static int fails;
static int count;

static void t_err(bpf_err e, int nonzero, const char *msg)
{
    const char *s;

    s = bpf_err_name(e);
    count = count + 1;
    if (s == 0 || (nonzero && s[0] == '\0'))
    {
        (void)fprintf(stderr, "FAIL: %s\n", msg);
        fails = fails + 1;
    }
}

static void test_all_err_names(void)
{
    t_err(BPF_OK, 1, "ok name");
    t_err(BPF_ETRUNC, 1, "trunc name");
    t_err(BPF_ECOUNT, 1, "count name");
    t_err(BPF_EREG, 1, "reg name");
    t_err(BPF_EDEPRECATED, 1, "deprecated name");
    t_err(BPF_EIMM, 1, "imm name");
    t_err(BPF_EEND, 1, "end name");
    t_err(BPF_ENEG, 1, "neg name");
    t_err(BPF_EMOVSX, 1, "movsx name");
    t_err(BPF_ECALL, 1, "call name");
    t_err(BPF_EEXIT, 1, "exit name");
    t_err(BPF_ESIZE, 1, "size name");
}

static void test_err_names_distinct(void)
{
    const char *a;
    const char *b;
    int i;
    int j;

    for (i = 0; i <= 11; ++i)
    {
        for (j = i + 1; j <= 11; ++j)
        {
            a = bpf_err_name((bpf_err)i);
            b = bpf_err_name((bpf_err)j);
            count = count + 1;
            if (strcmp(a, b) == 0)
            {
                (void)fprintf(stderr, "FAIL: err names %d and %d equal\n", i,
                              j);
                fails = fails + 1;
            }
        }
    }
}

static void t_regs(bpf_byte src, bpf_byte dst, bpf_byte want)
{
    bpf_byte r;

    r = bpf_regs_byte(src, dst);
    count = count + 1;
    if (r != want)
    {
        (void)fprintf(stderr, "FAIL: regs %u,%u got=%u want=%u\n", src, dst, r,
                      want);
        fails = fails + 1;
    }
}

static void test_regs_byte(void)
{
    t_regs(0, 0, 0x00);
    t_regs(0, 1, 0x01);
    t_regs(1, 0, 0x10);
    t_regs(1, 1, 0x11);
    t_regs(5, 9, 0x59);
    t_regs(10, 10, 0xAA);
    t_regs(10, 0, 0xA0);
    t_regs(0, 10, 0x0A);
    t_regs(7, 7, 0x77);
    t_regs(15, 15, 0xFF);
    t_regs(15, 0, 0xF0);
    t_regs(0, 15, 0x0F);
}

int main(void)
{
    test_all_err_names();
    test_err_names_distinct();
    test_regs_byte();
    if (fails)
    {
        (void)fprintf(stderr, "test_print3: %d/%d failed\n", fails, count);
        return 1;
    }
    (void)printf("ok: test_print3 (%d cases)\n", count);
    return 0;
}
