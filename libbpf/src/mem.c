#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"

typedef struct mem_region
{
    bpf_off64 base;
    bpf_off64 len;
    bpf_byte acc;
    bpf_byte *backing;
} mem_region;

struct bpf_memory
{
    bpf_u32 n;
    mem_region *reg;
};

static bpf_byte kind_acc(bpf_byte kind)
{
    bpf_byte a;

    a = BPF_MEM_R;
    if (kind == BPF_RMEM)
    {
        a = a | BPF_MEM_W;
    }
    if (kind == BPF_RSTACK)
    {
        a = a | BPF_MEM_W;
    }
    return a;
}

static bpf_byte intervals_overlap(bpf_off64 a, bpf_off64 alen, bpf_off64 b,
                                  bpf_off64 blen)
{
    bpf_byte r;
    bpf_off64 ae;
    bpf_off64 be;

    r = 0;
    if (a <= b)
    {
        ae = a + alen;
        if (ae > b)
        {
            r = 1;
        }
    }
    else
    {
        be = b + blen;
        if (be > a)
        {
            r = 1;
        }
    }
    return r;
}

static bpf_byte base_ok(bpf_off64 base, bpf_off64 len)
{
    bpf_off64 zero;
    bpf_off64 rem;
    bpf_byte r;

    r = 0;
    zero = 0;
    rem = zero - base;
    if (len < rem)
    {
        r = 1;
    }
    return r;
}

bpf_err bpf_memory_create(const bpf_region_cfg *cfg, bpf_u32 n,
                          bpf_memory **out)
{
    bpf_memory *m;
    mem_region *reg;
    bpf_u32 i;
    bpf_u32 filled;
    bpf_err e;
    bpf_byte many;
    size_t need;

    many = 0;
    if (n > BPF_MEM_MAX)
    {
        many = 1;
    }
    if (many)
    {
        return BPF_EADDR;
    }
    need = n * sizeof(mem_region);
    m = malloc(sizeof(bpf_memory));
    if (m == 0)
    {
        return BPF_EADDR;
    }
    reg = malloc(need);
    if (reg == 0)
    {
        free(m);
        return BPF_EADDR;
    }
    m->reg = reg;
    m->n = 0;
    filled = 0;
    i = 0;
    while (i < n)
    {
        const bpf_region_cfg *rc;
        bpf_byte kind;
        bpf_off64 base;
        bpf_off64 len;
        bpf_byte ov;
        bpf_byte *bk;
        bpf_u32 initlen;
        bpf_byte bo;
        bpf_byte ok;
        bpf_u32 j;

        rc = cfg + i;
        kind = rc->kind;
        base = rc->guest;
        len = rc->len;
        ok = 1;
        bo = base_ok(base, len);
        if (!bo)
        {
            ok = 0;
        }
        if (len == 0u)
        {
            ok = 0;
        }
        initlen = rc->init_len;
        if (initlen > len)
        {
            ok = 0;
        }
        j = 0;
        while (j < filled)
        {
            mem_region *pr;
            bpf_off64 ob;
            bpf_off64 ol;

            pr = reg + j;
            ob = pr->base;
            ol = pr->len;
            ov = intervals_overlap(base, len, ob, ol);
            if (ov)
            {
                ok = 0;
            }
            j = j + 1;
        }
        if (!ok)
        {
            bpf_u32 k;

            k = 0;
            while (k < filled)
            {
                mem_region *pr;
                bpf_byte *fb;

                pr = reg + k;
                fb = pr->backing;
                free(fb);
                k = k + 1;
            }
            free(reg);
            free(m);
            return BPF_EADDR;
        }
        bk = malloc(len);
        if (bk == 0)
        {
            bpf_u32 k;

            k = 0;
            while (k < filled)
            {
                mem_region *pr;
                bpf_byte *fb;

                pr = reg + k;
                fb = pr->backing;
                free(fb);
                k = k + 1;
            }
            free(reg);
            free(m);
            return BPF_EADDR;
        }
        if (initlen != 0u)
        {
            const bpf_byte *ip;

            ip = rc->init;
            if (ip != 0)
            {
                memcpy(bk, ip, initlen);
            }
        }
        {
            bpf_byte *q;
            bpf_off64 z;
            bpf_byte zero;

            q = bk + initlen;
            z = len - initlen;
            zero = 0;
            while (z != 0u)
            {
                *q = zero;
                q = q + 1;
                z = z - 1;
            }
        }
        {
            mem_region *nr;
            bpf_byte acc_k;
            bpf_u32 nxt;

            nr = reg + filled;
            nr->base = base;
            nr->len = len;
            acc_k = kind_acc(kind);
            nr->acc = acc_k;
            nr->backing = bk;
            nxt = filled + 1;
            filled = nxt;
            m->n = nxt;
        }
        i = i + 1;
    }
    (void)e;
    *out = m;
    return BPF_OK;
}

void bpf_memory_destroy(bpf_memory *m)
{
    mem_region *reg;
    bpf_u32 cnt;
    bpf_u32 j;

    if (m == 0)
    {
        return;
    }
    reg = m->reg;
    cnt = m->n;
    j = 0;
    while (j < cnt)
    {
        mem_region *pr;
        bpf_byte *bk;

        pr = reg + j;
        bk = pr->backing;
        free(bk);
        j = j + 1;
    }
    free(reg);
    free(m);
}

bpf_u32 bpf_memory_nregions(const bpf_memory *m)
{
    return m->n;
}

bpf_err bpf_mem_resolve(bpf_memory *m, bpf_off64 addr, bpf_off64 bytes,
                        bpf_byte acc, bpf_byte **host)
{
    mem_region *reg;
    bpf_u32 cnt;
    bpf_u32 j;

    reg = m->reg;
    cnt = m->n;
    j = 0;
    while (j < cnt)
    {
        mem_region *pr;
        bpf_off64 base;
        bpf_off64 len;
        bpf_byte racc;
        bpf_byte *bk;
        bpf_off64 delta;
        bpf_byte permok;

        pr = reg + j;
        base = pr->base;
        len = pr->len;
        racc = pr->acc;
        bk = pr->backing;
        if (addr >= base)
        {
            delta = addr - base;
            if (delta <= len)
            {
                if (bytes <= len - delta)
                {
                    permok = 0;
                    if ((racc & acc) == acc)
                    {
                        permok = 1;
                    }
                    if (permok)
                    {
                        bk = bk + delta;
                        *host = bk;
                        return BPF_OK;
                    }
                    return BPF_EPERM;
                }
            }
        }
        j = j + 1;
    }
    return BPF_EADDR;
}

static bpf_byte le_width_ok(bpf_byte width)
{
    bpf_byte r;

    r = 0;
    if (width == 1u)
    {
        r = 1;
    }
    if (width == 2u)
    {
        r = 1;
    }
    if (width == 4u)
    {
        r = 1;
    }
    if (width == 8u)
    {
        r = 1;
    }
    return r;
}

bpf_err bpf_mem_load(bpf_memory *m, bpf_off64 addr, bpf_byte width,
                     bpf_u64 *value)
{
    bpf_byte *h;
    bpf_err e;
    bpf_u64 got;
    bpf_byte ok;
    bpf_off64 w;

    ok = le_width_ok(width);
    if (!ok)
    {
        return BPF_ESIZE;
    }
    w = (bpf_off64)width;
    e = bpf_mem_resolve(m, addr, w, BPF_MEM_R, &h);
    if (e != BPF_OK)
    {
        return e;
    }
    got = 0;
    if (width == 1u)
    {
        bpf_byte c;

        c = h[0];
        got = (bpf_u64)c;
    }
    if (width == 2u)
    {
        bpf_u32 lo;
        bpf_u32 hi;
        bpf_u64 wlo;
        bpf_u64 whi;

        lo = h[0];
        hi = h[1];
        wlo = (bpf_u64)lo;
        whi = (bpf_u64)hi;
        got = wlo | (whi << 8);
    }
    if (width == 4u)
    {
        bpf_u32 b0;
        bpf_u32 b1;
        bpf_u32 b2;
        bpf_u32 b3;
        bpf_u64 w0;
        bpf_u64 w1;
        bpf_u64 w2;
        bpf_u64 w3;

        b0 = h[0];
        b1 = h[1];
        b2 = h[2];
        b3 = h[3];
        w0 = (bpf_u64)b0;
        w1 = (bpf_u64)b1;
        w2 = (bpf_u64)b2;
        w3 = (bpf_u64)b3;
        got = w0 | (w1 << 8) | (w2 << 16) | (w3 << 24);
    }
    if (width == 8u)
    {
        bpf_u32 k;
        bpf_u64 v;

        v = 0;
        k = 0;
        while (k < 8u)
        {
            bpf_byte c;
            bpf_u64 cc;
            bpf_u64 sh;
            bpf_u64 shb;

            c = h[k];
            cc = (bpf_u64)c;
            sh = (bpf_u64)k;
            shb = sh * 8u;
            v = v | (cc << shb);
            k = k + 1;
        }
        got = v;
    }
    *value = got;
    return BPF_OK;
}

bpf_err bpf_mem_store(bpf_memory *m, bpf_off64 addr, bpf_byte width,
                      bpf_u64 value)
{
    bpf_byte *h;
    bpf_err e;
    bpf_byte k;
    bpf_byte ok;
    bpf_off64 w;

    ok = le_width_ok(width);
    if (!ok)
    {
        return BPF_ESIZE;
    }
    w = (bpf_off64)width;
    e = bpf_mem_resolve(m, addr, w, BPF_MEM_W, &h);
    if (e != BPF_OK)
    {
        return e;
    }
    k = 0;
    while (k < width)
    {
        bpf_u64 sh;
        bpf_u64 shb;
        bpf_u64 shifted;
        bpf_byte out;
        bpf_u32 idx;

        sh = (bpf_u64)k;
        shb = sh * 8u;
        shifted = value >> shb;
        out = (bpf_byte)shifted;
        idx = k;
        h[idx] = out;
        k = k + 1;
    }
    return BPF_OK;
}
