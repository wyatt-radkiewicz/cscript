#include <stdalign.h>
#include <string.h>

#include "vm.h"

//
// Utility functions
//
static inline vm_error_t vm_error_init(vm_state_t *state, vm_error_code_t code) {
    return (vm_error_t){
        .errcode = code,
        .stack_pointer = state->sp,
        .stack_size = state->stack + state->stack_size - state->sp,
        .program_counter = (intptr_t)state->pc - (intptr_t)state->code,
        .callstack_len = state->rp - state->callstack + 1,
        .instr_pointer = state->curr_instr,
        .code_size = state->code_size,
    };
}

static inline uint8_t take_u8(uint8_t **code) {
    return *(*code)++;
}
static inline uint16_t take_u16(uint8_t **code) {
    uint16_t x = *(*code)++;
    x |= *(*code)++ << 8;
    return x;
}
static inline uint32_t take_u32(uint8_t **code) {
    uint32_t x = *(*code)++;
    x |= *(*code)++ << 8;
    x |= *(*code)++ << 16;
    x |= *(*code)++ << 24;
    return x;
}
static inline uint64_t take_u64(uint8_t **code) {
    uint64_t x = *(*code)++;
    x |= (uint64_t)(*(*code)++) << 8;
    x |= (uint64_t)(*(*code)++) << 16;
    x |= (uint64_t)(*(*code)++) << 24;
    x |= (uint64_t)(*(*code)++) << 32;
    x |= (uint64_t)(*(*code)++) << 40;
    x |= (uint64_t)(*(*code)++) << 48;
    x |= (uint64_t)(*(*code)++) << 56;
    return x;
}
#define cast_take_template(NAME, TYPE, WIDTH) \
    static inline TYPE take_##NAME(uint8_t **code) { \
        TYPE x; \
        *(uint##WIDTH##_t *)(&x) = take_u##WIDTH(code); \
        return x; \
    }
cast_take_template(i8, int8_t, 8)
cast_take_template(i16, int16_t, 16)
cast_take_template(i32, int32_t, 32)
cast_take_template(i64, int64_t, 64)
cast_take_template(f32, float, 32)
cast_take_template(f64, double, 64)
cast_take_template(ptr, void *, 64)

#define vm_errout(COND, ERR) \
    if (COND) { \
        *err = vm_error_init(state, ERR); \
        return false; \
    }

//
// Opcode implementations
//
static inline bool run_op_load_data(vm_state_t *state, vm_error_t *err, bool size_class) {
    const uint32_t offs = size_class ? take_u32(&state->pc) : take_u8(&state->pc);
    const uint32_t n = size_class ? take_u32(&state->pc) : take_u8(&state->pc);
    vm_errout(offs + n > state->data_size, vm_err_segfault);
    vm_errout(state->sp - n < state->stack, vm_err_stack_overflow);
    state->sp -= n;
    memcpy(state->sp, state->data + offs, n);
    return true;
}
static inline bool run_op_store_data(vm_state_t *state, vm_error_t *err, bool size_class) {
    const uint32_t offs = size_class ? take_u32(&state->pc) : take_u8(&state->pc);
    const uint32_t n = size_class ? take_u32(&state->pc) : take_u8(&state->pc);
    vm_errout(offs + n > state->data_size, vm_err_segfault);
    vm_errout(state->sp + n > state->stack + state->stack_size, vm_err_stack_overrun);
    memcpy(state->data + offs, state->sp, n);
    return true;
}
static inline bool run_op_load_data_indirect(vm_state_t *state, vm_error_t *err, bool size_class) {
    const uint32_t n = size_class ? take_u32(&state->pc) : take_u8(&state->pc);
    uint32_t offs;
    vm_errout(!vm_state_pop_u32(state, &offs), vm_err_stack_underflow);
    vm_errout(state->sp - n < state->stack, vm_err_stack_overflow);
    vm_errout(offs + n > state->data_size, vm_err_segfault);
    state->sp -= n;
    memcpy(state->sp, state->data + offs, n);
    return true;
}
static inline bool run_op_store_data_indirect(vm_state_t *state, vm_error_t *err, bool size_class) {
    const uint32_t n = size_class ? take_u32(&state->pc) : take_u8(&state->pc);
    uint32_t offs;
    vm_errout(!vm_state_pop_u32(state, &offs), vm_err_stack_underflow);
    vm_errout(state->sp + n > state->stack + state->stack_size, vm_err_stack_overrun);
    vm_errout(offs + n > state->data_size, vm_err_segfault);
    memcpy(state->data + offs, state->sp, n);
    return true;
}
static inline bool run_op_load_indirect(vm_state_t *state, vm_error_t *err, bool size_class) {
    const uint32_t n = size_class ? take_u32(&state->pc) : take_u8(&state->pc);
    void *ptr;
    vm_errout(!vm_state_pop_ptr(state, &ptr), vm_err_stack_underflow);
    //vm_errout(state->sp + n > state->stack + state->stack_size, vm_err_stack_overrun);
    vm_errout(state->sp - n < state->stack, vm_err_stack_overflow);
    state->sp -= n;
    memcpy(state->sp, ptr, n);
    return true;
}
static inline bool run_op_store_indirect(vm_state_t *state, vm_error_t *err, bool size_class) {
    const uint32_t n = size_class ? take_u32(&state->pc) : take_u8(&state->pc);
    void *ptr;
    vm_errout(!vm_state_pop_ptr(state, &ptr), vm_err_stack_underflow);
    vm_errout(state->sp + n > state->stack + state->stack_size, vm_err_stack_overrun);
    memcpy(ptr, state->sp, n);
    return true;
}
static inline bool run_op_load_stack(vm_state_t *state, vm_error_t *err, bool size_class) {
    const int32_t offs = size_class ? take_i32(&state->pc) : take_i8(&state->pc);
    const uint32_t n = size_class ? take_u32(&state->pc) : take_u8(&state->pc);
    //const uint32_t base = state->rp->stack_base;
    vm_errout(state->sp + offs - n < state->stack, vm_err_stack_overflow);
    vm_errout(state->sp + offs + n > state->stack + state->stack_size, vm_err_stack_underflow);
    //vm_errout((int64_t)base + offs + n > state->stack_size, vm_err_stack_overrun);
    //vm_errout((int64_t)base + offs < 0, vm_err_stack_overrun);
    memcpy(state->sp - n, state->sp + offs, n);
    state->sp -= n;
    return true;
}
static inline bool run_op_store_stack(vm_state_t *state, vm_error_t *err, bool size_class) {
    const int32_t offs = size_class ? take_i32(&state->pc) : take_i8(&state->pc);
    const uint32_t n = size_class ? take_u32(&state->pc) : take_u8(&state->pc);
    //const uint32_t base = state->rp->stack_base;
    vm_errout(state->sp - n < state->stack, vm_err_stack_overflow);
    vm_errout(state->sp + offs + n > state->stack + state->stack_size, vm_err_stack_underflow);
    //vm_errout(state->sp + n > state->stack + state->stack_size, vm_err_stack_underflow);
    //vm_errout((int64_t)base + offs + n > state->stack_size, vm_err_stack_overrun);
    //vm_errout((int64_t)base + offs < 0, vm_err_stack_overrun);
    //memcpy(state->stack + (int64_t)base + offs, state->sp, n);
    memcpy(state->sp + offs, state->sp, n);
    return true;
}
static inline bool run_op_load_stack_indirect(vm_state_t *state, vm_error_t *err, bool size_class) {
    const int32_t offs = size_class ? take_i32(&state->pc) : take_i8(&state->pc);
    const uint32_t n = size_class ? take_u32(&state->pc) : take_u8(&state->pc);
    const uint32_t base = state->rp->stack_base;
    int32_t base_offs;
    vm_errout(!vm_state_pop_i32(state, &base_offs), vm_err_stack_underflow);
    vm_errout(state->sp - n < state->stack, vm_err_stack_overflow);
    vm_errout((int64_t)base + offs + n > state->stack_size, vm_err_stack_overrun);
    vm_errout((int64_t)base + offs < 0, vm_err_stack_overrun);
    state->sp -= n;
    memcpy(state->sp, state->stack + (int64_t)base + base_offs + offs, n);
    return true;
}
static inline bool run_op_store_stack_indirect(vm_state_t *state, vm_error_t *err, bool size_class) {
    const int32_t offs = size_class ? take_i32(&state->pc) : take_i8(&state->pc);
    const uint32_t n = size_class ? take_u32(&state->pc) : take_u8(&state->pc);
    const uint32_t base = state->rp->stack_base;
    int32_t base_offs;
    vm_errout(!vm_state_pop_i32(state, &base_offs), vm_err_stack_underflow);
    vm_errout(state->sp + n > state->stack + state->stack_size, vm_err_stack_underflow);
    vm_errout((int64_t)base + offs + n > state->stack_size, vm_err_stack_overrun);
    vm_errout((int64_t)base + offs < 0, vm_err_stack_overrun);
    memcpy(state->stack + (int64_t)base + base_offs + offs, state->sp, n);
    return true;
}
static inline bool run_op_sub_stack(vm_state_t *state, vm_error_t *err, bool size_class) {
    uint32_t n = size_class ? take_u32(&state->pc) : take_u8(&state->pc);
    //vm_errout(!vm_state_pop_u32(state, &n), vm_err_stack_underflow);
    vm_errout(state->sp - n < state->stack, vm_err_stack_overflow);
    state->sp -= n;
    return true;
}
static inline bool run_op_add_stack(vm_state_t *state, vm_error_t *err, bool size_class) {
    uint32_t n = size_class ? take_u32(&state->pc) : take_u8(&state->pc);
    //vm_errout(!vm_state_pop_u32(state, &n), vm_err_stack_underflow);
    vm_errout(state->sp + n > state->stack + state->stack_size, vm_err_stack_underflow);
    state->sp += n;
    return true;
}
#define binary_op_type(OP, TYPE, NAME, FLAGS) \
    case FLAGS: { \
        TYPE a, b; \
        vm_errout(!vm_state_pop_##NAME(state, &b), vm_err_stack_underflow); \
        vm_errout(!vm_state_pop_##NAME(state, &a), vm_err_stack_underflow); \
        vm_state_push_##NAME(state, a OP b); \
        } return true;
#define binary_op_template(OP, OPNAME) \
static inline bool run_##OPNAME(vm_state_t *state, vm_error_t *err, bool flag) { \
    const uint8_t flags = take_u8(&state->pc); \
    switch (flags) { \
    binary_op_type(OP, int32_t, i32, 0b000) \
    binary_op_type(OP, uint32_t, u32, 0b100) \
    binary_op_type(OP, int64_t, i64, 0b001) \
    binary_op_type(OP, uint64_t, u64, 0b101) \
    binary_op_type(OP, float, f32, 0b010) \
    binary_op_type(OP, double, f64, 0b011) \
    /* Unreachable */ \
    default: \
        *err = vm_error_init(state, vm_err_illegal_instr); \
        return false; \
    } \
}
binary_op_template(+, op_add)
binary_op_template(-, op_sub)
binary_op_template(*, op_mul)
binary_op_template(/, op_div)
#undef binary_op_template
#undef binary_op_type
#define bit_op_template(OP, OPNAME) \
static inline bool run_##OPNAME(vm_state_t *state, vm_error_t *err, bool bits64) { \
    if (bits64) { \
        uint64_t a, b; \
        vm_errout(!vm_state_pop_u64(state, &b), vm_err_stack_underflow); \
        vm_errout(!vm_state_pop_u64(state, &a), vm_err_stack_underflow); \
        vm_state_push_u64(state, a OP b); \
    } else { \
        uint32_t a, b; \
        vm_errout(!vm_state_pop_u32(state, &b), vm_err_stack_underflow); \
        vm_errout(!vm_state_pop_u32(state, &a), vm_err_stack_underflow); \
        vm_state_push_u32(state, a OP b); \
    } \
    return true; \
}
bit_op_template(&, op_and)
bit_op_template(&, op_or)
bit_op_template(&, op_xor)
bit_op_template(&, op_sll)
bit_op_template(&, op_srl)
#undef bit_op_template
static inline bool run_op_sra(vm_state_t *state, vm_error_t *err, bool bits64) {
    if (bits64) {
        int64_t a, b;
        vm_errout(!vm_state_pop_i64(state, &b), vm_err_stack_underflow);
        vm_errout(!vm_state_pop_i64(state, &a), vm_err_stack_underflow);
        vm_state_push_i64(state, a >> b);
    } else {
        int32_t a, b;
        vm_errout(!vm_state_pop_i32(state, &b), vm_err_stack_underflow);
        vm_errout(!vm_state_pop_i32(state, &a), vm_err_stack_underflow);
        vm_state_push_i32(state, a >> b);
    }
    return true;
}
static inline bool run_op_mod(vm_state_t *state, vm_error_t *err, bool flag) {
    const uint8_t flags = take_u8(&state->pc);
    switch (flags) {
    case 0b00: {
        int32_t a, b;
        vm_errout(!vm_state_pop_i32(state, &b), vm_err_stack_underflow);
        vm_errout(!vm_state_pop_i32(state, &a), vm_err_stack_underflow);
        vm_state_push_i32(state, a % b);
        } return true;
    case 0b01: {
        int64_t a, b;
        vm_errout(!vm_state_pop_i64(state, &b), vm_err_stack_underflow);
        vm_errout(!vm_state_pop_i64(state, &a), vm_err_stack_underflow);
        vm_state_push_i64(state, a % b);
        } return true;
    case 0b10: {
        uint32_t a, b;
        vm_errout(!vm_state_pop_u32(state, &b), vm_err_stack_underflow);
        vm_errout(!vm_state_pop_u32(state, &a), vm_err_stack_underflow);
        vm_state_push_u32(state, a % b);
        } return true;
    case 0b11: {
        uint64_t a, b;
        vm_errout(!vm_state_pop_u64(state, &b), vm_err_stack_underflow);
        vm_errout(!vm_state_pop_u64(state, &a), vm_err_stack_underflow);
        vm_state_push_u64(state, a % b);
        } return true;
    // Unreachable
    default:
        *err = vm_error_init(state, vm_err_illegal_instr);
        return false;
    }
}
static inline bool run_op_not(vm_state_t *state, vm_error_t *err, bool bits64) {
    if (bits64) {
        uint64_t a;
        vm_errout(!vm_state_pop_u64(state, &a), vm_err_stack_underflow);
        vm_state_push_u64(state, ~a);
    } else {
        uint32_t a;
        vm_errout(!vm_state_pop_u32(state, &a), vm_err_stack_underflow);
        vm_state_push_u32(state, ~a);
    }
    return true;
}
static inline bool run_op_neg(vm_state_t *state, vm_error_t *err, bool flag) {
    const uint8_t flags = take_u8(&state->pc);
    switch (flags) {
    case 0b00: {
        int32_t a;
        vm_errout(!vm_state_pop_i32(state, &a), vm_err_stack_underflow);
        vm_state_push_i32(state, -a);
        } return true;
    case 0b01: {
        int64_t a;
        vm_errout(!vm_state_pop_i64(state, &a), vm_err_stack_underflow);
        vm_state_push_i64(state, -a);
        } return true;
    case 0b10: {
        float a;
        vm_errout(!vm_state_pop_f32(state, &a), vm_err_stack_underflow);
        vm_state_push_f32(state, -a);
        } return true;
    case 0b11: {
        double a;
        vm_errout(!vm_state_pop_f64(state, &a), vm_err_stack_underflow);
        vm_state_push_f64(state, -a);
        } return true;
    // Unreachable
    default:
        *err = vm_error_init(state, vm_err_illegal_instr);
        return false;
    }
}
static inline bool run_op_extend(vm_state_t *state, vm_error_t *err, bool u) {
    const uint8_t size_class = take_u8(&state->pc);
    switch (size_class | u << 2) {
    case 0b100: {
        uint8_t a;
        vm_errout(!vm_state_pop_u8(state, &a), vm_err_stack_underflow);
        vm_errout(!vm_state_push_u16(state, a), vm_err_stack_overflow);
        } return true;
    case 0b000: {
        int8_t a;
        vm_errout(!vm_state_pop_i8(state, &a), vm_err_stack_underflow);
        vm_errout(!vm_state_push_i16(state, a), vm_err_stack_overflow);
        } return true;
    case 0b101: {
        uint16_t a;
        vm_errout(!vm_state_pop_u16(state, &a), vm_err_stack_underflow);
        vm_errout(!vm_state_push_u32(state, a), vm_err_stack_overflow);
        } return true;
    case 0b001: {
        int16_t a;
        vm_errout(!vm_state_pop_i16(state, &a), vm_err_stack_underflow);
        vm_errout(!vm_state_push_i32(state, a), vm_err_stack_overflow);
        } return true;
    case 0b110: {
        uint32_t a;
        vm_errout(!vm_state_pop_u32(state, &a), vm_err_stack_underflow);
        vm_errout(!vm_state_push_u64(state, a), vm_err_stack_overflow);
        } return true;
    case 0b010: {
        int32_t a;
        vm_errout(!vm_state_pop_i32(state, &a), vm_err_stack_underflow);
        vm_errout(!vm_state_push_i64(state, a), vm_err_stack_overflow);
        } return true;
    // Unreachable
    default:
        *err = vm_error_init(state, vm_err_illegal_instr);
        return false;
    }
}
static inline bool run_op_reduce(vm_state_t *state, vm_error_t *err, bool flag) {
    const uint8_t size_class = take_u8(&state->pc);
    switch (size_class) {
    case 0b01: {
        uint16_t a;
        vm_errout(!vm_state_pop_u16(state, &a), vm_err_stack_underflow);
        vm_state_push_u8(state, a);
        } return true;
    case 0b10: {
        uint32_t a;
        vm_errout(!vm_state_pop_u32(state, &a), vm_err_stack_underflow);
        vm_state_push_u16(state, a);
        } return true;
    case 0b11: {
        uint64_t a;
        vm_errout(!vm_state_pop_u64(state, &a), vm_err_stack_underflow);
        vm_state_push_u32(state, a);
        } return true;
    // Unreachable
    default:
        *err = vm_error_init(state, vm_err_illegal_instr);
        return false;
    }
}
static inline bool run_op_f2u(vm_state_t *state, vm_error_t *err, bool bits64) {
    if (bits64) {
        double a;
        vm_errout(!vm_state_pop_f64(state, &a), vm_err_stack_underflow);
        vm_state_push_u64(state, a);
    } else {
        float a;
        vm_errout(!vm_state_pop_f32(state, &a), vm_err_stack_underflow);
        vm_state_push_u32(state, a);
    }
    return true;
}
static inline bool run_op_f2i(vm_state_t *state, vm_error_t *err, bool bits64) {
    if (bits64) {
        double a;
        vm_errout(!vm_state_pop_f64(state, &a), vm_err_stack_underflow);
        vm_state_push_i64(state, a);
    } else {
        float a;
        vm_errout(!vm_state_pop_f32(state, &a), vm_err_stack_underflow);
        vm_state_push_i32(state, a);
    }
    return true;
}
static inline bool run_op_i2f(vm_state_t *state, vm_error_t *err, bool bits64) {
    if (bits64) {
        int64_t a;
        vm_errout(!vm_state_pop_i64(state, &a), vm_err_stack_underflow);
        vm_state_push_f64(state, a);
    } else {
        int32_t a;
        vm_errout(!vm_state_pop_i32(state, &a), vm_err_stack_underflow);
        vm_state_push_f32(state, a);
    }
    return true;
}
static inline bool run_op_u2f(vm_state_t *state, vm_error_t *err, bool bits64) {
    if (bits64) {
        uint64_t a;
        vm_errout(!vm_state_pop_u64(state, &a), vm_err_stack_underflow);
        vm_state_push_f64(state, a);
    } else {
        uint32_t a;
        vm_errout(!vm_state_pop_u32(state, &a), vm_err_stack_underflow);
        vm_state_push_f32(state, a);
    }
    return true;
}
static inline bool run_op_f2d(vm_state_t *state, vm_error_t *err, bool flag) {
    float a;
    vm_errout(!vm_state_pop_f32(state, &a), vm_err_stack_underflow);
    vm_errout(!vm_state_push_f64(state, a), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_d2f(vm_state_t *state, vm_error_t *err, bool flag) {
    double a;
    vm_errout(!vm_state_pop_f64(state, &a), vm_err_stack_underflow);
    vm_state_push_f32(state, a);
    return true;
}
#define test_op_template(OP, OPNAME) \
static inline bool run_##OPNAME(vm_state_t *state, vm_error_t *err, bool flag) { \
    const uint8_t flags = take_u8(&state->pc); \
    switch (flags) { \
    /* 32 bit ints */ \
    case 0b00: { \
            uint32_t a, b; \
            vm_errout(!vm_state_pop_u32(state, &b), vm_err_stack_underflow); \
            vm_errout(!vm_state_pop_u32(state, &a), vm_err_stack_underflow); \
            vm_state_push_u32(state, a OP b); \
        } return true; \
    /* 64 bit ints */ \
    case 0b01: { \
            uint64_t a, b; \
            vm_errout(!vm_state_pop_u64(state, &b), vm_err_stack_underflow); \
            vm_errout(!vm_state_pop_u64(state, &a), vm_err_stack_underflow); \
            vm_state_push_u32(state, a OP b); \
        } return true; \
    /* 32 bit floats */ \
    case 0b10: { \
            float a, b; \
            vm_errout(!vm_state_pop_f32(state, &b), vm_err_stack_underflow); \
            vm_errout(!vm_state_pop_f32(state, &a), vm_err_stack_underflow); \
            vm_state_push_u32(state, a OP b); \
        } return true; \
    /* 64 bit floats */ \
    case 0b11: { \
            double a, b; \
            vm_errout(!vm_state_pop_f64(state, &b), vm_err_stack_underflow); \
            vm_errout(!vm_state_pop_f64(state, &a), vm_err_stack_underflow); \
            vm_state_push_u32(state, a OP b); \
        } return true; \
    /* Unreachable */ \
    default: \
        *err = vm_error_init(state, vm_err_illegal_instr); \
        return false; \
    } \
    return true; \
}
test_op_template(==, op_push_eq)
test_op_template(!=, op_push_ne)
#undef test_op_template
#define test_op_template(OP, OPNAME) \
static inline bool run_##OPNAME(vm_state_t *state, vm_error_t *err, bool flag) { \
    const uint8_t flags = take_u8(&state->pc); \
    switch (flags) { \
    /* 32 bit ints */ \
    case 0b000: { \
            int32_t a, b; \
            vm_errout(!vm_state_pop_i32(state, &b), vm_err_stack_underflow); \
            vm_errout(!vm_state_pop_i32(state, &a), vm_err_stack_underflow); \
            vm_state_push_u32(state, a OP b); \
        } return true; \
    /* 64 bit ints */ \
    case 0b001: { \
            int64_t a, b; \
            vm_errout(!vm_state_pop_i64(state, &b), vm_err_stack_underflow); \
            vm_errout(!vm_state_pop_i64(state, &a), vm_err_stack_underflow); \
            vm_state_push_u32(state, a OP b); \
        } return true; \
    /* 32 bit unsigned ints */ \
    case 0b100: { \
            uint32_t a, b; \
            vm_errout(!vm_state_pop_u32(state, &b), vm_err_stack_underflow); \
            vm_errout(!vm_state_pop_u32(state, &a), vm_err_stack_underflow); \
            vm_state_push_u32(state, a OP b); \
        } return true; \
    /* 64 bit unsigned ints */ \
    case 0b101: { \
            uint64_t a, b; \
            vm_errout(!vm_state_pop_u64(state, &b), vm_err_stack_underflow); \
            vm_errout(!vm_state_pop_u64(state, &a), vm_err_stack_underflow); \
            vm_state_push_u32(state, a OP b); \
        } return true; \
    /* 32 bit floats */ \
    case 0b010: { \
            float a, b; \
            vm_errout(!vm_state_pop_f32(state, &b), vm_err_stack_underflow); \
            vm_errout(!vm_state_pop_f32(state, &a), vm_err_stack_underflow); \
            vm_state_push_u32(state, a OP b); \
        } return true; \
    /* 64 bit floats */ \
    case 0b011: { \
            double a, b; \
            vm_errout(!vm_state_pop_f64(state, &b), vm_err_stack_underflow); \
            vm_errout(!vm_state_pop_f64(state, &a), vm_err_stack_underflow); \
            vm_state_push_u32(state, a OP b); \
        } return true; \
    /* Unreachable */ \
    default: \
        *err = vm_error_init(state, vm_err_illegal_instr); \
        return false; \
    } \
    return true; \
}
test_op_template(>, op_push_gt)
test_op_template(<, op_push_lt)
test_op_template(>=, op_push_ge)
test_op_template(<=, op_push_le)
#undef test_op_template
static inline bool run_op_ret(vm_state_t *state, vm_error_t *err, bool flag) {
    vm_errout(state->rp < state->callstack, vm_err_callstack_underflow);
    const vm_callstack_t x = *state->rp--;
    if (x.ret_loc == UINT32_MAX) {
        *err = vm_error_init(state, vm_err_okay);
        return false;
    } else {
        state->pc = state->code + x.ret_loc;
        return true;
    }
}
static inline bool run_op_call(vm_state_t *state, vm_error_t *err, bool flag) {
    const uint32_t loc = take_u32(&state->pc);
    vm_errout(state->rp + 1 >= state->rp + state->callstack_size, vm_err_callstack_overflow);
    state->rp++;
    state->rp->ret_loc = state->pc - state->code;
    state->rp->stack_base = state->sp - state->stack;
    state->pc = state->code + loc;
    return true;
}
static inline bool run_op_call_indirect(vm_state_t *state, vm_error_t *err, bool flag) {
    uint32_t loc;
    vm_errout(!vm_state_pop_u32(state, &loc), vm_err_callstack_underflow);
    vm_errout(state->rp + 1 >= state->callstack + state->callstack_size, vm_err_callstack_overflow);
    state->rp++;
    state->rp->ret_loc = state->pc - state->code;
    state->rp->stack_base = state->sp - state->stack;
    state->pc = state->code + loc;
    return true;
}
static inline bool run_op_extern_call(vm_state_t *state, vm_error_t *err, bool flag) {
    const uint32_t idx = take_u32(&state->pc);
    vm_errout(idx >= state->pfn_size, vm_err_invalid_pfn);
    vm_errout(!state->pfn[idx], vm_err_invalid_pfn);
    state->pfn[idx](state);
    return true;
}
static inline bool run_op_extern_call_indirect(vm_state_t *state, vm_error_t *err, bool flag) {
    uint32_t idx;
    vm_errout(!vm_state_pop_u32(state, &idx), vm_err_callstack_underflow);
    vm_errout(idx >= state->pfn_size, vm_err_invalid_pfn);
    state->pfn[idx](state);
    return true;
}
static inline bool run_op_jump(vm_state_t *state, vm_error_t *err, bool flag) {
    const int32_t offs = take_i32(&state->pc);
    vm_errout(state->pc + offs >= state->code + state->code_size, vm_err_code_segfault);
    vm_errout(state->pc - offs < state->code, vm_err_code_segfault);
    state->pc += offs;
    return true;
}
static inline bool run_op_jump_indirect(vm_state_t *state, vm_error_t *err, bool flag) {
    int32_t offs;
    vm_errout(!vm_state_pop_i32(state, &offs), vm_err_callstack_underflow);
    vm_errout(state->pc + offs >= state->code + state->code_size, vm_err_code_segfault);
    vm_errout(state->pc - offs < state->code, vm_err_code_segfault);
    state->pc += offs;
    return true;
}
static inline bool run_op_bne(vm_state_t *state, vm_error_t *err, bool bits64) {
    const int32_t offs = take_i32(&state->pc);
    vm_errout(state->pc + offs >= state->code + state->code_size, vm_err_code_segfault);
    vm_errout(state->pc - offs < state->code, vm_err_code_segfault);

    if (bits64) {
        uint64_t x;
        vm_errout(!vm_state_pop_u64(state, &x), vm_err_stack_underflow);
        if (x) state->pc += offs;
    } else {
        uint32_t x;
        vm_errout(!vm_state_pop_u32(state, &x), vm_err_stack_underflow);
        if (x) state->pc += offs;
    }
    return true;
}
static inline bool run_op_beq(vm_state_t *state, vm_error_t *err, bool bits64) {
    const int32_t offs = take_i32(&state->pc);
    vm_errout(state->pc + offs >= state->code + state->code_size, vm_err_code_segfault);
    vm_errout(state->pc - offs < state->code, vm_err_code_segfault);

    if (bits64) {
        uint64_t x;
        vm_errout(!vm_state_pop_u64(state, &x), vm_err_stack_underflow);
        if (!x) state->pc += offs;
    } else {
        uint32_t x;
        vm_errout(!vm_state_pop_u32(state, &x), vm_err_stack_underflow);
        if (!x) state->pc += offs;
    }
    return true;
}
static inline bool run_op_imm_i8(vm_state_t *state, vm_error_t *err, bool flag) {
    int8_t imm = take_i8(&state->pc);
    vm_errout(!vm_state_push_i8(state, imm), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_imm_i16(vm_state_t *state, vm_error_t *err, bool size_class) {
    int16_t imm = size_class ? take_i16(&state->pc) : take_i8(&state->pc);
    vm_errout(!vm_state_push_i16(state, imm), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_imm_i32(vm_state_t *state, vm_error_t *err, bool size_class) {
    int32_t imm = size_class ? take_i32(&state->pc) : take_i8(&state->pc);
    vm_errout(!vm_state_push_i32(state, imm), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_imm_i64(vm_state_t *state, vm_error_t *err, bool size_class) {
    int64_t imm = size_class ? take_i64(&state->pc) : take_i8(&state->pc);
    vm_errout(!vm_state_push_i64(state, imm), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_imm_u8(vm_state_t *state, vm_error_t *err, bool flag) {
    uint8_t imm = take_u8(&state->pc);
    vm_errout(!vm_state_push_u8(state, imm), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_imm_u16(vm_state_t *state, vm_error_t *err, bool size_class) {
    uint16_t imm = size_class ? take_i16(&state->pc) : take_i8(&state->pc);
    vm_errout(!vm_state_push_u16(state, imm), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_imm_u32(vm_state_t *state, vm_error_t *err, bool size_class) {
    uint32_t imm = size_class ? take_i32(&state->pc) : take_i8(&state->pc);
    vm_errout(!vm_state_push_u32(state, imm), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_imm_u64(vm_state_t *state, vm_error_t *err, bool size_class) {
    uint64_t imm = size_class ? take_i32(&state->pc) : take_i8(&state->pc);
    vm_errout(!vm_state_push_u64(state, imm), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_imm_f32(vm_state_t *state, vm_error_t *err, bool flag) {
    float imm = take_f32(&state->pc);
    vm_errout(!vm_state_push_f32(state, imm), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_imm_f64(vm_state_t *state, vm_error_t *err, bool flag) {
    double imm = take_f64(&state->pc);
    vm_errout(!vm_state_push_f64(state, imm), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_imm_ptr(vm_state_t *state, vm_error_t *err, bool flag) {
    void *imm = take_ptr(&state->pc);
    vm_errout(!vm_state_push_ptr(state, imm), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_alloc(vm_state_t *state, vm_error_t *err, bool size_class) {
    const uint32_t n = size_class ? take_u32(&state->pc) : take_u8(&state->pc);
    assert(state->alloc && "Virtual machine must have an alloc pfn!");
    void *ptr = state->alloc(state->alloc_user, n);
    vm_errout(!vm_state_push_ptr(state, ptr), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_alloc_indirect(vm_state_t *state, vm_error_t *err, bool flag) {
    uint32_t n;
    vm_errout(!vm_state_pop_u32(state, &n), vm_err_stack_underflow);
    assert(state->alloc && "Virtual machine must have an alloc pfn!");
    void *ptr = state->alloc(state->alloc_user, n);
    vm_errout(!vm_state_push_ptr(state, ptr), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_free_indirect(vm_state_t *state, vm_error_t *err, bool flag) {
    void *ptr;
    vm_errout(!vm_state_pop_ptr(state, &ptr), vm_err_stack_underflow);
    if (!state->free) return true;
    state->free(state->alloc_user, ptr);
    return true;
}

//
// Virtual Machine
//
static bool vm_state_run_instr(vm_state_t *state, vm_error_t *err) {
    vm_errout(state->pc > state->code + state->code_size, vm_err_code_segfault);
    state->curr_instr = state->pc;
    const uint8_t byte = *state->pc++;
    const bool flag = byte & 0x80;

#define X(ENUM) case ENUM: return run_##ENUM(state, err, flag);
    switch ((vm_opcode_t)(byte & 0x7f)) {
    VM_OPCODES
    default:
        *err = vm_error_init(state, vm_err_illegal_instr);
        return false;
        break;
    }
#undef X

    return true;
}
vm_error_t vm_state_run(vm_state_t *state, uint32_t offs, bool reset) {
    state->pc = state->code + offs;
    if (reset) {
        state->sp = state->stack + state->stack_size;
        state->rp = state->callstack;
        *state->rp = (vm_callstack_t){
            .ret_loc = UINT32_MAX,
            .stack_base = UINT32_MAX,
        };
    } else {
        if (state->rp - state->callstack + 1 >= state->callstack_size) {
            return vm_error_init(state, vm_err_callstack_overflow);
        }
        *(++state->rp) = (vm_callstack_t){
            .ret_loc = UINT32_MAX,
            .stack_base = UINT32_MAX,
        };
    }

    vm_error_t err;
    while (vm_state_run_instr(state, &err));
    return err;
}
#define vm_push_template(NAME, TYPE) \
    bool vm_state_push_##NAME(vm_state_t *state, TYPE x) { \
        if (state->sp - sizeof(x) < state->stack) return false; \
        state->sp -= ((uintptr_t)state->sp & (alignof(TYPE) - 1)) + sizeof(x); \
        *(TYPE *)state->sp = x; \
        return true; \
    }
vm_push_template(i8, int8_t);
vm_push_template(i16, int16_t);
vm_push_template(i32, int32_t);
vm_push_template(i64, int64_t);
vm_push_template(u8, uint8_t);
vm_push_template(u16, uint16_t);
vm_push_template(u32, uint32_t);
vm_push_template(u64, uint64_t);
vm_push_template(ptr, void *);
vm_push_template(f32, float);
vm_push_template(f64, double);
#define vm_pop_template(NAME, TYPE) \
    bool vm_state_pop_##NAME(vm_state_t *state, TYPE *x) { \
        if (state->sp + sizeof(*x) > state->stack + state->stack_size) return false; \
        *x = *(TYPE *)state->sp; \
        state->sp += sizeof(*x); \
        return true; \
    }
vm_pop_template(i8, int8_t)
vm_pop_template(i16, int16_t)
vm_pop_template(i32, int32_t)
vm_pop_template(i64, int64_t)
vm_pop_template(u8, uint8_t)
vm_pop_template(u16, uint16_t)
vm_pop_template(u32, uint32_t)
vm_pop_template(u64, uint64_t)
vm_pop_template(ptr, void *)
vm_pop_template(f32, float)
vm_pop_template(f64, double)

#define X(ENUM) case ENUM: return #ENUM;
static const char *vm_error_code_to_str(vm_error_code_t code) {
    switch (code) {
    VM_ERROR_CODES
    default: return "";
    }
}
#undef X
void vm_error_log(const vm_error_t err, FILE *out) {
    fprintf(out, "(vm_error_t){\n");
    fprintf(out, "\terrcode: %s,\n", vm_error_code_to_str(err.errcode));
    fprintf(out, "\tprogram_counter: %ld,\n", err.program_counter);
    fprintf(out, "\tinstr: ");
    if (err.program_counter < err.code_size && err.instr_pointer) {
        vm_opcode_log(&(const uint8_t *){err.instr_pointer}, out);
    } else {
        fprintf(out, "<null>");
    }
    fprintf(out, "\n");
    fprintf(out, "\tstack_pointer: %p,\n", err.stack_pointer);
    fprintf(out, "\tstack_size: %zu,\n", err.stack_size);
    fprintf(out, "\tcallstack_len: %zu,\n", err.callstack_len);
    fprintf(out, "}\n");
}

#define flagstr(FLAG, BIT) (((FLAG) & (BIT)) ? "true" : "false")
static void log_op_load_data(uint8_t **code, FILE *out, bool size_class) {
    if (size_class) {
        fprintf(out, "offs: %ud, ", take_u32(code));
        fprintf(out, "n: %ud", take_u32(code));
    } else {
        fprintf(out, "offs: %ud, ", take_u8(code));
        fprintf(out, "n: %ud", take_u8(code));
    }
}
static void log_op_store_data(uint8_t **code, FILE *out, bool size_class) {
    if (size_class) {
        fprintf(out, "offs: %ud, ", take_u32(code));
        fprintf(out, "n: %ud", take_u32(code));
    } else {
        fprintf(out, "offs: %ud, ", take_u8(code));
        fprintf(out, "n: %ud", take_u8(code));
    }
}
static void log_op_load_data_indirect(uint8_t **code, FILE *out, bool size_class) {
    if (size_class) fprintf(out, "n: %ud", take_u32(code));
    else fprintf(out, "n: %ud", take_u8(code));
}
static void log_op_store_data_indirect(uint8_t **code, FILE *out, bool size_class) {
    if (size_class) fprintf(out, "n: %ud", take_u32(code));
    else fprintf(out, "n: %ud", take_u8(code));
}
static void log_op_load_indirect(uint8_t **code, FILE *out, bool size_class) {
    if (size_class) fprintf(out, "n: %ud", take_u32(code));
    else fprintf(out, "n: %ud", take_u8(code));
}
static void log_op_store_indirect(uint8_t **code, FILE *out, bool size_class) {
    if (size_class) fprintf(out, "n: %ud", take_u32(code));
    else fprintf(out, "n: %ud", take_u8(code));
}
static void log_op_load_stack(uint8_t **code, FILE *out, bool size_class) {
    if (size_class) {
        fprintf(out, "offs: %ud, ", take_u32(code));
        fprintf(out, "n: %ud", take_u32(code));
    } else {
        fprintf(out, "offs: %ud, ", take_u8(code));
        fprintf(out, "n: %ud", take_u8(code));
    }
}
static void log_op_store_stack(uint8_t **code, FILE *out, bool size_class) {
    if (size_class) {
        fprintf(out, "offs: %ud, ", take_u32(code));
        fprintf(out, "n: %ud", take_u32(code));
    } else {
        fprintf(out, "offs: %ud, ", take_u8(code));
        fprintf(out, "n: %ud", take_u8(code));
    }
}
static void log_op_load_stack_indirect(uint8_t **code, FILE *out, bool size_class) {
    if (size_class) {
        fprintf(out, "offs: %ud, ", take_u32(code));
        fprintf(out, "n: %ud", take_u32(code));
    } else {
        fprintf(out, "offs: %ud, ", take_u8(code));
        fprintf(out, "n: %ud", take_u8(code));
    }
}
static void log_op_store_stack_indirect(uint8_t **code, FILE *out, bool size_class) {
    if (size_class) {
        fprintf(out, "offs: %ud, ", take_u32(code));
        fprintf(out, "n: %ud", take_u32(code));
    } else {
        fprintf(out, "offs: %ud, ", take_u8(code));
        fprintf(out, "n: %ud", take_u8(code));
    }
}
static void log_op_sub_stack(uint8_t **code, FILE *out, bool size_class) {
    if (size_class) fprintf(out, "n: %ud", take_u32(code));
    else fprintf(out, "n: %ud", take_u8(code));
}
static void log_op_add_stack(uint8_t **code, FILE *out, bool size_class) {
    if (size_class) fprintf(out, "n: %ud", take_u32(code));
    else fprintf(out, "n: %ud", take_u8(code));
}
static void log_op_add(uint8_t **code, FILE *out, bool flag) {
    const uint8_t flags = take_u8(code);
    fprintf(out, "bits64: %s, fp: %s, u: %s", flagstr(flags, 0), flagstr(flags, 1), flagstr(flags, 2));
}
static void log_op_sub(uint8_t **code, FILE *out, bool flag) {
    const uint8_t flags = take_u8(code);
    fprintf(out, "bits64: %s, fp: %s, u: %s", flagstr(flags, 0), flagstr(flags, 1), flagstr(flags, 2));
}
static void log_op_mul(uint8_t **code, FILE *out, bool flag) {
    const uint8_t flags = take_u8(code);
    fprintf(out, "bits64: %s, fp: %s, u: %s", flagstr(flags, 0), flagstr(flags, 1), flagstr(flags, 2));
}
static void log_op_div(uint8_t **code, FILE *out, bool flag) {
    const uint8_t flags = take_u8(code);
    fprintf(out, "bits64: %s, fp: %s, u: %s", flagstr(flags, 0), flagstr(flags, 1), flagstr(flags, 2));
}
static void log_op_and(uint8_t **code, FILE *out, bool flag) {
    fprintf(out, "bits64: %s", flag ? "true" : "false");
}
static void log_op_or(uint8_t **code, FILE *out, bool flag) {
    fprintf(out, "bits64: %s", flag ? "true" : "false");
}
static void log_op_xor(uint8_t **code, FILE *out, bool flag) {
    fprintf(out, "bits64: %s", flag ? "true" : "false");
}
static void log_op_sll(uint8_t **code, FILE *out, bool flag) {
    fprintf(out, "bits64: %s", flag ? "true" : "false");
}
static void log_op_srl(uint8_t **code, FILE *out, bool flag) {
    fprintf(out, "bits64: %s", flag ? "true" : "false");
}
static void log_op_sra(uint8_t **code, FILE *out, bool flag) {
    fprintf(out, "bits64: %s", flag ? "true" : "false");
}
static void log_op_not(uint8_t **code, FILE *out, bool flag) {
    fprintf(out, "bits64: %s", flag ? "true" : "false");
}
static void log_op_mod(uint8_t **code, FILE *out, bool flag) {
    const uint8_t flags = take_u8(code);
    fprintf(out, "bits64: %s, u: %s", flagstr(flags, 0), flagstr(flags, 1));
}
static void log_op_neg(uint8_t **code, FILE *out, bool flag) {
    const uint8_t flags = take_u8(code);
    fprintf(out, "bits64: %s, u: %s", flagstr(flags, 0), flagstr(flags, 1));
}
static void log_op_extend(uint8_t **code, FILE *out, bool u) {
    const uint8_t size_class = take_u8(code);
    fprintf(out, "u: %s, size_class: %u", u ? "true" : "false", size_class);
}
static void log_op_reduce(uint8_t **code, FILE *out, bool u) {
    const uint8_t size_class = take_u8(code);
    fprintf(out, "u: %s, size_class: %u", u ? "true" : "false", size_class);
}
static void log_op_f2u(uint8_t **code, FILE *out, bool flag) {
    fprintf(out, "bits64: %s", flag ? "true" : "false");
}
static void log_op_f2i(uint8_t **code, FILE *out, bool flag) {
    fprintf(out, "bits64: %s", flag ? "true" : "false");
}
static void log_op_i2f(uint8_t **code, FILE *out, bool flag) {
    fprintf(out, "bits64: %s", flag ? "true" : "false");
}
static void log_op_u2f(uint8_t **code, FILE *out, bool flag) {
    fprintf(out, "bits64: %s", flag ? "true" : "false");
}
static void log_op_f2d(uint8_t **code, FILE *out, bool flag) {}
static void log_op_d2f(uint8_t **code, FILE *out, bool flag) {}
static void log_op_push_eq(uint8_t **code, FILE *out, bool flag) {
    const uint8_t flags = take_u8(code);
    fprintf(out, "bits64: %s, fp: %s", flagstr(flags, 0), flagstr(flags, 1));
}
static void log_op_push_ne(uint8_t **code, FILE *out, bool flag) {
    const uint8_t flags = take_u8(code);
    fprintf(out, "bits64: %s, fp: %s", flagstr(flags, 0), flagstr(flags, 1));
}
static void log_op_push_gt(uint8_t **code, FILE *out, bool flag) {
    const uint8_t flags = take_u8(code);
    fprintf(out, "bits64: %s, fp: %s, u: %s", flagstr(flags, 0), flagstr(flags, 1), flagstr(flags, 2));
}
static void log_op_push_lt(uint8_t **code, FILE *out, bool flag) {
    const uint8_t flags = take_u8(code);
    fprintf(out, "bits64: %s, fp: %s, u: %s", flagstr(flags, 0), flagstr(flags, 1), flagstr(flags, 2));
}
static void log_op_push_ge(uint8_t **code, FILE *out, bool flag) {
    const uint8_t flags = take_u8(code);
    fprintf(out, "bits64: %s, fp: %s, u: %s", flagstr(flags, 0), flagstr(flags, 1), flagstr(flags, 2));
}
static void log_op_push_le(uint8_t **code, FILE *out, bool flag) {
    const uint8_t flags = take_u8(code);
    fprintf(out, "bits64: %s, fp: %s, u: %s", flagstr(flags, 0), flagstr(flags, 1), flagstr(flags, 2));
}
static void log_op_ret(uint8_t **code, FILE *out, bool flag) {}
static void log_op_call(uint8_t **code, FILE *out, bool flag) {
    fprintf(out, "offs: %ud", take_u32(code));
}
static void log_op_call_indirect(uint8_t **code, FILE *out, bool flag) {}
static void log_op_extern_call(uint8_t **code, FILE *out, bool flag) {
    fprintf(out, "fn_idx: %ud", take_u32(code));
}
static void log_op_extern_call_indirect(uint8_t **code, FILE *out, bool flag) {}
static void log_op_jump(uint8_t **code, FILE *out, bool flag) {
    fprintf(out, "offs: %d", take_i32(code));
}
static void log_op_jump_indirect(uint8_t **code, FILE *out, bool flag) {}
static void log_op_bne(uint8_t **code, FILE *out, bool flag) {
    fprintf(out, "bits64: %s, offs: %d", flag ? "true" : "false", take_i32(code));
}
static void log_op_beq(uint8_t **code, FILE *out, bool flag) {
    fprintf(out, "bits64: %s, offs: %d", flag ? "true" : "false", take_i32(code));
}
static void log_op_imm_i8(uint8_t **code, FILE *out, bool flag) {
    fprintf(out, "imm: %d", take_i8(code));
}
static void log_op_imm_i16(uint8_t **code, FILE *out, bool size_class) {
    if (size_class) fprintf(out, "imm: %d", take_i16(code));
    else fprintf(out, "imm: %d", take_i8(code));
}
static void log_op_imm_i32(uint8_t **code, FILE *out, bool size_class) {
    if (size_class) fprintf(out, "imm: %d", take_i32(code));
    else fprintf(out, "imm: %d", take_i8(code));
}
static void log_op_imm_i64(uint8_t **code, FILE *out, bool size_class) {
    if (size_class) fprintf(out, "imm: %ld", take_i64(code));
    else fprintf(out, "imm: %d", take_i8(code));
}
static void log_op_imm_u8(uint8_t **code, FILE *out, bool flag) {
    fprintf(out, "imm: %ud", take_u8(code));
}
static void log_op_imm_u16(uint8_t **code, FILE *out, bool size_class) {
    if (size_class) fprintf(out, "imm: %ud", take_u16(code));
    else fprintf(out, "imm: %ud", take_u8(code));
}
static void log_op_imm_u32(uint8_t **code, FILE *out, bool size_class) {
    if (size_class) fprintf(out, "imm: %ud", take_u32(code));
    else fprintf(out, "imm: %ud", take_u8(code));
}
static void log_op_imm_u64(uint8_t **code, FILE *out, bool size_class) {
    if (size_class) fprintf(out, "imm: %lu", take_u64(code));
    else fprintf(out, "imm: %ud", take_u8(code));
}
static void log_op_imm_f32(uint8_t **code, FILE *out, bool flag) {
    fprintf(out, "imm: %f", take_f32(code));
}
static void log_op_imm_f64(uint8_t **code, FILE *out, bool flag) {
    fprintf(out, "imm: %f", take_f64(code));
}
static void log_op_imm_ptr(uint8_t **code, FILE *out, bool flag) {
    fprintf(out, "imm: %p", take_ptr(code));
}
static void log_op_alloc(uint8_t **code, FILE *out, bool size_class) {
    if (size_class) fprintf(out, "n: %ud", take_u32(code));
    else fprintf(out, "n: %ud", take_u8(code));
}
static void log_op_alloc_indirect(uint8_t **code, FILE *out, bool flag) {}
static void log_op_free_indirect(uint8_t **code, FILE *out, bool flag) {}

vm_opcode_t vm_opcode_log(const uint8_t **code, FILE *out) {
    const uint8_t byte = *(*code)++;
    const bool flag = byte & 0x80;
    const vm_opcode_t opcode = byte & 0x7f;

#define X(ENUM) \
case ENUM: \
    fprintf(out, #ENUM"("); \
    log_##ENUM((uint8_t **)code, out, flag); \
    fprintf(out, ")"); \
    return opcode;
    switch (opcode) {
    VM_OPCODES
    default: fprintf(out, "op_invalid()"); return op_illegal;
    }
#undef X
}

