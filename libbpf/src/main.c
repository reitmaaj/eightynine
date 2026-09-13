#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bpf.h"
#include "decode.h"
#include "eval.h"
#include "print.h"
#include "validate.h"

#define BPF_MAX_INS 4096
#define BPF_MEM_SZ 4096
#define GROUPS_CAP 256

static int read_file(const char *path, bpf_byte **out, bpf_u32 *len)
{
    FILE *f;
    long sz;
    size_t z;
    bpf_byte *buf;
    size_t got;
    int rc;
    bpf_u32 b;

    f = fopen(path, "rb");
    if (f == 0)
    {
        return -1;
    }
    rc = fseek(f, 0, SEEK_END);
    if (rc != 0)
    {
        fclose(f);
        return -1;
    }
    sz = ftell(f);
    if (sz < 0)
    {
        fclose(f);
        return -1;
    }
    rc = fseek(f, 0, SEEK_SET);
    if (rc != 0)
    {
        fclose(f);
        return -1;
    }
    z = (size_t)sz;
    buf = malloc(z);
    if (buf == 0)
    {
        fclose(f);
        return -1;
    }
    got = fread(buf, 1, z, f);
    fclose(f);
    if (got != z)
    {
        free(buf);
        return -1;
    }
    *out = buf;
    b = (bpf_u32)sz;
    *len = b;
    return 0;
}

static int do_run(const char *path)
{
    bpf_byte *buf;
    bpf_u32 len;
    bpf_insn ins[BPF_MAX_INS];
    bpf_u32 n;
    bpf_u32 conf;
    bpf_err e;
    bpf_machine m;
    bpf_status st;
    bpf_byte mem[BPF_MEM_SZ];
    int w;
    int i;
    const char *nm;
    bpf_u64 r0;
    bpf_u32 hp;

    i = read_file(path, &buf, &len);
    if (i != 0)
    {
        fprintf(stderr, "bpf: cannot read %s\n", path);
        return 1;
    }
    e = bpf_decode(buf, len, ins, BPF_MAX_INS, &n);
    if (e != BPF_OK)
    {
        nm = bpf_err_name(e);
        fprintf(stderr, "bpf: decode: %s\n", nm);
        free(buf);
        return 1;
    }
    e = bpf_validate(ins, n, &conf);
    if (e != BPF_OK)
    {
        nm = bpf_err_name(e);
        fprintf(stderr, "bpf: validate: %s\n", nm);
        free(buf);
        return 1;
    }
    for (i = 0; i < BPF_MEM_SZ; ++i)
    {
        mem[i] = 0;
    }
    bpf_machine_init(&m, ins, n);
    m.mem = mem;
    m.mem_size = BPF_MEM_SZ;
    st = BPF_STAT_RUNNING;
    while (st == BPF_STAT_RUNNING)
    {
        st = bpf_step(&m);
    }
    if (st == BPF_STAT_RETURNED)
    {
        r0 = m.regs.r[0];
        w = printf("%lu\n", r0);
        (void)w;
        free(buf);
        return 0;
    }
    if (st == BPF_STAT_HOSTCALL)
    {
        hp = m.helper;
        fprintf(stderr, "bpf: unsupported helper %u\n", hp);
    }
    if (st == BPF_STAT_TRAP)
    {
        fprintf(stderr, "bpf: out-of-bounds memory access\n");
    }
    if (st == BPF_STAT_EXHAUSTED)
    {
        fprintf(stderr, "bpf: step budget exhausted\n");
    }
    if (st == BPF_STAT_ERR)
    {
        fprintf(stderr, "bpf: program error (fell off end or stack)\n");
    }
    free(buf);
    return 1;
}

static int do_dis(const char *path)
{
    bpf_byte *buf;
    bpf_u32 len;
    bpf_insn ins[BPF_MAX_INS];
    bpf_u32 n;
    bpf_err e;
    bpf_u32 i;
    char line[128];
    int w;
    int rc;
    const char *nm;
    bpf_byte opcode;
    bpf_byte rb;
    bpf_byte src;
    bpf_byte dst;
    int off;
    int imm;
    unsigned uoff;
    unsigned uimm;
    const bpf_insn *pi;

    rc = read_file(path, &buf, &len);
    if (rc != 0)
    {
        fprintf(stderr, "bpf: cannot read %s\n", path);
        return 1;
    }
    e = bpf_decode(buf, len, ins, BPF_MAX_INS, &n);
    if (e != BPF_OK)
    {
        nm = bpf_err_name(e);
        fprintf(stderr, "bpf: decode: %s\n", nm);
        free(buf);
        return 1;
    }
    i = 0;
    while (i < n)
    {
        opcode = ins[i].opcode;
        off = ins[i].offset;
        imm = ins[i].imm;
        src = ins[i].src_reg;
        dst = ins[i].dst_reg;
        rb = bpf_regs_byte(src, dst);
        uoff = (unsigned)(off & 0xFFFF);
        uimm = (unsigned)imm;
        pi = &ins[i];
        bpf_dis_one(pi, line);
        w = printf("%03u: %02x %02x %04x %08x  %s\n", i, opcode, rb, uoff, uimm,
                   line);
        (void)w;
        i = i + 1;
    }
    free(buf);
    return 0;
}

static int do_groups(const char *path)
{
    bpf_byte *buf;
    bpf_u32 len;
    bpf_insn ins[BPF_MAX_INS];
    bpf_u32 n;
    bpf_u32 conf;
    bpf_err e;
    char groups[GROUPS_CAP];
    int rc;
    const char *nm;
    int w;

    rc = read_file(path, &buf, &len);
    if (rc != 0)
    {
        fprintf(stderr, "bpf: cannot read %s\n", path);
        return 1;
    }
    e = bpf_decode(buf, len, ins, BPF_MAX_INS, &n);
    if (e != BPF_OK)
    {
        nm = bpf_err_name(e);
        fprintf(stderr, "bpf: decode: %s\n", nm);
        free(buf);
        return 1;
    }
    e = bpf_validate(ins, n, &conf);
    if (e != BPF_OK)
    {
        nm = bpf_err_name(e);
        fprintf(stderr, "bpf: validate: %s\n", nm);
        free(buf);
        return 1;
    }
    groups[0] = '\0';
    bpf_groups_into(conf, groups, GROUPS_CAP);
    w = printf("%s", groups);
    (void)w;
    free(buf);
    return 0;
}

static void print_usage(void)
{
    int rc;

    rc = fprintf(stderr, "usage: bpf version\n"
                         "       bpf run <file>\n"
                         "       bpf dis <file>\n"
                         "       bpf groups <file>\n");
    (void)rc;
}

int main(int argc, char **argv)
{
    int have_arg;
    int is_version;
    int is_run;
    int is_dis;
    int is_groups;
    int cmp;
    char *cmd;
    char *path;
    int rc;

    have_arg = argc >= 2;
    if (!have_arg)
    {
        print_usage();
        return 1;
    }
    cmd = argv[1];
    cmp = strcmp(cmd, "version");
    is_version = cmp == 0;
    cmp = strcmp(cmd, "run");
    is_run = cmp == 0;
    cmp = strcmp(cmd, "dis");
    is_dis = cmp == 0;
    cmp = strcmp(cmd, "groups");
    is_groups = cmp == 0;
    if (is_version)
    {
        int v;
        int w;

        v = bpf_version();
        w = printf("%d\n", v);
        (void)w;
        return 0;
    }
    have_arg = argc >= 3;
    if (!have_arg)
    {
        print_usage();
        return 1;
    }
    path = argv[2];
    rc = 1;
    if (is_run)
    {
        rc = do_run(path);
    }
    if (is_dis)
    {
        rc = do_dis(path);
    }
    if (is_groups)
    {
        rc = do_groups(path);
    }
    if (rc == 1)
    {
        if (!is_run)
        {
            if (!is_dis)
            {
                if (!is_groups)
                {
                    print_usage();
                }
            }
        }
    }
    return rc;
}
