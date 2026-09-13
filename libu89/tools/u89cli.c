/* u89cli.c - test harness: normalize hex input from stdin, emit hex out.
   Usage: u89cli MODE   (MODE: 0 NFC, 1 NFD, 2 NFKC, 3 NFKD)
   Reads a hex string (no spaces) from stdin, prints normalized hex. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/u89.h"

static unsigned char *hex_to_bytes(const char *h, size_t *n)
{
    size_t len;
    size_t i;
    unsigned char *b;

    len = strlen(h) / 2;
    b = (unsigned char *)malloc(len);
    if (b == 0) {
        return 0;
    }
    for (i = 0; i < len; i++) {
        unsigned int hi;
        unsigned int lo;
        int a;
        int b2;

        a = h[i * 2];
        b2 = h[i * 2 + 1];
        hi = (a >= '0' && a <= '9') ? (unsigned int)(a - '0')
             : (unsigned int)(a - 'a' + 10);
        lo = (b2 >= '0' && b2 <= '9') ? (unsigned int)(b2 - '0')
             : (unsigned int)(b2 - 'a' + 10);
        b[i] = (unsigned char)((hi << 4) | lo);
    }
    *n = len;
    return b;
}

int main(int argc, char **argv)
{
    char inbuf[4096];
    unsigned char *src;
    unsigned char *dst;
    u89_cp *work;
    size_t n;
    size_t cap;
    int mode;
    int r;
    size_t i;

    if (argc < 2) {
        return 2;
    }
    mode = atoi(argv[1]);
    while (fgets(inbuf, sizeof inbuf, stdin) != 0) {
        while (inbuf[strlen(inbuf) - 1] == '\n'
               || inbuf[strlen(inbuf) - 1] == '\r') {
            inbuf[strlen(inbuf) - 1] = '\0';
        }
        if (inbuf[0] == '\0') {
            printf("\n");
            continue;
        }
        src = hex_to_bytes(inbuf, &n);
        if (src == 0) {
            return 2;
        }
        cap = n * 72 + 16;
        dst = (unsigned char *)malloc(cap);
        if (dst == 0) {
            free(src);
            return 2;
        }
        work = (u89_cp *)malloc((n * 18 + 16) * sizeof(u89_cp));
        if (work == 0) {
            free(src);
            free(dst);
            return 2;
        }
        r = u89_normalize_ex(mode, src, n, dst, cap, work, n * 18 + 16);
        if (r < 0) {
            free(src);
            free(dst);
            free(work);
            return 3;
        }
        for (i = 0; i < (size_t)r; i++) {
            printf("%02x", dst[i]);
        }
        printf("\n");
        free(src);
        free(dst);
        free(work);
    }
    return 0;
}
