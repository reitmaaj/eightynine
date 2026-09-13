#include <stdio.h>

#include "bpf.h"

int main(void)
{
    if (bpf_version() != 1)
    {
        (void)fprintf(stderr, "test_version: bpf_version != 1\n");
        return 1;
    }
    (void)printf("ok: bpf_version() == %d\n", bpf_version());
    return 0;
}
