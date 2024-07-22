#ifndef _cnm_h_
#define _cnm_h_

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

typedef int64_t cnm_int;
typedef uint64_t cnm_uint;
typedef double cnm_fp;

typedef struct cnm_cg cnm_cg;       // Code generator
typedef struct cnm_vm cnm_vm;       // Virtual machine

typedef void (*cnm_fn)(cnm_vm *c);
typedef void (*cnm_error)(int line, const char *msg);

typedef struct {
    const char *name;
    cnm_fn pfn;
} cnm_efn;

typedef enum {
    CNM_TYPE_INT,   CNM_TYPE_UINT,  CNM_TYPE_FP,    CNM_TYPE_STR,
    CNM_TYPE_REF,   CNM_TYPE_HNDL,  CNM_TYPE_PFN,   CNM_TYPE_BUF,
} cnm_type;

typedef struct cnm_val {
    cnm_type type;

    union {
        struct cnm_val *ref;
        void *hndl;
        size_t bufsz;
        cnm_int i;
        cnm_uint ui;
        cnm_fp fp;
        const char *str;
        int pfn;
    } val;
} cnm_val;

void cnm_error_default_printf(int line, const char *msg);
int cnm_fnid(const uint8_t *code, const char *name);
// Pass in NULL to use the error handler used in compilation
const cnm_val *cnm_run_ex(const uint8_t *code, int fn, cnm_error error,
                    const cnm_val *argbuf, size_t argsz);
bool cnm_compile_ex(uint8_t *buf, size_t buflen, cnm_error error,
                 const char *src, size_t nvals, size_t nvars,
                 const cnm_efn *fns, size_t nfns);

static inline bool cnm_compile(uint8_t *buf, size_t buflen, const char *src,
                               const cnm_efn *fns, size_t nfns) {
    return cnm_compile_ex(buf, buflen, cnm_error_default_printf,
                          src, 1024, 256,
                          fns, nfns);
}
static inline const cnm_val *cnm_run(const uint8_t *codebuf, const char *name,
                               const cnm_val *argbuf, size_t argsz) {
    return cnm_run_ex(codebuf, cnm_fnid(codebuf, name), NULL, argbuf, argsz);
}

typedef struct {
    const char *str;
    size_t len;
} cnm_strview;

#define CNM_SV(S) (cnm_strview){ .str = S, .len = sizeof(S) - 1 }
#define CNM_ARRSZ(A) (sizeof(A) / sizeof((A)[0]))

typedef enum {
    CNM_TOK_UNINIT, CNM_TOK_EOF,
    CNM_TOK_INT,    CNM_TOK_FP,     CNM_TOK_STR,    CNM_TOK_IDENT,

    CNM_TOK_ADD,    CNM_TOK_SUB,    CNM_TOK_DIV,    CNM_TOK_STAR,
    CNM_TOK_MOD,
    CNM_TOK_BAND,   CNM_TOK_AND,    CNM_TOK_BOR,    CNM_TOK_OR,
    CNM_TOK_NOT,    CNM_TOK_BNOT,   CNM_TOK_SHL,    CNM_TOK_SHR,
    CNM_TOK_BXOR,
    CNM_TOK_COMMA,  CNM_TOK_DOT,    CNM_TOK_SEMICOL,
    CNM_TOK_EQ,     CNM_TOK_EQEQ,   CNM_TOK_NEQ,
    CNM_TOK_LT,     CNM_TOK_LE,     CNM_TOK_GT,     CNM_TOK_GE,
    CNM_TOK_LBRA,   CNM_TOK_RBRA,   CNM_TOK_LBRK,   CNM_TOK_RBRK,
    CNM_TOK_LPAR,   CNM_TOK_RPAR,

    CNM_TOK_MAX,
} cnm_toktype;

typedef struct {
    cnm_toktype type;
    cnm_strview src;
} cnm_tok;

typedef struct {
    char name[30];
    bool defined;
    unsigned loc;
} cnm_ifn;

typedef struct {
    cnm_strview name;
    bool inited;
} cnm_scope;

typedef enum cnm_op {
    CNM_OP_HALT,CNM_OP_LN,  CNM_OP_SLN,
    CNM_OP_ADD, CNM_OP_SUB, CNM_OP_MUL, CNM_OP_DIV, CNM_OP_MOD,
    CNM_OP_NEG, CNM_OP_NOT, CNM_OP_AND, CNM_OP_OR,  CNM_OP_XOR,
    CNM_OP_SHL, CNM_OP_SHR,
    CNM_OP_CI,  CNM_OP_CUI, CNM_OP_CFP, CNM_OP_CSTR,CNM_OP_CPFN,
    CNM_OP_SEQ, CNM_OP_SNE, CNM_OP_SGT, CNM_OP_SLT, CNM_OP_SGE,
    CNM_OP_SLE, CNM_OP_BEQ, CNM_OP_BNE, CNM_OP_JMP, CNM_OP_SEQZ, CNM_OP_SNEZ,
    CNM_OP_STV, CNM_OP_LDV, CNM_OP_CPV, CNM_OP_POP,
} cnm_op;

typedef struct {
    uint16_t scopesize, valssize;
    uint16_t nifns, nefns;
    uint32_t len;
    const cnm_efn *efns;
    cnm_error default_error;       // error fn used for compiling
    uint8_t data[];
} cnm_code;

struct cnm_vm {
    struct {
        cnm_val *buf;
        cnm_val *top;
    } vals;
    
    struct {
        cnm_val **buf;
        cnm_val **top;
    } scope;

    const cnm_ifn *ifns;
    const cnm_efn *efns;
    cnm_error error;
    int line;
    const uint8_t *ip;
};

struct cnm_cg {
    struct {
        cnm_scope *buf, *start;
        cnm_scope *top;
    } scope;

    cnm_strview src;
    cnm_tok tok;
    int line;

    struct {
        cnm_ifn *buf;
        size_t len;
    } ifns;
    
    struct {
        const cnm_efn *buf;
        size_t len;
    } efns;

    cnm_error error;
    uint8_t *ip;

    cnm_code *code;   // Moveable code buffer
};

static inline bool cnm_strview_eq(const cnm_strview a, const cnm_strview b) {
    if (a.len != b.len) return false;
    return memcmp(a.str, b.str, a.len) == 0;
}

static cnm_tok cnm_toknext(cnm_cg *cg);

static void cnm_vm_init(cnm_vm *vm, cnm_val *vals, size_t valssize,
                        cnm_val **scope, size_t scopesize,
                        const cnm_code *code, cnm_error error) {
    *vm = (cnm_vm){
        .vals = {
            .buf = vals,
            .top = vals + valssize,
        },
        .scope = {
            .buf = scope,
            .top = scope + scopesize,
        },
        .ifns = (cnm_ifn *)(code->data + code->len),
        .efns = code->efns,
        .error = error,
        .line = 1,
        .ip = code->data,
    };
}

static bool cnm_cg_init(cnm_cg *cg, size_t valssize, uint8_t *code,
                        size_t codesize, cnm_scope *scope, size_t scopesize,
                        const cnm_efn *efns, size_t nefns,
                        cnm_strview src, cnm_error error) {
    if (codesize < sizeof(cnm_code) + sizeof(cnm_ifn) + sizeof(void *) + 32) {
        if (error) error(0, "code space too small");
        return false;
    }

    cnm_code *codebuf = (cnm_code *)code;
    *codebuf = (cnm_code){
        .scopesize = scopesize,
        .valssize = valssize,
        .len = codesize - sizeof(cnm_code),
        .efns = efns,
        .nefns = nefns,
        .default_error = error,
    };
    codebuf->len = codebuf->len / alignof(cnm_ifn) * alignof(cnm_ifn);

    *cg = (cnm_cg){
        .scope = {
            .buf = scope,
            .top = scope + scopesize,
            .start = scope + scopesize,
        },
        .src = src,
        .tok = {
            .type = CNM_TOK_UNINIT,
            .src.str = src.str,
        },
        .line = 1,
        .ifns = {
            .buf = (cnm_ifn *)(codebuf->data + codebuf->len),
            .len = 0,
        },
        .efns = {
            .buf = efns,
            .len = nefns,
        },
        .error = error,
        .ip = codebuf->data,
        .code = codebuf,
    };

    cnm_toknext(cg);

    return true;
}

static inline size_t cnm_valsize(const cnm_val val) {
    return val.type == CNM_TYPE_BUF ? val.val.bufsz + 1 : 1;
}

static bool cnm_vm_pop(cnm_vm *vm, cnm_val *val, size_t bufsz) {
    cnm_val *scope = *vm->scope.top++;
    const size_t size = cnm_valsize(*scope);
    if (size > bufsz) {
        if (vm->error) vm->error(vm->line, "value does not fit into buffer size\n"
                                           "could be that you passed buffer into\n"
                                           "arithmetic operation?\n");
        return false;
    }
    memcpy(val, scope, size * sizeof(cnm_val));
    vm->vals.top += size;
    return true;
}

static bool cnm_vm_push(cnm_vm *vm, cnm_val *val) {
    const size_t size = cnm_valsize(*val);
    if (vm->scope.top <= vm->scope.buf) {
        if (vm->error) vm->error(vm->line, "hit vm scope buffer limit");
        return false;
    }
    if (vm->vals.top - size < vm->vals.buf) {
        if (vm->error) vm->error(vm->line, "hit vm value buffer limit");
        return false;
    }
    vm->vals.top -= size;
    memcpy(vm->vals.top, val, size * sizeof(cnm_val));
    *--vm->scope.top = vm->vals.top;
    return true;
}

static bool cnm_vm_conv_to_commonty(cnm_val *left, cnm_val *right) {
    if (left->type == right->type) {
        return true;
    } else if (left->type == CNM_TYPE_FP || right->type == CNM_TYPE_FP) {
        switch (right->type) {
        case CNM_TYPE_INT: right->val.fp = right->val.i; break;
        case CNM_TYPE_UINT: right->val.fp = right->val.ui; break;
        default: break;
        }
        switch (left->type) {
        case CNM_TYPE_INT: left->val.fp = left->val.i; break;
        case CNM_TYPE_UINT: left->val.fp = left->val.ui; break;
        default: break;
        }
        return true;
    } else if (left->type == CNM_TYPE_UINT && right->type == CNM_TYPE_INT) {
        right->val.ui = right->val.i;
        return true;
    } else if (right->type == CNM_TYPE_UINT && left->type == CNM_TYPE_INT) {
        left->val.ui = left->val.i;
        return true;
    } else {
        return false;
    }
}

static inline cnm_int cnm_int_math_op(cnm_int l, cnm_int r, uint8_t op) {
    switch (op) {
    case CNM_OP_ADD: return l + r;
    case CNM_OP_SUB: return l - r;
    case CNM_OP_MUL: return l * r;
    case CNM_OP_DIV: return l / r;
    case CNM_OP_MOD: return l % r;
    case CNM_OP_SHR: return l >> r;
    case CNM_OP_SHL: return l << r;
    case CNM_OP_AND: return l & r;
    case CNM_OP_OR: return l | r;
    case CNM_OP_XOR: return l ^ r;
    default: return l;
    }
}
static inline cnm_uint cnm_uint_math_op(cnm_uint l, cnm_uint r, uint8_t op) {
    switch (op) {
    case CNM_OP_ADD: return l + r;
    case CNM_OP_SUB: return l - r;
    case CNM_OP_MUL: return l * r;
    case CNM_OP_DIV: return l / r;
    case CNM_OP_MOD: return l % r;
    case CNM_OP_SHR: return l >> r;
    case CNM_OP_SHL: return l << r;
    case CNM_OP_AND: return l & r;
    case CNM_OP_OR: return l | r;
    case CNM_OP_XOR: return l ^ r;
    default: return l;
    }
}
static inline cnm_fp cnm_fp_math_op(cnm_fp l, cnm_fp r, uint8_t op) {
    switch (op) {
    case CNM_OP_ADD: return l + r;
    case CNM_OP_SUB: return l - r;
    case CNM_OP_MUL: return l * r;
    case CNM_OP_DIV: return l / r;
    case CNM_OP_MOD: return fmod(l, r);
    case CNM_OP_SHR: return (cnm_fp)((cnm_int)l >> (cnm_int)r);
    case CNM_OP_SHL: return (cnm_fp)((cnm_int)l << (cnm_int)r);
    case CNM_OP_AND: return (cnm_fp)((cnm_int)l & (cnm_int)r);
    case CNM_OP_OR: return  (cnm_fp)((cnm_int)l | (cnm_int)r);
    case CNM_OP_XOR: return (cnm_fp)((cnm_int)l ^ (cnm_int)r);
    default: return l;
    }
}
static inline cnm_int cnm_compare_int(cnm_int l, cnm_int r, uint8_t op) {
    switch (op) {
    case CNM_OP_SEQ: return l == r;
    case CNM_OP_SNE: return l != r;
    case CNM_OP_SGT: return l > r;
    case CNM_OP_SLT: return l < r;
    case CNM_OP_SGE: return l >= r;
    case CNM_OP_SLE: return l <= r;
    default: return 0;
    }
}
static inline cnm_int cnm_compare_uint(cnm_uint l, cnm_uint r, uint8_t op) {
    switch (op) {
    case CNM_OP_SEQ: return l == r;
    case CNM_OP_SNE: return l != r;
    case CNM_OP_SGT: return l > r;
    case CNM_OP_SLT: return l < r;
    case CNM_OP_SGE: return l >= r;
    case CNM_OP_SLE: return l <= r;
    default: return 0;
    }
}
static inline cnm_int cnm_compare_fp(cnm_fp l, cnm_fp r, uint8_t op) {
    switch (op) {
    case CNM_OP_SEQ: return l == r;
    case CNM_OP_SNE: return l != r;
    case CNM_OP_SGT: return l > r;
    case CNM_OP_SLT: return l < r;
    case CNM_OP_SGE: return l >= r;
    case CNM_OP_SLE: return l <= r;
    default: return 0;
    }
}
static inline void cnm_take_ui(cnm_vm *vm, cnm_uint *ui) {
    *ui = 0;
    *ui |= (cnm_uint)(*vm->ip++) << 0;
    *ui |= (cnm_uint)(*vm->ip++) << 8;
    *ui |= (cnm_uint)(*vm->ip++) << 16;
    *ui |= (cnm_uint)(*vm->ip++) << 24;
    *ui |= (cnm_uint)(*vm->ip++) << 32;
    *ui |= (cnm_uint)(*vm->ip++) << 40;
    *ui |= (cnm_uint)(*vm->ip++) << 48;
    *ui |= (cnm_uint)(*vm->ip++) << 56;
}

static inline cnm_val *cnm_vm_top(const cnm_vm *const vm) {
    return *vm->scope.top;
}

static bool cnm_vm_run_instr(cnm_vm *vm, bool *running) {
    uint8_t instr = *vm->ip++;
    switch (instr) {
    case CNM_OP_SLN:
        vm->line = *vm->ip++;
        return true;
    case CNM_OP_LN:
        vm->line++;
        return true;
    case CNM_OP_CI: {
        cnm_int i;
        cnm_take_ui(vm, (cnm_uint *)&i);
        return cnm_vm_push(vm, &(cnm_val){
            .type = CNM_TYPE_INT,
            .val.i = i,
        });
    }
    case CNM_OP_CUI: {
        cnm_uint ui;
        cnm_take_ui(vm, (cnm_uint *)&ui);
        return cnm_vm_push(vm, &(cnm_val){
            .type = CNM_TYPE_UINT,
            .val.ui = ui,
        });
    }
    case CNM_OP_CFP: {
        double fp;
        cnm_take_ui(vm, (cnm_uint *)&fp);
        return cnm_vm_push(vm, &(cnm_val){
            .type = CNM_TYPE_FP,
            .val.fp = fp,
        });
    }
    case CNM_OP_POP: {
        const uint8_t n = *vm->ip++;
        for (int i = 0; i < n; i++) {
            vm->vals.top += cnm_valsize(**vm->scope.top);
            vm->scope.top++;
        }
        break;
    }
    case CNM_OP_STV:
        memcpy(vm->scope.top[*vm->ip++], *vm->scope.top, sizeof(**vm->scope.top) * cnm_valsize(**vm->scope.top));
        return true;
    case CNM_OP_LDV:
        return cnm_vm_push(vm, vm->scope.top[*vm->ip++]);
    case CNM_OP_SEQZ: case CNM_OP_SNEZ:
        instr = instr == CNM_OP_SNEZ ? CNM_OP_SNE : CNM_OP_SEQ;
        switch (cnm_vm_top(vm)->type) {
        case CNM_TYPE_INT: if (!cnm_vm_push(vm, &(cnm_val){
            .type = CNM_TYPE_INT, .val.i = 0,
        })) { return false; } break;
        case CNM_TYPE_UINT: if (!cnm_vm_push(vm, &(cnm_val){
            .type = CNM_TYPE_UINT, .val.ui = 0,
        })) { return false; } break;
        case CNM_TYPE_FP: if (!cnm_vm_push(vm, &(cnm_val){
            .type = CNM_TYPE_FP, .val.fp = 0.0,
        })) { return false; } break;
        case CNM_TYPE_STR: if (!cnm_vm_push(vm, &(cnm_val){
            .type = CNM_TYPE_STR, .val.str = NULL,
        })) { return false; } break;
        case CNM_TYPE_HNDL: if (!cnm_vm_push(vm, &(cnm_val){
            .type = CNM_TYPE_HNDL, .val.hndl = NULL,
        })) { return false; } break;
        case CNM_TYPE_PFN: if (!cnm_vm_push(vm, &(cnm_val){
            .type = CNM_TYPE_PFN, .val.pfn = 0,
        })) { return false; } break;
        default:
            if (vm->error) vm->error(vm->line, "can not convert type to boolean");
            return false;
        }
    case CNM_OP_SLT: case CNM_OP_SGE: case CNM_OP_SGT:
    case CNM_OP_SLE: case CNM_OP_SEQ: case CNM_OP_SNE: {
        cnm_val left, right;
        if (!cnm_vm_pop(vm, &right, 1)) return false;
        if (!cnm_vm_pop(vm, &left, 1)) return false;
        if (!cnm_vm_conv_to_commonty(&left, &right)) return false;

        cnm_int out;
        switch (left.type) {
        case CNM_TYPE_INT: out = cnm_compare_int(left.val.i, right.val.i, instr); break;
        case CNM_TYPE_UINT: out = cnm_compare_uint(left.val.ui, right.val.ui, instr); break;
        case CNM_TYPE_FP: out = cnm_compare_fp(left.val.fp, right.val.fp, instr); break;
        case CNM_TYPE_PFN:
            if (instr != CNM_OP_SEQ || instr != CNM_OP_SNE) {
                if (vm->error) vm->error(vm->line, "can not compare pfn");
                return false;
            }
            out = left.val.pfn == right.val.pfn;
            if (instr == CNM_OP_SNE) out = !out;
            break;
        case CNM_TYPE_HNDL:
            if (instr != CNM_OP_SEQ || instr != CNM_OP_SNE) {
                if (vm->error) vm->error(vm->line, "can not compare hndl");
                return false;
            }
            out = left.val.pfn == right.val.pfn;
            if (instr == CNM_OP_SNE) out = !out;
            break;
        case CNM_TYPE_STR:
            if (instr != CNM_OP_SEQ || instr != CNM_OP_SNE) {
                if (vm->error) vm->error(vm->line, "can not compare str");
                return false;
            }
            out = 0;
            if (!left.val.str && !right.val.str) {
                out = 1;
            } else if (left.val.str && right.val.str) {
                out = strcmp(left.val.str, right.val.str) == 0;
            }
            if (instr == CNM_OP_SNE) out = !out;
            break;
        default:
            if (vm->error) vm->error(vm->line, "can not convert type to boolean");
            return false;
        }
        return cnm_vm_push(vm, &(cnm_val){
            .type = CNM_TYPE_INT,
            .val.i = out,
        });
    }
    case CNM_OP_NEG:
        switch (cnm_vm_top(vm)->type) {
        case CNM_TYPE_INT: cnm_vm_top(vm)->val.i = -cnm_vm_top(vm)->val.i; break;
        case CNM_TYPE_UINT: cnm_vm_top(vm)->val.ui = -cnm_vm_top(vm)->val.ui; break;
        case CNM_TYPE_FP: cnm_vm_top(vm)->val.fp = -cnm_vm_top(vm)->val.fp; break;
        default: return false;
        }
        return true;
    case CNM_OP_NOT:
        switch (cnm_vm_top(vm)->type) {
        case CNM_TYPE_INT: cnm_vm_top(vm)->val.i = ~cnm_vm_top(vm)->val.i; break;
        case CNM_TYPE_UINT: cnm_vm_top(vm)->val.ui = ~cnm_vm_top(vm)->val.ui; break;
        case CNM_TYPE_FP:
            cnm_vm_top(vm)->type = CNM_TYPE_INT;
            cnm_vm_top(vm)->val.i = ~(cnm_int)cnm_vm_top(vm)->val.fp;
            break;
        default: return false;
        }
        return true;
    case CNM_OP_ADD: case CNM_OP_SUB: case CNM_OP_MUL: case CNM_OP_DIV:
    case CNM_OP_MOD: case CNM_OP_SHL: case CNM_OP_SHR: case CNM_OP_AND:
    case CNM_OP_OR:  case CNM_OP_XOR: {
        cnm_val left, right;
        if (!cnm_vm_pop(vm, &right, 1)) return false;
        if (!cnm_vm_pop(vm, &left, 1)) return false;
        if (!cnm_vm_conv_to_commonty(&left, &right)) return false;
        switch (left.type) {
        case CNM_TYPE_FP:
            return cnm_vm_push(vm, &(cnm_val){
                .type = CNM_TYPE_FP,
                .val.fp = cnm_fp_math_op(left.val.fp, right.val.fp, instr),
            });
        case CNM_TYPE_UINT:
            return cnm_vm_push(vm, &(cnm_val){
                .type = CNM_TYPE_UINT,
                .val.ui = cnm_uint_math_op(left.val.ui, right.val.ui, instr),
            });
        case CNM_TYPE_INT:
            return cnm_vm_push(vm, &(cnm_val){
                .type = CNM_TYPE_INT,
                .val.i = cnm_int_math_op(left.val.i, right.val.i, instr),
            });
        default: return false;
        }
    }
    case CNM_OP_HALT:
        *running = false;
        return true;
    default:
        if (vm->error) vm->error(vm->line, "invalid vm instruction");
        return false;
    }
}

#define CNM_PREC_FULL 1
static cnm_scope *cnm_eval_expr(cnm_cg *cg, int prec);

static bool cnm_eval_stmtgrp(cnm_cg *cg);

void cnm_error_default_printf(int line, const char *msg) {
    printf("cnm error [%d]: %s\n", line, msg);
}

int cnm_fnid(const uint8_t *codebuf, const char *name) {
    const cnm_code *code = (const cnm_code *)codebuf;
    const cnm_ifn *ifns = (const cnm_ifn *)(code->data + code->len);

    for (int i = 0; i < code->nifns; i++) {
        if (strcmp(ifns[-i].name, name) == 0) return -i - 1;
    }
    for (int i = 0; i < code->nefns; i++) {
        if (strcmp(code->efns[i].name, name) == 0) return i + 1;
    }

    return 0;
}

static thread_local cnm_val cnm_run_ex_retbuf[32];

// Pass in NULL to use the error handler used in compilation
const cnm_val *cnm_run_ex(const uint8_t *codebuf, int fn, cnm_error error,
                          const cnm_val *args, size_t argsz) {
    const cnm_code *code = (const cnm_code *)codebuf;

    cnm_vm vm;
    cnm_val *vals = alloca(sizeof(*vals) * code->valssize);
    cnm_val **vars = alloca(sizeof(*vars) * code->scopesize);

    if (!error) error = code->default_error;

    cnm_vm_init(&vm, vals, code->valssize, vars, code->scopesize,
                code, error);

    for (int i = 0, j = 0; i < argsz; i++) {
        const size_t size = cnm_valsize(args[j]);
        if (vm.scope.top <= vm.scope.buf) {
            if (error) error(0, "ran out of scope space");
            return NULL;
        }
        vm.vals.top -= size;
        if (vm.vals.top - size < vm.vals.buf) {
            if (error) error(0, "ran out of stack space");
            return NULL;
        }
        memcpy(vm.vals.top, args + j, size * sizeof(cnm_val));
        *--vm.scope.top = vm.vals.top;
        j += size;
    }

    //if (fn >= 0 || fn < code->nifns) {
    //    if (error) error(0, "vm started with invalid function id");
    //    return false;
    //}

    bool running = true;
    while (running) if (!cnm_vm_run_instr(&vm, &running)) return NULL;

    if (cnm_valsize(**vm.scope.top) > CNM_ARRSZ(cnm_run_ex_retbuf)) {
        if (error) error(0, "return value can not fit in return buffer");
        return NULL;
    }
    cnm_run_ex_retbuf[0] = (cnm_val){
        .type = CNM_TYPE_INT,
        .val.i = 0,
    };
    memcpy(cnm_run_ex_retbuf, *vm.scope.top,
           sizeof(**vm.scope.top) * cnm_valsize(**vm.scope.top));

    return cnm_run_ex_retbuf;
}
bool cnm_compile_ex(uint8_t *codebuf, size_t buflen, cnm_error error,
                 const char *src, size_t nvals, size_t nvars,
                 const cnm_efn *fns, size_t nfns) {
    cnm_cg cg;
    cnm_scope *scope = alloca(nvars * sizeof(*scope));
    cnm_cg_init(&cg, nvals, codebuf, buflen,
                scope, nvars, fns, nfns,
                (cnm_strview){ .str = src, .len = strlen(src) },
                error);
    //if (!cnm_eval_expr(&cg, CNM_PREC_FULL)) return false;
    if (!cnm_eval_stmtgrp(&cg)) return false;
    cg.code->nifns = cg.ifns.len;

    return true;
}

static bool cnm_cg_emit(cnm_cg *cg, uint8_t byte) {
    if (cg->ip >= (uint8_t *)(cg->ifns.buf - cg->ifns.len)) {
        if (cg->error) cg->error(cg->line, "ran out of code space");
        return false;
    }

    *cg->ip++ = byte;
    return true;
}

static const char *cnm_eatspaces(cnm_cg *cg, const char *str) {
    while (isspace(*str)) {
        if (*str == '\n') {
            cg->line++;
            if (!cnm_cg_emit(cg, CNM_OP_LN)) return NULL;
        }
        ++str;
        if (str >= cg->src.str + cg->src.len) return NULL;
    }
    return str;
}

static const char *cnm_whitespace(cnm_cg *cg, const char *str) {
    if (!(str = cnm_eatspaces(cg, str))) return NULL;
    while (str[0] == '/' && str[1] == '/') {
        while (*str != '\n') {
            ++str;
            if (str >= cg->src.str + cg->src.len) return NULL;
        }
        cg->line++;
        if (!cnm_cg_emit(cg, CNM_OP_LN)) return NULL;
        if (!(str = cnm_eatspaces(cg, str))) return NULL;
    }

    return str;
}

static cnm_tok cnm_toknext(cnm_cg *cg) {
    const char *const end = cg->src.str + cg->src.len;
    if (cg->tok.type == CNM_TOK_EOF) goto ret_eof;

    cg->tok.src.str += cg->tok.src.len + (cg->tok.type == CNM_TOK_STR);
    cg->tok.src.len = 1;
    const char *ptr = cg->tok.src.str = cnm_whitespace(cg, cg->tok.src.str);

    if (!ptr || ptr >= end) goto ret_eof;

    switch (tolower(*ptr++)) {
    case '\0': cg->tok.type = CNM_TOK_EOF; break;
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9': {
        bool isfp = false;
        while (isdigit(*ptr) || *ptr == '.') {
            if (*ptr == '.') isfp = true;
            ++ptr;
            ++cg->tok.src.len;

            if (ptr >= end) break;
        }
        cg->tok.type = isfp ? CNM_TOK_FP : CNM_TOK_INT;
        break;
    }
    case '"':
        while (*ptr != '"') {
            if (*ptr == '\\') {
                ++ptr;
                ++cg->tok.src.len;
            }
            ++ptr;
            ++cg->tok.src.len;

            if (ptr >= end) goto ret_eof;
        }
        cg->tok.type = CNM_TOK_STR;
        break;
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i':
    case 'j': case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r':
    case 's': case 't': case 'u': case 'v': case 'w': case 'y': case 'x': case 'z': case '_':
        while (isalnum(*ptr) || *ptr == '_') {
            ++ptr;
            ++cg->tok.src.len;
            if (ptr >= end) break;
        }
        cg->tok.type = CNM_TOK_IDENT;
        break;
    default: goto ret_eof;

    case '<':
        cg->tok.type = CNM_TOK_LT;
        if (*ptr == '<' && ptr < end) {
            cg->tok.type = CNM_TOK_SHL;
            cg->tok.src.len = 2;
        } else if (*ptr == '=' && ptr < end) {
            cg->tok.type = CNM_TOK_LE;
            cg->tok.src.len = 2;
        }
        break;
    case '>':
        cg->tok.type = CNM_TOK_GT;
        if (*ptr == '>' && ptr < end) {
            cg->tok.type = CNM_TOK_SHR;
            cg->tok.src.len = 2;
        } else if (*ptr == '=' && ptr < end) {
            cg->tok.type = CNM_TOK_GE;
            cg->tok.src.len = 2;
        }
        break;
    case '&':
        cg->tok.type = CNM_TOK_BAND;
        if (*ptr == '&' && ptr < end) {
            cg->tok.type = CNM_TOK_AND;
            cg->tok.src.len = 2;
        }
        break;
    case '|':
        cg->tok.type = CNM_TOK_BOR;
        if (*ptr == '|' && ptr < end) {
            cg->tok.type = CNM_TOK_OR;
            cg->tok.src.len = 2;
        }
        break;
    case '=':
        cg->tok.type = CNM_TOK_EQ;
        if (*ptr == '=' && ptr < end) {
            cg->tok.type = CNM_TOK_EQEQ;
            cg->tok.src.len = 2;
        }
        break;
    case '!':
        cg->tok.type = CNM_TOK_NOT;
        if (*ptr == '=' && ptr < end) {
            cg->tok.type = CNM_TOK_NEQ;
            cg->tok.src.len = 2;
        }
        break;
    case '+': cg->tok.type = CNM_TOK_ADD; break;
    case '-': cg->tok.type = CNM_TOK_SUB; break;
    case '/': cg->tok.type = CNM_TOK_DIV; break;
    case '%': cg->tok.type = CNM_TOK_MOD; break;
    case '*': cg->tok.type = CNM_TOK_STAR; break;
    case '^': cg->tok.type = CNM_TOK_BXOR; break;
    case '~': cg->tok.type = CNM_TOK_BNOT; break;
    case ',': cg->tok.type = CNM_TOK_COMMA; break;
    case '.': cg->tok.type = CNM_TOK_DOT; break;
    case ';': cg->tok.type = CNM_TOK_SEMICOL; break;
    case '{': cg->tok.type = CNM_TOK_LBRA; break;
    case '}': cg->tok.type = CNM_TOK_RBRA; break;
    case '[': cg->tok.type = CNM_TOK_LBRK; break;
    case ']': cg->tok.type = CNM_TOK_RBRK; break;
    case '(': cg->tok.type = CNM_TOK_LPAR; break;
    case ')': cg->tok.type = CNM_TOK_RPAR; break;
    }

    return cg->tok;

ret_eof:
    cg->tok.type = CNM_TOK_EOF;
    return cg->tok;
}

static cnm_scope *cnm_cg_allocscope(cnm_cg *cg) {
    if (cg->scope.top <= cg->scope.buf) {
        if (cg->error) cg->error(cg->line, "ran out of scope entries");
        return NULL;
    }

    return --cg->scope.top;
}
static inline int cnm_scopeoffs(cnm_cg *cg, cnm_scope *scope) {
    int offs = 0;
    for (cnm_scope *i = cg->scope.top; i != scope; i++) offs += i->inited;
    return offs;
}
static inline int cnm_scopeloc(cnm_cg *cg, cnm_scope *scope) {
    return cnm_scopeoffs(cg, scope) + (cg->scope.top - cg->scope.buf);
}

static bool cnm_emit_ui(cnm_cg *cg, cnm_uint ui) {
    if (!cnm_cg_emit(cg, (ui >> 0) & 0xff)) return false;
    if (!cnm_cg_emit(cg, (ui >> 8) & 0xff)) return false;
    if (!cnm_cg_emit(cg, (ui >> 16) & 0xff)) return false;
    if (!cnm_cg_emit(cg, (ui >> 24) & 0xff)) return false;
    if (!cnm_cg_emit(cg, (ui >> 32) & 0xff)) return false;
    if (!cnm_cg_emit(cg, (ui >> 40) & 0xff)) return false;
    if (!cnm_cg_emit(cg, (ui >> 48) & 0xff)) return false;
    if (!cnm_cg_emit(cg, (ui >> 56) & 0xff)) return false;
    return true;
}

static cnm_scope *_cnm_eval_int(cnm_cg *cg, int prec, bool neg) {
    cnm_scope *scope = cnm_cg_allocscope(cg);
    if (!scope) return NULL;
    scope->name = (cnm_strview){0};
    scope->inited = true;

    errno = 0;
    cnm_uint ui = strtoull(cg->tok.src.str, NULL, 0);
    if (!cnm_cg_emit(cg, ui <= (uint64_t)INT64_MAX+1 || neg ? CNM_OP_CI : CNM_OP_CUI)) {
        return NULL;
    }
    if (neg) {
        const cnm_int i = -(cnm_int)ui;
        ui = *(cnm_uint *)&i;
    }
    if (!cnm_emit_ui(cg, ui)) return NULL;

    cnm_toknext(cg);

    return scope;
}
static cnm_scope *_cnm_eval_fp(cnm_cg *cg, int prec, bool neg) {
    cnm_scope *scope = cnm_cg_allocscope(cg);
    if (!scope) return NULL;
    scope->name = (cnm_strview){0};
    scope->inited = true;

    errno = 0;
    double fp = strtod(cg->tok.src.str, NULL);
    if (neg) fp = -fp;
    if (!cnm_cg_emit(cg, CNM_OP_CFP)) return NULL;
    if (!cnm_emit_ui(cg, *(cnm_uint *)&fp)) return NULL;

    cnm_toknext(cg);

    return scope;
}

static cnm_scope *cnm_eval_int(cnm_cg *const cg, int prec) {
    return _cnm_eval_int(cg, prec, false);
}

static cnm_scope *cnm_eval_fp(cnm_cg *const cg, int prec) {
    return _cnm_eval_fp(cg, prec, false);
}

static cnm_scope *cnm_eval_pre(cnm_cg *const cg, int prec) {
    int op;
    switch (cg->tok.type) {
    case CNM_TOK_NOT:
        op = CNM_OP_SEQZ;
        cnm_toknext(cg);
        break;
    case CNM_TOK_BNOT:
        op = CNM_OP_NOT;
        cnm_toknext(cg);
        break;
    case CNM_TOK_SUB:
        op = CNM_OP_NEG;
        cnm_toknext(cg);
        if (cg->tok.type == CNM_TOK_INT) return _cnm_eval_int(cg, 15, true);
        else if (cg->tok.type == CNM_TOK_FP) return _cnm_eval_fp(cg, 15, true);
        break;
    default: return NULL;
    }
    cnm_scope *right = cnm_eval_expr(cg, prec);
    if (!cnm_cg_emit(cg, op)) return NULL;
    return right;
}

static cnm_scope *cnm_eval_grp(cnm_cg *const cg, int prec) {
    cnm_toknext(cg);
    cnm_scope *scope = cnm_eval_expr(cg, CNM_PREC_FULL);
    if (cg->tok.type != CNM_TOK_RPAR) {
        if (cg->error) cg->error(cg->line, "expected parenthesis");
        return NULL;
    }
    cnm_toknext(cg);
    return scope;
}

static cnm_scope *cnm_eval_assign(cnm_cg *const cg, cnm_scope *left, int prec) {
    cnm_toknext(cg);
    cnm_scope *right = cnm_eval_expr(cg, CNM_PREC_FULL);
    if (!right) return NULL;
    if (left->inited) {
        if (!cnm_cg_emit(cg, CNM_OP_STV)) return NULL;
        if (!cnm_cg_emit(cg, cnm_scopeoffs(cg, left))) return NULL;
    } else {
        cg->scope.top++;
        left->inited = true;
    }
    return left;
}

static cnm_scope *cnm_eval_ident(cnm_cg *const cg, int prec) {
    const cnm_strview name = cg->tok.src;
    cnm_toknext(cg);

    for (cnm_scope *i = cg->scope.top; i != cg->scope.start; i++) {
        if (!cnm_strview_eq(name, i->name) || !i->inited) continue;
        return i;
    }

    if (cg->tok.type != CNM_TOK_EQ) {
        if (cg->error) cg->error(cg->line, "identifier not in scope");
        return NULL;
    }

    cnm_scope *scope = cnm_cg_allocscope(cg);
    if (!scope) return NULL;
    scope->name = name;
    scope->inited = false;
    return scope;
}

static bool cnm_getval(cnm_cg *cg, cnm_scope *val) {
    if (!val->name.str && cnm_scopeoffs(cg, val) == 0) return true;
    if (!cnm_cg_emit(cg, CNM_OP_LDV)) return false;
    if (!cnm_cg_emit(cg, cnm_scopeoffs(cg, val))) return false;
    *--cg->scope.top = *val;
    return true;
}

static cnm_scope *cnm_eval_math(cnm_cg *const cg, cnm_scope *left, int prec) {
    int op;
    switch (cg->tok.type) {
    case CNM_TOK_ADD: op = CNM_OP_ADD; break;
    case CNM_TOK_SUB: op = CNM_OP_SUB; break;
    case CNM_TOK_STAR: op = CNM_OP_MUL; break;
    case CNM_TOK_DIV: op = CNM_OP_DIV; break;
    case CNM_TOK_MOD: op = CNM_OP_MOD; break;
    case CNM_TOK_SHR: op = CNM_OP_SHR; break;
    case CNM_TOK_SHL: op = CNM_OP_SHL; break;
    case CNM_TOK_BAND: op = CNM_OP_AND; break;
    case CNM_TOK_BOR: op = CNM_OP_OR; break;
    case CNM_TOK_BXOR: op = CNM_OP_XOR; break;
    case CNM_TOK_EQEQ: op = CNM_OP_SEQ; break;
    case CNM_TOK_NEQ: op = CNM_OP_SNE; break;
    case CNM_TOK_GT: op = CNM_OP_SGT; break;
    case CNM_TOK_LT: op = CNM_OP_SLT; break;
    case CNM_TOK_GE: op = CNM_OP_SGE; break;
    case CNM_TOK_LE: op = CNM_OP_SLE; break;
    default: return NULL;
    }
    cnm_toknext(cg);

    if (!cnm_getval(cg, left)) return NULL;
    cnm_scope *right = cnm_eval_expr(cg, prec + 1);
    if (!right) return NULL;
    if (!cnm_getval(cg, right)) return NULL;

    if (cg->scope.start - cg->scope.top < 2) {
        if (cg->error) cg->error(cg->line, "expected 2 parameters on the stack first");
        return NULL;
    }
    cg->scope.top += 2;

    int scopeid;
    cnm_scope *scope = cnm_cg_allocscope(cg);
    scope->name = (cnm_strview){0};
    scope->inited = true;

    if (!cnm_cg_emit(cg, op)) return scope;

    return scope;
}

typedef cnm_scope *(*cnm_eval_expr_pre)(cnm_cg *cg, int prec);
typedef cnm_scope *(*cnm_eval_expr_in)(cnm_cg *cg, cnm_scope *scope, int prec);

typedef struct {
    int prec_pre;
    int prec_in;

    cnm_eval_expr_pre pre;
    cnm_eval_expr_in in;
} cnm_expr_rule;

static const cnm_expr_rule cnm_expr_rules[CNM_TOK_MAX] = {
    [CNM_TOK_LPAR] = { .prec_pre = 16, .pre = cnm_eval_grp },
    [CNM_TOK_INT] = { .prec_pre = 16, .pre = cnm_eval_int },
    [CNM_TOK_FP] = { .prec_pre = 16, .pre = cnm_eval_fp },
    [CNM_TOK_ADD] = { .prec_in = 12, .in = cnm_eval_math },
    [CNM_TOK_SUB] = { .prec_in = 12, .in = cnm_eval_math, .prec_pre = 14, .pre = cnm_eval_pre },
    [CNM_TOK_STAR] = { .prec_in = 13, .in = cnm_eval_math },
    [CNM_TOK_DIV] = { .prec_in = 13, .in = cnm_eval_math },
    [CNM_TOK_MOD] = { .prec_in = 13, .in = cnm_eval_math },
    [CNM_TOK_SHL] = { .prec_in = 11, .in = cnm_eval_math },
    [CNM_TOK_SHR] = { .prec_in = 11, .in = cnm_eval_math },
    [CNM_TOK_BAND] = { .prec_in = 8, .in = cnm_eval_math },
    [CNM_TOK_BXOR] = { .prec_in = 7, .in = cnm_eval_math },
    [CNM_TOK_BOR] = { .prec_in = 6, .in = cnm_eval_math },
    [CNM_TOK_NOT] = { .prec_pre = 14, .pre = cnm_eval_pre },
    [CNM_TOK_BNOT] = { .prec_pre = 14, .pre = cnm_eval_pre },
    [CNM_TOK_EQ] = { .prec_in = 2, .in = cnm_eval_assign },
    [CNM_TOK_EQEQ] = { .prec_in = 9, .in = cnm_eval_math },
    [CNM_TOK_NEQ] = { .prec_in = 9, .in = cnm_eval_math },
    [CNM_TOK_GT] = { .prec_in = 10, .in = cnm_eval_math },
    [CNM_TOK_LT] = { .prec_in = 10, .in = cnm_eval_math },
    [CNM_TOK_GE] = { .prec_in = 10, .in = cnm_eval_math },
    [CNM_TOK_LE] = { .prec_in = 10, .in = cnm_eval_math },
    [CNM_TOK_IDENT] = { .prec_pre = 16, .pre = cnm_eval_ident },
};

static cnm_scope *cnm_eval_expr(cnm_cg *cg, const int prec) {
    const cnm_expr_rule *rule = cnm_expr_rules + cg->tok.type;
    if (!rule->pre || prec > rule->prec_pre) {
        if (cg->error) cg->error(cg->line, "expected expression");
        return NULL;
    }
    cnm_scope *scope = rule->pre(cg, rule->prec_pre);
    if (!scope) return NULL;

    rule = cnm_expr_rules + cg->tok.type;
    while (rule->in && prec <= rule->prec_in) {
        scope = rule->in(cg, scope, rule->prec_pre);
        if (!scope) return NULL;
        rule = cnm_expr_rules + cg->tok.type;
    }

    return scope;
}

static bool cnm_eval_stmtexpr(cnm_cg *cg) {
    const cnm_scope *oldtop = cg->scope.top;
    const int start = cnm_scopeloc(cg, cg->scope.top);
    if (!cnm_eval_expr(cg, CNM_PREC_FULL)) return false;
    const int end = cnm_scopeloc(cg, cg->scope.top);
    int npop = 0;
    for (cnm_scope *i = cg->scope.top; !i->name.str && i != oldtop; i++, npop++);
    if (npop) {
        if (!cnm_cg_emit(cg, CNM_OP_POP)) return false;
        if (!cnm_cg_emit(cg, npop)) return false;
        cg->scope.top += npop;
    }
    if (cg->tok.type != CNM_TOK_SEMICOL) {
        if (cg->error) cg->error(cg->line, "expected semicolon after statement");
        return -1;
    }
    cnm_toknext(cg);
    return true;
}

static bool cnm_eval_stmtgrp(cnm_cg *cg) {
    if (cg->tok.type != CNM_TOK_LBRA) {
        if (cg->error) cg->error(cg->line, "expected left brace for statement group");
        return false;
    }
    cnm_toknext(cg);

    while (cg->tok.type != CNM_TOK_RBRA) {
        if (!cnm_eval_stmtexpr(cg)) return false;
    }
    cnm_toknext(cg);
    return true;
}

#endif

