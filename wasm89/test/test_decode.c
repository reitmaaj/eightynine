#include <stdio.h>
#include <string.h>
#include "module.h"

static int failures;

static void chk_err(const char *name, w89_err got, w89_err expect)
{
    if (got != expect) {
        fprintf(stderr, "FAIL: %s: got %d (%s), expected %d (%s)\n",
                name, (int)got, w89_err_message(got),
                (int)expect, w89_err_message(expect));
        failures++;
    }
}

static void decode_ok(const char *name, const w89_byte *b, w89_u32 n)
{
    w89_module m;
    w89_err e;
    w89_module_init(&m);
    e = w89_module_decode(b, n, &m);
    if (e != W89_ERR_NONE) {
        fprintf(stderr, "FAIL: %s: expected OK, got %d (%s)\n",
                name, (int)e, w89_err_message(e));
        failures++;
    }
    w89_module_free(&m);
}

static void decode_bad(const char *name, const w89_byte *b, w89_u32 n,
                       w89_err expect)
{
    w89_module m;
    w89_err e;
    w89_module_init(&m);
    e = w89_module_decode(b, n, &m);
    chk_err(name, e, expect);
    w89_module_free(&m);
}

static void decode_checked(const char *name, const w89_byte *b, w89_u32 n,
                           w89_u32 expect_types, w89_u32 expect_funcs)
{
    w89_module m;
    w89_err e;
    w89_module_init(&m);
    e = w89_module_decode(b, n, &m);
    if (e != W89_ERR_NONE) {
        fprintf(stderr, "FAIL: %s: expected OK, got %d (%s)\n",
                name, (int)e, w89_err_message(e));
        failures++;
        return;
    }
    if (m.nrectypes != expect_types || m.nfuncs != expect_funcs) {
        fprintf(stderr, "FAIL: %s: types=%lu funcs=%lu, expected %lu/%lu\n",
                name, (unsigned long)m.nrectypes, (unsigned long)m.nfuncs,
                (unsigned long)expect_types, (unsigned long)expect_funcs);
        failures++;
    }
    w89_module_free(&m);
}

int main(void)
{
    static const w89_byte hdr[8] = { 0x00, 0x61, 0x73, 0x6D,
                                     0x01, 0x00, 0x00, 0x00 };
    static const w89_byte bad_magic[8] = { 0x01, 0x61, 0x73, 0x6D,
                                           0x01, 0x00, 0x00, 0x00 };
    static const w89_byte bad_version[8] = { 0x00, 0x61, 0x73, 0x6D,
                                             0x02, 0x00, 0x00, 0x00 };
    w89_byte buf[256];
    w89_u32 n;

    decode_bad("empty module", hdr, 0, W89_ERR_EOF);
    decode_bad("short magic", hdr, 3, W89_ERR_EOF);
    decode_bad("short header", hdr, 7, W89_ERR_EOF);
    decode_bad("bad magic", bad_magic, 8, W89_ERR_MAGIC);
    decode_bad("bad version", bad_version, 8, W89_ERR_VERSION);
    decode_ok("empty module", hdr, 8);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x01; buf[n++] = 0x04; buf[n++] = 0x01; buf[n++] = 0x60;
    buf[n++] = 0x00; buf[n++] = 0x00;
    decode_checked("func type section", buf, n, 1, 0);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x01; buf[n++] = 0x05; buf[n++] = 0x01; buf[n++] = 0x60;
    buf[n++] = 0x00; buf[n++] = 0x00; buf[n++] = 0x00;
    decode_bad("section size mismatch", buf, n, W89_ERR_SECTION_SIZE);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x01; buf[n++] = 0x04; buf[n++] = 0x01; buf[n++] = 0x60;
    buf[n++] = 0x00; buf[n++] = 0x00;
    buf[n++] = 0x01; buf[n++] = 0x04; buf[n++] = 0x01; buf[n++] = 0x60;
    buf[n++] = 0x00; buf[n++] = 0x00;
    decode_bad("duplicate section id", buf, n, W89_ERR_TRAILING);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x01; buf[n++] = 0x04; buf[n++] = 0x01; buf[n++] = 0x60;
    buf[n++] = 0x00; buf[n++] = 0x00;
    buf[n++] = 0x00; buf[n++] = 0x02; buf[n++] = 0x01; buf[n++] = 0x61;
    decode_ok("custom section between", buf, n);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x01; buf[n++] = 0x04; buf[n++] = 0x01; buf[n++] = 0x60;
    buf[n++] = 0x00; buf[n++] = 0x00;
    buf[n++] = 0x7A; buf[n++] = 0x01; buf[n++] = 0x00;
    decode_bad("unknown section id", buf, n, W89_ERR_SECTION_ID);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x01; buf[n++] = 0x07; buf[n++] = 0x01; buf[n++] = 0x60;
    buf[n++] = 0x02; buf[n++] = 0x7F; buf[n++] = 0x7E;
    buf[n++] = 0x01; buf[n++] = 0x7D;
    decode_checked("func type params/results", buf, n, 1, 0);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x01; buf[n++] = 0x07; buf[n++] = 0x01; buf[n++] = 0x5F;
    buf[n++] = 0x02; buf[n++] = 0x7F; buf[n++] = 0x00;
    buf[n++] = 0x7E; buf[n++] = 0x01;
    decode_checked("struct type", buf, n, 1, 0);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x01; buf[n++] = 0x04; buf[n++] = 0x01; buf[n++] = 0x5E;
    buf[n++] = 0x77; buf[n++] = 0x01;
    decode_checked("array type", buf, n, 1, 0);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x01; buf[n++] = 0x06; buf[n++] = 0x01; buf[n++] = 0x4F;
    buf[n++] = 0x00; buf[n++] = 0x60; buf[n++] = 0x00; buf[n++] = 0x00;
    decode_checked("sub final func", buf, n, 1, 0);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x01; buf[n++] = 0x09; buf[n++] = 0x01; buf[n++] = 0x4E;
    buf[n++] = 0x02; buf[n++] = 0x60; buf[n++] = 0x00; buf[n++] = 0x00;
    buf[n++] = 0x60; buf[n++] = 0x00; buf[n++] = 0x00;
    decode_checked("rec type two subs", buf, n, 1, 0);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x01; buf[n++] = 0x03; buf[n++] = 0x01;
    buf[n++] = 0x4E; buf[n++] = 0x00;
    {
        w89_module m;
        w89_err e;
        w89_module_init(&m);
        e = w89_module_decode(buf, n, &m);
        if (e != W89_ERR_NONE) {
            fprintf(stderr, "FAIL: empty rec: decode %d\n", (int)e);
            failures++;
        } else if (m.nrectypes != 1 || m.rectypes[0].n != 0) {
            fprintf(stderr, "FAIL: empty rec: rectype shape\n");
            failures++;
        }
        w89_module_free(&m);
    }


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x01; buf[n++] = 0x06; buf[n++] = 0x01; buf[n++] = 0x60;
    buf[n++] = 0x01; buf[n++] = 0x63; buf[n++] = 0x70;
    buf[n++] = 0x00;
    decode_checked("ref null func param", buf, n, 1, 0);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x01; buf[n++] = 0x05; buf[n++] = 0x01; buf[n++] = 0x60;
    buf[n++] = 0x00; buf[n++] = 0x01; buf[n++] = 0x76;
    decode_bad("reserved valtype", buf, n, W89_ERR_MALFORMED_TYPE);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x01; buf[n++] = 0x07; buf[n++] = 0x01; buf[n++] = 0x4F;
    buf[n++] = 0x01; buf[n++] = 0x00; buf[n++] = 0x60; buf[n++] = 0x00;
    buf[n++] = 0x00;
    decode_checked("sub with supertype", buf, n, 1, 0);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x01; buf[n++] = 0x04; buf[n++] = 0x01; buf[n++] = 0x60;
    buf[n++] = 0x01; buf[n++] = 0x7F;
    decode_bad("truncated functype", buf, n, W89_ERR_EOF_SECTION);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x05; buf[n++] = 0x03; buf[n++] = 0x01; buf[n++] = 0x00;
    buf[n++] = 0x01;
    decode_checked("memory min", buf, n, 0, 0);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x05; buf[n++] = 0x03; buf[n++] = 0x01; buf[n++] = 0x02;
    buf[n++] = 0x01;
    decode_bad("bad limits flag", buf, n, W89_ERR_MALFORMED_LIMITS);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x05; buf[n++] = 0x04; buf[n++] = 0x01; buf[n++] = 0x01;
    buf[n++] = 0x05; buf[n++] = 0x04;
    decode_bad("min greater than max", buf, n, W89_ERR_MIN_GT_MAX);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x05; buf[n++] = 0x03; buf[n++] = 0x01; buf[n++] = 0x04;
    buf[n++] = 0x01;
    decode_checked("memory64", buf, n, 0, 0);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x02; buf[n++] = 0x07; buf[n++] = 0x01;
    buf[n++] = 0x01; buf[n++] = 0x6D; buf[n++] = 0x01; buf[n++] = 0x66;
    buf[n++] = 0x00; buf[n++] = 0x00;
    decode_checked("func import", buf, n, 0, 0);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x02; buf[n++] = 0x07; buf[n++] = 0x01;
    buf[n++] = 0x01; buf[n++] = 0x6D; buf[n++] = 0x01; buf[n++] = 0x66;
    buf[n++] = 0x05; buf[n++] = 0x00;
    decode_bad("bad import kind", buf, n, W89_ERR_MALFORMED_IMPORT_KIND);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x02; buf[n++] = 0x09; buf[n++] = 0x01;
    buf[n++] = 0x01; buf[n++] = 0x6D; buf[n++] = 0x01; buf[n++] = 0x66;
    buf[n++] = 0x01; buf[n++] = 0x70; buf[n++] = 0x00; buf[n++] = 0x00;
    decode_checked("table import", buf, n, 0, 0);

    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x02; buf[n++] = 0x08; buf[n++] = 0x01;
    buf[n++] = 0x01; buf[n++] = 0x6D; buf[n++] = 0x01; buf[n++] = 0x66;
    buf[n++] = 0x02; buf[n++] = 0x00; buf[n++] = 0x01;
    decode_checked("memory import", buf, n, 0, 0);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x02; buf[n++] = 0x08; buf[n++] = 0x01;
    buf[n++] = 0x01; buf[n++] = 0x6D; buf[n++] = 0x01; buf[n++] = 0x66;
    buf[n++] = 0x03; buf[n++] = 0x7F; buf[n++] = 0x00;
    decode_checked("global import", buf, n, 0, 0);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x07; buf[n++] = 0x09; buf[n++] = 0x02;
    buf[n++] = 0x01; buf[n++] = 0x65; buf[n++] = 0x00; buf[n++] = 0x00;
    buf[n++] = 0x01; buf[n++] = 0x65; buf[n++] = 0x00; buf[n++] = 0x01;
    decode_bad("duplicate export name", buf, n, W89_ERR_DUP_EXPORT);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x07; buf[n++] = 0x05; buf[n++] = 0x01;
    buf[n++] = 0x01; buf[n++] = 0x65; buf[n++] = 0x06; buf[n++] = 0x00;
    decode_bad("bad export kind", buf, n, W89_ERR_MALFORMED_EXPORT_KIND);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x03; buf[n++] = 0x02; buf[n++] = 0x01; buf[n++] = 0x00;
    buf[n++] = 0x0A; buf[n++] = 0x04; buf[n++] = 0x01;
    buf[n++] = 0x02; buf[n++] = 0x00; buf[n++] = 0x0B;
    decode_checked("func and code", buf, n, 0, 1);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x03; buf[n++] = 0x02; buf[n++] = 0x01; buf[n++] = 0x00;
    buf[n++] = 0x0A; buf[n++] = 0x01; buf[n++] = 0x00;
    decode_bad("func/code count mismatch", buf, n, W89_ERR_FUNC_CODE_LEN);



    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x0C; buf[n++] = 0x01; buf[n++] = 0x01;
    buf[n++] = 0x0A; buf[n++] = 0x01; buf[n++] = 0x00;
    decode_bad("data count mismatch", buf, n, W89_ERR_DATA_COUNT);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x0C; buf[n++] = 0x01; buf[n++] = 0x01;
    buf[n++] = 0x0B; buf[n++] = 0x04; buf[n++] = 0x01;
    buf[n++] = 0x01; buf[n++] = 0x01; buf[n++] = 0x41;
    decode_checked("data count + data", buf, n, 0, 0);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x0B; buf[n++] = 0x04; buf[n++] = 0x01;
    buf[n++] = 0x01; buf[n++] = 0x01; buf[n++] = 0x41;
    buf[n++] = 0x0C; buf[n++] = 0x01; buf[n++] = 0x01;
    decode_bad("data count after data", buf, n, W89_ERR_TRAILING);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x0C; buf[n++] = 0x01; buf[n++] = 0x01;
    buf[n++] = 0x01; buf[n++] = 0x04; buf[n++] = 0x01;
    buf[n++] = 0x60; buf[n++] = 0x00; buf[n++] = 0x00;
    decode_bad("datacnt then type", buf, n, W89_ERR_TRAILING);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x09; buf[n++] = 0x05; buf[n++] = 0x01;
    buf[n++] = 0x01; buf[n++] = 0x00; buf[n++] = 0x01; buf[n++] = 0x00;
    buf[n++] = 0x0C; buf[n++] = 0x01; buf[n++] = 0x00;
    buf[n++] = 0x0A; buf[n++] = 0x01; buf[n++] = 0x00;
    decode_checked("elem datacnt code", buf, n, 0, 0);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x09; buf[n++] = 0x07; buf[n++] = 0x01;
    buf[n++] = 0x00; buf[n++] = 0x41; buf[n++] = 0x00; buf[n++] = 0x0B;
    buf[n++] = 0x01; buf[n++] = 0x00;
    decode_checked("active elem", buf, n, 0, 0);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x09; buf[n++] = 0x05; buf[n++] = 0x01;
    buf[n++] = 0x01; buf[n++] = 0x00; buf[n++] = 0x01; buf[n++] = 0x00;
    decode_checked("passive elem", buf, n, 0, 0);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x0D; buf[n++] = 0x03; buf[n++] = 0x01;
    buf[n++] = 0x00; buf[n++] = 0x00;
    decode_checked("tag section", buf, n, 0, 0);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x06; buf[n++] = 0x06; buf[n++] = 0x01;
    buf[n++] = 0x7F; buf[n++] = 0x00; buf[n++] = 0x41; buf[n++] = 0x00;
    buf[n++] = 0x0B;
    decode_checked("global section", buf, n, 0, 0);


    memcpy(buf, hdr, 8); n = 8;
    buf[n++] = 0x09; buf[n++] = 0x0B; buf[n++] = 0x01;
    buf[n++] = 0x05; buf[n++] = 0x63; buf[n++] = 0x70;
    buf[n++] = 0x02;
    buf[n++] = 0xD2; buf[n++] = 0x00; buf[n++] = 0x0B;
    buf[n++] = 0xD2; buf[n++] = 0x01; buf[n++] = 0x0B;
    {
        w89_module m;
        w89_err e;
        w89_module_init(&m);
        e = w89_module_decode(buf, n, &m);
        if (e != W89_ERR_NONE) {
            fprintf(stderr, "FAIL: elem expr items: decode %d\n", (int)e);
            failures++;
        } else if (m.nelems != 1 || m.elems[0].exprs.n != 4 ||
                   m.elems[0].exprs.items[0].op != 0xD2 ||
                   m.elems[0].exprs.items[1].op != 0x0B ||
                   m.elems[0].exprs.items[2].op != 0xD2 ||
                   m.elems[0].exprs.items[3].op != 0x0B) {
            fprintf(stderr, "FAIL: elem expr item markers\n");
            failures++;
        }
        w89_module_free(&m);
    }

    if (failures) {
        fprintf(stderr, "%d decoder test(s) failed\n", failures);
        return 1;
    }
    printf("PASS: decoder tests\n");
    return 0;
}
