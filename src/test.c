#include <stdio.h>

#include "cnm.h"

int main(int argc, char **argv) {
    printf("cnm script tester\n");

    uint8_t buf[256] = {0};
    cnm_compile(buf, sizeof(buf), "6 + 9", NULL, 0);
    cnm_run_ex(buf, 0, NULL, NULL, 0);

    return 0;
}

