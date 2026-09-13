#include <stdio.h>
#include <string.h>

#include "print.h"

static int fails;
static int count;

static void check(bpf_u32 conf, const char *want)
{
    char out[256];
    int cmp;

    out[0] = '\0';
    bpf_groups_into(conf, out, sizeof(out));
    count = count + 1;
    cmp = strcmp(out, want);
    if (cmp != 0)
    {
        (void)fprintf(stderr, "FAIL: conf=%x got=[%s] want=[%s]\n", conf, out,
                      want);
        fails = fails + 1;
    }
}

static void test_groups(void)
{
    check(0, "base32\n");
    check(BPF_CONF_BASE32, "base32\n");
    check(BPF_CONF_BASE64, "base32\nbase64\n");
    check(BPF_CONF_BASE32 | BPF_CONF_BASE64, "base32\nbase64\n");
    check(BPF_CONF_DIVMUL32, "base32\ndivmul32\n");
    check(BPF_CONF_DIVMUL64, "base32\ndivmul64\n");
    check(BPF_CONF_ATOMIC32, "base32\natomic32\n");
    check(BPF_CONF_ATOMIC64, "base32\natomic64\n");
    check(BPF_CONF_PACKET, "base32\npacket\n");
    check(BPF_CONF_BASE64 | BPF_CONF_DIVMUL64 | BPF_CONF_ATOMIC64,
          "base32\nbase64\ndivmul64\natomic64\n");
    check(BPF_CONF_BASE64 | BPF_CONF_DIVMUL32 | BPF_CONF_ATOMIC32,
          "base32\nbase64\ndivmul32\natomic32\n");
    check(BPF_CONF_BASE32 | BPF_CONF_BASE64 | BPF_CONF_DIVMUL32 |
              BPF_CONF_DIVMUL64 | BPF_CONF_ATOMIC32 | BPF_CONF_ATOMIC64 |
              BPF_CONF_PACKET,
          "base32\nbase64\ndivmul32\ndivmul64\natomic32\natomic64\npacket\n");
}

static void test_tiny_buffer(void)
{
    char out[4];
    size_t used;

    out[0] = '\0';
    bpf_groups_into(BPF_CONF_BASE32 | BPF_CONF_BASE64, out, sizeof(out));
    used = strlen(out);
    count = count + 1;
    if (strcmp(out, "bas") != 0 && strcmp(out, "ba") != 0 &&
        strcmp(out, "") != 0)
    {
        (void)fprintf(stderr, "FAIL: tiny buffer overflow-ish [%s]\n", out);
        fails = fails + 1;
    }
    if (used > sizeof(out) - 1)
    {
        (void)fprintf(stderr, "FAIL: tiny buffer wrote past\n");
        fails = fails + 1;
    }
}

int main(void)
{
    test_groups();
    test_tiny_buffer();
    if (fails)
    {
        (void)fprintf(stderr, "test_groups: %d/%d failed\n", fails, count);
        return 1;
    }
    (void)printf("ok: test_groups (%d cases)\n", count);
    return 0;
}
