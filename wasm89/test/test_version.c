#include <stdio.h>
#include "wasm89.h"

int main(void)
{
    if (w89_version() != 1) {
        fprintf(stderr, "FAIL: w89_version() returned %d, expected 1\n",
                w89_version());
        return 1;
    }
    printf("PASS: w89_version() == 1\n");
    return 0;
}
