#include <stdint.h>
#include <string.h>

#include "cnm.h"

#define arrlen(a) (sizeof(a) / sizeof((a)[0])

static struct cnm_type *g_types;
static size_t g_ntypes, g_maxtypes;
static cnm_error_cb g_errcb;

//enum opcode {
//};

enum pod_types {
    POD_BOOL,
    POD_INT,
    POD_UINT,
    POD_REAL,
    POD_END
};

void cnm_set_error_cb(cnm_error_cb cb) {
    g_errcb = cb;
}
bool cnm_set_global_type_buffer(struct cnm_type *buf, size_t size) {
    if (size < POD_END) return false;
    g_types = buf;
    g_maxtypes = size;
    g_ntypes = POD_END;
    g_types[POD_BOOL] = (struct cnm_type){
        .id = POD_BOOL,
        .name = "bool",
        .ops = {
            [CNM_OP_EQEQ] = NULL,
            [CNM_OP_BITOR] = NULL,
            [CNM_OP_BITAND] = NULL,
            [CNM_OP_BITXOR] = NULL,
            [CNM_OP_BITNOT] = NULL,
            [CNM_OP_ASSIGN] = NULL,
        },
        .cast = NULL,
        .ctor = NULL,
        .size = 1,
        .align = 1,
        .accs = NULL,
        .naccs = 0,
    };
    g_types[POD_INT] = (struct cnm_type){
        .id = POD_INT,
        .name = "int",
        .ops = {
            [CNM_OP_ASSIGN] = NULL,
            [CNM_OP_EQEQ] = NULL,
            [CNM_OP_GT] = NULL,
            [CNM_OP_ADD] = NULL,
            [CNM_OP_SUB] = NULL,
            [CNM_OP_MUL] = NULL,
            [CNM_OP_DIV] = NULL,
            [CNM_OP_MOD] = NULL,
            [CNM_OP_BITAND] = NULL,
            [CNM_OP_BITOR] = NULL,
            [CNM_OP_BITXOR] = NULL,
            [CNM_OP_BITNOT] = NULL,
        },
        .cast = NULL,
        .ctor = NULL,
        .size = sizeof(int),
        .align = sizeof(int),
        .accs = NULL,
        .naccs = 0,
    };
    g_types[POD_UINT] = (struct cnm_type){
        .id = POD_UINT,
        .name = "uint",
        .ops = {
            [CNM_OP_ASSIGN] = NULL,
            [CNM_OP_EQEQ] = NULL,
            [CNM_OP_GT] = NULL,
            [CNM_OP_ADD] = NULL,
            [CNM_OP_SUB] = NULL,
            [CNM_OP_MUL] = NULL,
            [CNM_OP_DIV] = NULL,
            [CNM_OP_MOD] = NULL,
            [CNM_OP_BITAND] = NULL,
            [CNM_OP_BITOR] = NULL,
            [CNM_OP_BITXOR] = NULL,
            [CNM_OP_BITNOT] = NULL,
        },
        .cast = NULL,
        .ctor = NULL,
        .size = sizeof(unsigned int),
        .align = sizeof(unsigned int),
        .accs = NULL,
        .naccs = 0,
    };
    g_types[POD_REAL] = (struct cnm_type){
        .id = POD_REAL,
        .name = "real",
        .ops = {
            [CNM_OP_ASSIGN] = NULL,
            [CNM_OP_EQEQ] = NULL,
            [CNM_OP_GT] = NULL,
            [CNM_OP_ADD] = NULL,
            [CNM_OP_SUB] = NULL,
            [CNM_OP_MUL] = NULL,
            [CNM_OP_DIV] = NULL,
            [CNM_OP_MOD] = NULL,
        },
        .cast = NULL,
        .ctor = NULL,
        .size = sizeof(double),
        .align = sizeof(double),
        .accs = NULL,
        .naccs = 0,
    };

    return true;
}



