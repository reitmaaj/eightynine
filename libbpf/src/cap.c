#include <stddef.h>
#include <stdlib.h>

#include "cap.h"

struct bpf_caps
{
    bpf_u32 max;
    bpf_u32 next;
    bpf_byte *live;
    bpf_u32 *type;
    bpf_u32 *rights;
    void **host;
};

bpf_err bpf_caps_create(bpf_u32 max, bpf_caps **out)
{
    bpf_caps *c;
    bpf_byte *lv;
    bpf_u32 *ty;
    bpf_u32 *rt;
    void **hs;
    bpf_u32 i;
    size_t sz;

    c = malloc(sizeof(bpf_caps));
    if (c == 0)
    {
        return BPF_EFULL;
    }
    sz = max * sizeof(bpf_byte);
    lv = malloc(sz);
    if (lv == 0)
    {
        free(c);
        return BPF_EFULL;
    }
    sz = max * sizeof(bpf_u32);
    ty = malloc(sz);
    if (ty == 0)
    {
        free(lv);
        free(c);
        return BPF_EFULL;
    }
    rt = malloc(sz);
    if (rt == 0)
    {
        free(ty);
        free(lv);
        free(c);
        return BPF_EFULL;
    }
    sz = max * sizeof(void *);
    hs = malloc(sz);
    if (hs == 0)
    {
        free(rt);
        free(ty);
        free(lv);
        free(c);
        return BPF_EFULL;
    }
    c->max = max;
    c->next = 0;
    c->live = lv;
    c->type = ty;
    c->rights = rt;
    c->host = hs;
    i = 0;
    while (i < max)
    {
        lv[i] = 0;
        ty[i] = 0;
        rt[i] = 0;
        hs[i] = 0;
        i = i + 1;
    }
    *out = c;
    return BPF_OK;
}

void bpf_caps_destroy(bpf_caps *caps)
{
    bpf_byte *lv;
    bpf_u32 *ty;
    bpf_u32 *rt;
    void **hs;

    if (caps == 0)
    {
        return;
    }
    lv = caps->live;
    ty = caps->type;
    rt = caps->rights;
    hs = caps->host;
    free(lv);
    free(ty);
    free(rt);
    free(hs);
    free(caps);
}

static bpf_byte live_at(const bpf_caps *caps, bpf_u32 h)
{
    bpf_u32 max;
    const bpf_byte *lv;
    bpf_byte v;
    bpf_byte r;

    r = 0;
    max = caps->max;
    if (h < max)
    {
        lv = caps->live;
        v = lv[h];
        if (v != 0u)
        {
            r = 1;
        }
    }
    return r;
}

bpf_err bpf_caps_alloc(bpf_caps *caps, bpf_u32 type, bpf_u32 rights, void *host,
                       bpf_handle *out)
{
    bpf_u32 next;
    bpf_u32 max;
    bpf_byte *lv;
    bpf_u32 *ty;
    bpf_u32 *rt;
    void **hs;
    bpf_u32 nv;

    next = caps->next;
    max = caps->max;
    if (next >= max)
    {
        return BPF_EFULL;
    }
    lv = caps->live;
    ty = caps->type;
    rt = caps->rights;
    hs = caps->host;
    lv[next] = 1;
    ty[next] = type;
    rt[next] = rights;
    hs[next] = host;
    nv = next + 1;
    caps->next = nv;
    *out = next;
    return BPF_OK;
}

bpf_err bpf_caps_lookup(bpf_caps *caps, bpf_handle h, bpf_u32 *type,
                        bpf_u32 *rights, void **host)
{
    bpf_byte live;
    bpf_u32 *ty;
    bpf_u32 *rt;
    void **hs;
    bpf_u32 t;
    bpf_u32 r;
    void *hv;

    live = live_at(caps, h);
    if (!live)
    {
        return BPF_EHANDLE;
    }
    ty = caps->type;
    rt = caps->rights;
    hs = caps->host;
    t = ty[h];
    r = rt[h];
    hv = hs[h];
    if (type != 0)
    {
        *type = t;
    }
    if (rights != 0)
    {
        *rights = r;
    }
    if (host != 0)
    {
        *host = hv;
    }
    return BPF_OK;
}

bpf_err bpf_caps_revoke(bpf_caps *caps, bpf_handle h)
{
    bpf_byte live;
    bpf_byte *lv;

    live = live_at(caps, h);
    if (!live)
    {
        return BPF_EHANDLE;
    }
    lv = caps->live;
    lv[h] = 0;
    return BPF_OK;
}

bpf_u32 bpf_caps_live(const bpf_caps *caps)
{
    bpf_u32 max;
    const bpf_byte *lv;
    bpf_u32 i;
    bpf_u32 count;

    count = 0;
    max = caps->max;
    lv = caps->live;
    i = 0;
    while (i < max)
    {
        bpf_byte v;

        v = lv[i];
        if (v != 0u)
        {
            count = count + 1;
        }
        i = i + 1;
    }
    return count;
}
