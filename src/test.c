#include <stdio.h>

#include "cnm.h"

int main(int argc, char **argv) {
    printf("cnm script tester\n");

    uint8_t buf[256];
    cnm_compile(buf, sizeof(buf), "14", NULL, 0);

    return 0;
}

