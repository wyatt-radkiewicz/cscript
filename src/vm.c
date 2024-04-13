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
        .callstack_len = state->rp - state->callstack,
    };
}

static inline uint8_t take_u8(vm_state_t *state) {
    return *state->pc++;
}
static inline uint16_t take_u16(vm_state_t *state) {
    uint16_t x = *state->pc++;
    x |= *state->pc++ << 8;
    return x;
}
static inline uint32_t take_u32(vm_state_t *state) {
    uint32_t x = *state->pc++;
    x |= *state->pc++ << 8;
    x |= *state->pc++ << 16;
    x |= *state->pc++ << 24;
    return x;
}
static inline uint64_t take_u64(vm_state_t *state) {
    uint64_t x = *state->pc++;
    x |= (uint64_t)(*state->pc++) << 8;
    x |= (uint64_t)(*state->pc++) << 16;
    x |= (uint64_t)(*state->pc++) << 24;
    x |= (uint64_t)(*state->pc++) << 32;
    x |= (uint64_t)(*state->pc++) << 40;
    x |= (uint64_t)(*state->pc++) << 48;
    x |= (uint64_t)(*state->pc++) << 56;
    return x;
}
#define cast_take_template(NAME, TYPE, WIDTH) \
    static inline TYPE take_##NAME(vm_state_t *state) { \
        TYPE x; \
        *(uint##WIDTH##_t *)(&x) = take_u##WIDTH(state); \
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
    const uint32_t offs = size_class ? take_u32(state) : take_u8(state);
    const uint32_t n = size_class ? take_u32(state) : take_u8(state);
    vm_errout(offs + n > state->data_size, vm_err_segfault);
    vm_errout(state->sp - n < state->stack, vm_err_stack_overflow);
    state->sp -= n;
    memcpy(state->sp, state->data + offs, n);
    return true;
}
static inline bool run_op_store_data(vm_state_t *state, vm_error_t *err, bool size_class) {
    const uint32_t offs = size_class ? take_u32(state) : take_u8(state);
    const uint32_t n = size_class ? take_u32(state) : take_u8(state);
    vm_errout(offs + n > state->data_size, vm_err_segfault);
    vm_errout(state->sp + n > state->stack + state->stack_size, vm_err_stack_overrun);
    memcpy(state->data + offs, state->sp, n);
    return true;
}
static inline bool run_op_load_data_indirect(vm_state_t *state, vm_error_t *err, bool size_class) {
    const uint32_t n = size_class ? take_u32(state) : take_u8(state);
    uint32_t offs;
    vm_errout(!vm_state_pop_u32(state, &offs), vm_err_stack_underflow);
    vm_errout(state->sp - n < state->stack, vm_err_stack_overflow);
    vm_errout(offs + n > state->data_size, vm_err_segfault);
    state->sp -= n;
    memcpy(state->sp, state->data + offs, n);
    return true;
}
static inline bool run_op_store_data_indirect(vm_state_t *state, vm_error_t *err, bool size_class) {
    const uint32_t n = size_class ? take_u32(state) : take_u8(state);
    uint32_t offs;
    vm_errout(!vm_state_pop_u32(state, &offs), vm_err_stack_underflow);
    vm_errout(state->sp + n > state->stack + state->stack_size, vm_err_stack_overrun);
    vm_errout(offs + n > state->data_size, vm_err_segfault);
    memcpy(state->data + offs, state->sp, n);
    return true;
}
static inline bool run_op_load_indirect(vm_state_t *state, vm_error_t *err, bool size_class) {
    const uint32_t n = size_class ? take_u32(state) : take_u8(state);
    void *ptr;
    vm_errout(!vm_state_pop_ptr(state, &ptr), vm_err_stack_underflow);
    //vm_errout(state->sp + n > state->stack + state->stack_size, vm_err_stack_overrun);
    vm_errout(state->sp - n < state->stack, vm_err_stack_overflow);
    state->sp -= n;
    memcpy(state->sp, ptr, n);
    return true;
}
static inline bool run_op_store_indirect(vm_state_t *state, vm_error_t *err, bool size_class) {
    const uint32_t n = size_class ? take_u32(state) : take_u8(state);
    void *ptr;
    vm_errout(!vm_state_pop_ptr(state, &ptr), vm_err_stack_underflow);
    vm_errout(state->sp + n > state->stack + state->stack_size, vm_err_stack_overrun);
    memcpy(ptr, state->sp, n);
    return true;
}
static inline bool run_op_load_stack(vm_state_t *state, vm_error_t *err, bool size_class) {
    const int32_t offs = size_class ? take_i32(state) : take_i8(state);
    const uint32_t n = size_class ? take_u32(state) : take_u8(state);
    const uint32_t base = state->rp->stack_base;
    vm_errout(state->sp - n < state->stack, vm_err_stack_overflow);
    vm_errout((int64_t)base + offs + n > state->stack_size, vm_err_stack_overrun);
    vm_errout((int64_t)base + offs < 0, vm_err_stack_overrun);
    state->sp -= n;
    memcpy(state->sp, state->stack + (int64_t)base - offs, n);
    return true;
}
static inline bool run_op_store_stack(vm_state_t *state, vm_error_t *err, bool size_class) {
    const int32_t offs = size_class ? take_i32(state) : take_i8(state);
    const uint32_t n = size_class ? take_u32(state) : take_u8(state);
    const uint32_t base = state->rp->stack_base;
    vm_errout(state->sp + n > state->stack + state->stack_size, vm_err_stack_underflow);
    vm_errout((int64_t)base + offs + n > state->stack_size, vm_err_stack_overrun);
    vm_errout((int64_t)base + offs < 0, vm_err_stack_overrun);
    memcpy(state->stack + (int64_t)base - offs, state->sp, n);
    return true;
}
static inline bool run_op_load_stack_indirect(vm_state_t *state, vm_error_t *err, bool size_class) {
    const int32_t offs = size_class ? take_i32(state) : take_i8(state);
    const uint32_t n = size_class ? take_u32(state) : take_u8(state);
    const uint32_t base = state->rp->stack_base;
    int32_t base_offs;
    vm_errout(!vm_state_pop_i32(state, &base_offs), vm_err_stack_underflow);
    vm_errout(state->sp - n < state->stack, vm_err_stack_overflow);
    vm_errout((int64_t)base + offs + n > state->stack_size, vm_err_stack_overrun);
    vm_errout((int64_t)base + offs < 0, vm_err_stack_overrun);
    state->sp -= n;
    memcpy(state->sp, state->stack + (int64_t)base - base_offs + offs, n);
    return true;
}
static inline bool run_op_store_stack_indirect(vm_state_t *state, vm_error_t *err, bool size_class) {
    const int32_t offs = size_class ? take_i32(state) : take_i8(state);
    const uint32_t n = size_class ? take_u32(state) : take_u8(state);
    const uint32_t base = state->rp->stack_base;
    int32_t base_offs;
    vm_errout(!vm_state_pop_i32(state, &base_offs), vm_err_stack_underflow);
    vm_errout(state->sp + n > state->stack + state->stack_size, vm_err_stack_underflow);
    vm_errout((int64_t)base + offs + n > state->stack_size, vm_err_stack_overrun);
    vm_errout((int64_t)base + offs < 0, vm_err_stack_overrun);
    memcpy(state->stack + (int64_t)base - base_offs + offs, state->sp, n);
    return true;
}
static inline bool run_op_sub_stack(vm_state_t *state, vm_error_t *err, bool size_class) {
    uint32_t n = size_class ? take_u32(state) : take_u8(state);
    vm_errout(!vm_state_pop_u32(state, &n), vm_err_stack_underflow);
    vm_errout(state->sp - n < state->stack, vm_err_stack_overflow);
    state->sp -= n;
    return true;
}
static inline bool run_op_add_stack(vm_state_t *state, vm_error_t *err, bool size_class) {
    uint32_t n = size_class ? take_u32(state) : take_u8(state);
    vm_errout(!vm_state_pop_u32(state, &n), vm_err_stack_underflow);
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
    const uint8_t flags = take_u8(state); \
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
    const uint8_t flags = take_u8(state);
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
    const uint8_t flags = take_u8(state);
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
    const uint8_t size_class = take_u8(state);
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
    const uint8_t size_class = take_u8(state);
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
    const uint8_t flags = take_u8(state); \
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
    const uint8_t flags = take_u8(state); \
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
    vm_errout(state->callstack - 1 < state->callstack, vm_err_callstack_underflow);
    const vm_callstack_t x = *state->callstack--;
    if (x.ret_loc == UINT32_MAX) {
        return false;
    } else {
        state->pc = state->code + x.ret_loc;
        return true;
    }
}
static inline bool run_op_call(vm_state_t *state, vm_error_t *err, bool flag) {
    const uint32_t loc = take_u32(state);
    vm_errout(state->callstack + 1 >= state->callstack + state->callstack_size, vm_err_callstack_overflow);
    state->callstack++;
    state->callstack->ret_loc = state->pc - state->code;
    state->callstack->stack_base = state->sp - state->stack;
    state->pc = state->code + loc;
    return true;
}
static inline bool run_op_call_indirect(vm_state_t *state, vm_error_t *err, bool flag) {
    uint32_t loc;
    vm_errout(!vm_state_pop_u32(state, &loc), vm_err_callstack_underflow);
    vm_errout(state->callstack + 1 >= state->callstack + state->callstack_size, vm_err_callstack_overflow);
    state->callstack++;
    state->callstack->ret_loc = state->pc - state->code;
    state->callstack->stack_base = state->sp - state->stack;
    state->pc = state->code + loc;
    return true;
}
static inline bool run_op_extern_call(vm_state_t *state, vm_error_t *err, bool flag) {
    const uint32_t idx = take_u32(state);
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
    const int32_t offs = take_i32(state);
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
    const int32_t offs = take_i32(state);
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
    const int32_t offs = take_i32(state);
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
    int8_t imm = take_i8(state);
    vm_errout(!vm_state_push_i8(state, imm), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_imm_i16(vm_state_t *state, vm_error_t *err, bool flag) {
    int16_t imm = take_i16(state);
    vm_errout(!vm_state_push_i16(state, imm), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_imm_i32(vm_state_t *state, vm_error_t *err, bool flag) {
    int32_t imm = take_i32(state);
    vm_errout(!vm_state_push_i32(state, imm), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_imm_i64(vm_state_t *state, vm_error_t *err, bool flag) {
    int64_t imm = take_i64(state);
    vm_errout(!vm_state_push_i64(state, imm), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_imm_u8(vm_state_t *state, vm_error_t *err, bool flag) {
    uint8_t imm = take_u8(state);
    vm_errout(!vm_state_push_u8(state, imm), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_imm_u16(vm_state_t *state, vm_error_t *err, bool flag) {
    uint16_t imm = take_u16(state);
    vm_errout(!vm_state_push_u16(state, imm), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_imm_u32(vm_state_t *state, vm_error_t *err, bool flag) {
    uint32_t imm = take_u32(state);
    vm_errout(!vm_state_push_u32(state, imm), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_imm_u64(vm_state_t *state, vm_error_t *err, bool flag) {
    uint64_t imm = take_u64(state);
    vm_errout(!vm_state_push_u64(state, imm), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_imm_f32(vm_state_t *state, vm_error_t *err, bool flag) {
    float imm = take_f32(state);
    vm_errout(!vm_state_push_f32(state, imm), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_imm_f64(vm_state_t *state, vm_error_t *err, bool flag) {
    double imm = take_f64(state);
    vm_errout(!vm_state_push_f64(state, imm), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_imm_ptr(vm_state_t *state, vm_error_t *err, bool flag) {
    void *imm = take_ptr(state);
    vm_errout(!vm_state_push_ptr(state, imm), vm_err_stack_overflow);
    return true;
}
static inline bool run_op_alloc(vm_state_t *state, vm_error_t *err, bool size_class) {
    const uint32_t n = size_class ? take_u32(state) : take_u8(state);
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
vm_error_t vm_state_run(vm_state_t *state, uint32_t offs) {
    state->pc = state->code + offs;
    state->sp = state->stack + state->stack_size;
    state->rp = state->callstack;
    *state->rp = (vm_callstack_t){
        .ret_loc = UINT32_MAX,
        .stack_base = UINT32_MAX,
    };

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
const char *vm_error_code_to_str(vm_error_code_t code) {
    switch (code) {
    VM_ERROR_CODES
    }
}
#undef X
void vm_error_log(const vm_error_t *err, FILE *out) {
    fprintf(out, "vm_error_t {\n");
    fprintf(out, "\tprogram_counter: %ld,\n", err->program_counter);
    fprintf(out, "\tstack_pointer: %p,\n", err->stack_pointer);
    fprintf(out, "\tstack_size: %zu,\n", err->stack_size);
    fprintf(out, "}\n");

}
