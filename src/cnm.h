#ifndef _cnm_h_
#define _cnm_h_

#include <ctype.h>
#include <errno.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
bool cnm_run_ex(const uint8_t *code, int fn, cnm_error error, cnm_val **vals, size_t nvals);
bool cnm_compile_ex(uint8_t *buf, size_t buflen, cnm_error error,
                 const char *src, size_t nvals, size_t nvars,
                 const cnm_efn *fns, size_t nfns);

static inline bool cnm_compile(uint8_t *buf, size_t buflen, const char *src,
                               const cnm_efn *fns, size_t nfns) {
    return cnm_compile_ex(buf, buflen, cnm_error_default_printf,
                          src, 1024, 256,
                          fns, nfns);
}
static inline bool cnm_run(const uint8_t *codebuf, const char *name,
                           cnm_val **vals, size_t nvals) {
    return cnm_run_ex(codebuf, cnm_fnid(codebuf, name), NULL, vals, nvals);
}

typedef struct {
    const char *str;
    size_t len;
} cnm_strview;

#define SV(S) (cnm_strview){ .str = S, .len = sizeof(S) - 1 }

typedef enum {
    CNM_TOK_UNINIT, CNM_TOK_EOF,
    CNM_TOK_INT,    CNM_TOK_FP,     CNM_TOK_STR,    CNM_TOK_IDENT,

    CNM_TOK_ADD,    CNM_TOK_SUB,    CNM_TOK_DIV,    CNM_TOK_STAR,
    CNM_TOK_BAND,   CNM_TOK_AND,    CNM_TOK_BOR,    CNM_TOK_OR,
    CNM_TOK_NOT,    CNM_TOK_BNOT,   CNM_TOK_SHL,    CNM_TOK_SHR,
    CNM_TOK_BXOR,
    CNM_TOK_COMMA,  CNM_TOK_DOT,
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

typedef enum {
    CNM_OP_HALT,
    CNM_OP_LINE,
    CNM_OP_ADD,
    CNM_OP_SUB,
    CNM_OP_MUL,
    CNM_OP_DIV,
    CNM_OP_MOD,
    CNM_OP_NEG,
    CNM_OP_NOT,
    CNM_OP_AND,
    CNM_OP_OR,
    CNM_OP_XOR,
    CNM_OP_SHL,
    CNM_OP_SHR,
    CNM_OP_CI,
    CNM_OP_CUI,
    CNM_OP_CFP,
    CNM_OP_CSTR,
    CNM_OP_CPFN,
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
        size_t len, size;
    } vals;
    
    struct {
        cnm_val **buf;
        size_t len, size;
    } scope;

    const cnm_ifn *ifns;
    const cnm_efn *efns;
    cnm_error error;
    int line;
    const uint8_t *ip;
};

struct cnm_cg {
    struct {
        cnm_scope *buf;
        size_t len, size;
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
            .size = valssize,
        },
        .scope = {
            .buf = scope,
            .size = scopesize,
        },
        .ifns = (cnm_ifn *)(code->data + code->len),
        .efns = code->efns,
        .error = error,
        .line = 0,
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
            .size = scopesize,
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

static bool cnm_vm_pop_arith(cnm_vm *vm, cnm_val *val) {
    if (vm->scope.len == 0) {
        if (vm->error) vm->error(vm->line, "vm stack buffer underflow");
        return false;
    }
    cnm_val **scope = &vm->scope.buf[vm->scope.len - 1];
    switch ((*scope)->type) {
    case CNM_TYPE_INT: case CNM_TYPE_UINT: case CNM_TYPE_FP: break;
    default:
        if (vm->error) vm->error(vm->line, "can only do math on arithmetic types");
        return false;
    }

    vm->scope.len--;
    vm->vals.len--;

    *val = **scope;
    return true;
}

static bool cnm_vm_push(cnm_vm *vm, cnm_val val) {
    if (vm->scope.len >= vm->scope.size) {
        if (vm->error) vm->error(vm->line, "hit vm scope buffer limit");
        return false;
    }
    if (vm->vals.len >= vm->vals.size) {
        if (vm->error) vm->error(vm->line, "hit vm value buffer limit");
        return false;
    }
    vm->scope.buf[vm->scope.len++] = vm->vals.buf + vm->vals.len;
    vm->vals.buf[vm->vals.len++] = val;
    return true;
}

static cnm_type cnm_vm_conv_to_commonty(cnm_val *left, cnm_val *right) {
    if (left->type == right->type) {
        return left->type;
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
        return CNM_TYPE_FP;
    } else if (left->type == CNM_TYPE_UINT && right->type == CNM_TYPE_INT) {
        right->val.ui = right->val.i;
        return CNM_TYPE_UINT;
    } else if (right->type == CNM_TYPE_UINT && left->type == CNM_TYPE_INT) {
        left->val.ui = left->val.i;
        return CNM_TYPE_UINT;
    } else {
        return CNM_TYPE_INT;
    }
}

static bool cnm_vm_run_instr(cnm_vm *vm, bool *running) {
    switch (*vm->ip++) {
    case CNM_OP_CI: {
        cnm_int i = 0;
        i |= (cnm_int)(*vm->ip++) << 0;
        i |= (cnm_int)(*vm->ip++) << 8;
        i |= (cnm_int)(*vm->ip++) << 16;
        i |= (cnm_int)(*vm->ip++) << 24;
        i |= (cnm_int)(*vm->ip++) << 32;
        i |= (cnm_int)(*vm->ip++) << 40;
        i |= (cnm_int)(*vm->ip++) << 48;
        i |= (cnm_int)(*vm->ip++) << 56;
        return cnm_vm_push(vm, (cnm_val){
            .type = CNM_TYPE_INT,
            .val.i = i,
        });
    }
    case CNM_OP_ADD: {
        cnm_val left, right;
        if (!cnm_vm_pop_arith(vm, &right)) return false;
        if (!cnm_vm_pop_arith(vm, &left)) return false;
        switch (cnm_vm_conv_to_commonty(&left, &right)) {
        case CNM_TYPE_FP:
            return cnm_vm_push(vm, (cnm_val){
                .type = CNM_TYPE_FP,
                .val.fp = left.val.fp + right.val.fp,
            });
        case CNM_TYPE_UINT:
            return cnm_vm_push(vm, (cnm_val){
                .type = CNM_TYPE_UINT,
                .val.ui = left.val.ui + right.val.ui,
            });
        case CNM_TYPE_INT:
            return cnm_vm_push(vm, (cnm_val){
                .type = CNM_TYPE_INT,
                .val.i = left.val.i + right.val.i,
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
static int cnm_eval_expr(cnm_cg *cg, int prec);

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
// Pass in NULL to use the error handler used in compilation
bool cnm_run_ex(const uint8_t *codebuf, int fn, cnm_error error,
                cnm_val **args, size_t nargs) {
    const cnm_code *code = (const cnm_code *)codebuf;

    cnm_vm vm;
    cnm_val *vals = alloca(sizeof(*vals) * code->valssize);
    cnm_val **vars = alloca(sizeof(*vars) * code->scopesize);

    if (!error) error = code->default_error;

    if (nargs > code->scopesize) {
        if (error) error(0, "too many arguments passed to vm");
        return false;
    }

    cnm_vm_init(&vm, vals, code->valssize, vars, code->scopesize,
                code, error);

    for (int i = 0; i < nargs; i++) {
        const size_t valsz = cnm_valsize(*args[i]);
        if (vm.vals.len + valsz > vm.vals.size) {
            if (error) error(0, "arguments passed to vm overflowed");
            return false;
        }
        vm.scope.buf[i] = vm.vals.buf + vm.vals.len;
        memcpy(vm.scope.buf[i], args[i], valsz * sizeof(cnm_val));
        vm.vals.len += valsz;
    }

    //if (fn >= 0 || fn < code->nifns) {
    //    if (error) error(0, "vm started with invalid function id");
    //    return false;
    //}

    bool running = true;
    while (running) if (!cnm_vm_run_instr(&vm, &running)) return false;

    return true;
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
    if (cnm_eval_expr(&cg, CNM_PREC_FULL) == -1) return false;
    cg.code->nifns = cg.ifns.len;

    return true;
}

static const char *cnm_eatspaces(cnm_cg *cg, const char *str) {
    while (isspace(*str)) {
        cg->line += *str == '\n';
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
    case '*': cg->tok.type = CNM_TOK_STAR; break;
    case '^': cg->tok.type = CNM_TOK_BXOR; break;
    case '~': cg->tok.type = CNM_TOK_BNOT; break;
    case ',': cg->tok.type = CNM_TOK_COMMA; break;
    case '.': cg->tok.type = CNM_TOK_DOT; break;
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

static cnm_scope *cnm_cg_allocscope(cnm_cg *cg, int *scope) {
    if (cg->scope.len >= cg->scope.size) {
        if (cg->error) cg->error(cg->line, "ran out of scope entries");
        return NULL;
    }

    if (*scope) *scope = cg->scope.len;
    return cg->scope.buf + cg->scope.len++;
}

static bool cnm_cg_emit(cnm_cg *cg, uint8_t byte) {
    if (cg->ip >= (uint8_t *)(cg->ifns.buf - cg->ifns.len)) {
        if (cg->error) cg->error(cg->line, "ran out of code space");
        return false;
    }

    *cg->ip++ = byte;
    return true;
}

static int cnm_eval_int(cnm_cg *const cg, int prec) {
    int scopeid;
    cnm_scope *scope = cnm_cg_allocscope(cg, &scopeid);
    scope->name = (cnm_strview){0};
    scope->inited = true;

    errno = 0;
    cnm_uint ui = strtoull(cg->tok.src.str, NULL, 0);
    if (!cnm_cg_emit(cg, ui <= (uint64_t)INT64_MAX+1 ? CNM_OP_CI : CNM_OP_CUI)) return -1;
    if (!cnm_cg_emit(cg, (ui >> 0) & 0xff)) return -1;
    if (!cnm_cg_emit(cg, (ui >> 8) & 0xff)) return -1;
    if (!cnm_cg_emit(cg, (ui >> 16) & 0xff)) return -1;
    if (!cnm_cg_emit(cg, (ui >> 24) & 0xff)) return -1;
    if (!cnm_cg_emit(cg, (ui >> 32) & 0xff)) return -1;
    if (!cnm_cg_emit(cg, (ui >> 40) & 0xff)) return -1;
    if (!cnm_cg_emit(cg, (ui >> 48) & 0xff)) return -1;
    if (!cnm_cg_emit(cg, (ui >> 56) & 0xff)) return -1;

    cnm_toknext(cg);

    return scopeid;
}

static int cnm_eval_add(cnm_cg *const cg, int left, int prec) {
    cnm_toknext(cg);

    int right = cnm_eval_expr(cg, prec + 1);
    if (right == -1) return -1;

    if (cg->scope.len < 2) {
        if (cg->error) cg->error(cg->line, "expected 2 parameters on the stack first");
        return -1;
    }
    cg->scope.len -= 2;

    int scopeid;
    cnm_scope *scope = cnm_cg_allocscope(cg, &scopeid);
    scope->name = (cnm_strview){0};
    scope->inited = true;

    cnm_cg_emit(cg, CNM_OP_ADD);

    return scopeid;
}

typedef int(*cnm_eval_expr_pre)(cnm_cg *cg, int prec);
typedef int(*cnm_eval_expr_in)(cnm_cg *cg, int scope, int prec);

typedef struct {
    int prec_pre;
    int prec_in;

    cnm_eval_expr_pre pre;
    cnm_eval_expr_in in;
} cnm_expr_rule;

static const cnm_expr_rule cnm_expr_rules[CNM_TOK_MAX] = {
    [CNM_TOK_INT] = { .prec_pre = 16, .pre = cnm_eval_int  },
    [CNM_TOK_ADD] = { .prec_in = 12, .in = cnm_eval_add },
};

static int cnm_eval_expr(cnm_cg *cg, const int prec) {
    const cnm_expr_rule *rule = cnm_expr_rules + cg->tok.type;
    if (!rule->pre || prec > rule->prec_pre) {
        if (cg->error) cg->error(cg->line, "expected expression");
        return -1;
    }
    int scope = rule->pre(cg, rule->prec_pre);
    if (scope == -1) return -1;

    rule = cnm_expr_rules + cg->tok.type;
    while (rule->in && prec <= rule->prec_in) {
        scope = rule->in(cg, scope, rule->prec_pre);
        if (scope == -1) return -1;
        rule = cnm_expr_rules + cg->tok.type;
    }

    return scope;
}

#endif

