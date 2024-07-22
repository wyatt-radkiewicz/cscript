#include <stdio.h>

#include "cnm.h"

void print_val(const cnm_val *val) {
    if (!val) {
        printf("nullval\n");
        return;
    }
    switch (val->type) {
    case CNM_TYPE_INT: printf("int: %ld\n", val->val.i); break;
    case CNM_TYPE_UINT: printf("uint: %lu\n", val->val.ui); break;
    case CNM_TYPE_FP: printf("fp: %f\n", val->val.fp); break;
    case CNM_TYPE_STR: printf("str: %s\n", val->val.str); break;
    case CNM_TYPE_HNDL: printf("hndl: %p\n", val->val.hndl); break;
    case CNM_TYPE_REF: printf("ref of: "); print_val(val->val.ref); break;
    case CNM_TYPE_BUF: printf("buf size: %zu\n", val->val.bufsz); break;
    case CNM_TYPE_PFN: printf("pfn id: %d\n", val->val.pfn); break;
    }
}

int main(int argc, char **argv) {
    printf("cnm script tester\n");

    uint8_t buf[256] = {0};
    if (!cnm_compile(buf, sizeof(buf), "{ i = 3; i = j = 4; }", NULL, 0)) {
        printf("compilation failed\n");
        return 1;
    }
    //cnm_compile(buf, sizeof(buf), "4 + 5", NULL, 0);
    const cnm_val *ret = cnm_run_ex(buf, 0, NULL, NULL, 0);
    print_val(ret);

    return 0;
}

