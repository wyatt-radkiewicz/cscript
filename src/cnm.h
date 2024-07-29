#ifndef _cnm_h_
#define _cnm_h_

#include <stdbool.h>
#include <stddef.h>

struct cnm;
struct cnm_type;

typedef void(*cnm_error_cb)(const char *msg);

typedef bool(*cnm_op_binary)(void *lhs, const struct cnm_type *lty,
                             void *rhs, const struct cnm_type *rty);
typedef bool(*cnm_op_unary)(struct cnm *vm, const struct cnm_type *ty);

// Push type onto vm stack
typedef bool(*cnm_getter)(struct cnm *vm, const void *from,
                          const struct cnm_type *ty, int member);
// Pop type off the stack and store it into to
typedef bool(*cnm_setter)(struct cnm *vm, void *to,
                          const struct cnm_type *ty, int member);

enum cnm_op_class {
    CNM_OP_ASSIGN,
    CNM_OP_EQEQ,
    CNM_OP_GT,
    CNM_OP_ADD,
    CNM_OP_SUB,
    CNM_OP_MUL,
    CNM_OP_DIV,
    CNM_OP_MOD,
    CNM_OP_BITAND,
    CNM_OP_BITOR,
    CNM_OP_BITXOR,
    CNM_OP_BITNOT,
    CNM_OP_CLASS_MAX,
};

// Even though you could just use the index operator overload as a way to add
// members to your types, it can be slow. Accessors will be compiled into
// indexes into the accessor array which can help performance.
struct cnm_accessor {
    const char *name;

    // You're accessor can also be simply compiled into a get X at X address
    // also. This also helps with performance.
    bool isfield;
    union {
        struct {
            int offset;
            const struct cnm_type *type;
            size_t arrlen;
        } field;
        struct {
            cnm_getter get;
            cnm_setter set;
        } ;
    };
};

struct cnm_type {
    int id;
    const char *name;
    cnm_op_binary ops[CNM_OP_CLASS_MAX];
    cnm_op_unary cast, call, index, ctor; // ctor is a static unary func

    size_t size, align;
    struct cnm_accessor *accs;
    size_t naccs;
};

struct cnm_val {
    const struct cnm_type *ty;

    // If it is a refrence, data points to a pointer to the data
    bool ref;

    // Is this value mutable?
    bool mut;

    // If this is an array, the array size. If it is also a refrence, it is
    // a fat pointer style length parameter.
    size_t arrlen;
    void *data;
};

void cnm_set_error_cb(cnm_error_cb cb);
bool cnm_set_global_type_buffer(struct cnm_type *buf, size_t size);

struct cnm_type *cnm_new_type(const char *name);
struct cnm_type *cnm_get_type(struct cnm *cnm, const char *name);
struct cnm_type *cnm_get_typeid(struct cnm *cnm, int id);

struct cnm *cnm_init(void *region, size_t regionsz);
bool cnm_parse(struct cnm *cnm, const char *src);
bool cnm_push(struct cnm *cnm, const struct cnm_val *val);
struct cnm_val cnm_pop(struct cnm *cnm);
const struct cnm_val *cnm_get(struct cnm *cnm, int stack_idx);

#endif

