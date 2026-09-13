#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wasm89.h"
#include "module.h"
#include "validate.h"
#include "repl.h"

static void usage(FILE *f)
{
    fprintf(f, "usage: wasm89 version\n");
    fprintf(f, "       wasm89 load <file.wasm>\n");
    fprintf(f, "       wasm89 repl\n");
}

static int load_file(const char *path)
{
    FILE *fp;
    long size;
    w89_byte *buf;
    w89_module m;
    w89_err e;
    w89_u32 bsize;
    size_t want;
    size_t got;
    long alloc;
    unsigned int op;
    int opcode;
    int result = 1;
    const char *msg;
    int rc;

    fp = fopen(path, "rb");
    if (fp == 0) {
        fprintf(stderr, "error: cannot open %s\n", path);
        return 1;
    }
    rc = fseek(fp, 0, SEEK_END);
    if (rc != 0) {
        fclose(fp);
        return 1;
    }
    size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return 1;
    }
    rc = fseek(fp, 0, SEEK_SET);
    if (rc != 0) {
        fclose(fp);
        return 1;
    }
    if (size > 0) {
        alloc = size;
    } else {
        alloc = 1;
    }
    want = (size_t)alloc;
    buf = malloc(want);
    if (buf == 0) {
        fclose(fp);
        return 1;
    }
    got = fread(buf, 1, want, fp);
    if (got != want) {
        fclose(fp);
        free(buf);
        return 1;
    }
    fclose(fp);

    w89_module_init(&m);
    bsize = (w89_u32)size;
    e = w89_module_decode(buf, bsize, &m);
    if (e == W89_ERR_NONE) {
        e = w89_module_validate(&m);
        if (e == W89_ERR_NONE) {
            printf("ok\n");
            result = 0;
        } else {
            msg = w89_validate_message();
            printf("error: %s\n", msg);
        }
    } else if (e == W89_ERR_UNKNOWN_OPCODE) {
        opcode = w89_last_illegal();
        op = (unsigned int)opcode;
        printf("error: illegal opcode %02x\n", op);
    } else {
        msg = w89_err_message(e);
        printf("error: %s\n", msg);
    }
    w89_module_free(&m);
    free(buf);
    return result;
}

int main(int argc, char *argv[])
{
    const char *arg;
    int cmp;
    int ver;

    if (argc == 2) {
        arg = argv[1];
        cmp = strcmp(arg, "version");
        if (cmp == 0) {
            ver = w89_version();
            printf("%d\n", ver);
            return 0;
        }
        cmp = strcmp(arg, "repl");
        if (cmp == 0) {
            return w89_repl_main();
        }
    }
    if (argc == 3) {
        arg = argv[1];
        cmp = strcmp(arg, "load");
        if (cmp == 0) {
            arg = argv[2];
            return load_file(arg);
        }
    }
    usage(stderr);
    return 1;
}
