/* smoke.c - one end-to-end path through libbwt89: transform a binary sample
 * with bwt89_bwt and recover it exactly with bwt89_ibwt. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bwt89.h>

int main(void)
{
    static const unsigned char sample[] = {0x00, 0xff, 'h', 'e', 'l', 'l', 'o',
                                           0x00, 0x0a, 'w', 'o', 'r', 'l', 'd',
                                           0x00, 0x80, 'b', 'y', 't', 'e'};
    unsigned char transformed[20];
    unsigned char recovered[20];
    size_t index;
    enum bwt89_status st;
    st = bwt89_bwt(sample, sizeof(sample), &index, transformed);
    if (st != BWT89_OK)
    {
        fprintf(stderr, "smoke: bwt89_bwt failed (%d)\n", (int)st);
        return 1;
    }
    if (index >= sizeof(sample))
    {
        fprintf(stderr, "smoke: index out of range\n");
        return 1;
    }
    st = bwt89_ibwt(transformed, sizeof(sample), index, recovered);
    if (st != BWT89_OK)
    {
        fprintf(stderr, "smoke: bwt89_ibwt failed (%d)\n", (int)st);
        return 1;
    }
    if (memcmp(sample, recovered, sizeof(sample)) != 0)
    {
        fprintf(stderr, "smoke: round trip mismatch\n");
        return 1;
    }
    printf("smoke ok\n");
    return 0;
}
